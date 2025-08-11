// kernel/agent_loader.c — ultra-verbose PIE/ET_EXEC loader (pure C)
// - Rebase entry for BOTH ET_DYN and ET_EXEC (we always map at a fresh base)
// - Applies R_X86_64_RELATIVE relocations
// - 1 MiB arena fallback if kalloc() fails (so agents still run)
// - Very chatty logging around mapping, relocations, and entry bytes
// Toggle verbosity: set LOADER_VERBOSE to 0/1 below

#include "agent_loader.h"
#include "Task/thread.h"
#include "VM/kheap.h"
#include "symbols.h"
#include "agent.h"
#include "regx.h"

#include <elf.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>   // snprintf

extern int kprintf(const char *fmt, ...);

/* ---------- Tuning ---------- */
#ifndef LOADER_VERBOSE
#define LOADER_VERBOSE 1
#endif
#if LOADER_VERBOSE
  #define LOGV(...) kprintf(__VA_ARGS__)
#else
  #define LOGV(...) do{}while(0)
#endif

/* ---------- Hooks wired elsewhere ---------- */
static agent_read_file_fn g_read_file = 0;
static agent_free_fn      g_free_fn   = 0;
static agent_gate_fn      g_gate_fn   = 0;

void agent_loader_set_read(agent_read_file_fn r, agent_free_fn f){ g_read_file=r; g_free_fn=f; }
void agent_loader_set_gate(agent_gate_fn g){ g_gate_fn=g; }

/* ---------- Small utils ---------- */
static const void* memmem_local(const void* hay, size_t haylen, const void* nee, size_t neelen){
	if(!hay || !nee || neelen==0 || haylen<neelen) return NULL;
	const uint8_t *h=(const uint8_t*)hay, *n=(const uint8_t*)nee;
	for(size_t i=0;i+neelen<=haylen;i++){
		if(memcmp(h+i,n,neelen)==0) return h+i;
	}
	return NULL;
}
static const char* skip_ws(const char* p){ while(p && (*p==' '||*p=='\t'||*p=='\n'||*p=='\r')) p++; return p; }

static int json_get_str(const char* json, const char* key, char* out, size_t osz){
	if(!json || !key || !out || osz==0) return -1;
	char pat[64];
	snprintf(pat,sizeof(pat),"\"%s\"",key);
	const char* p = strstr(json, pat);
	if(!p) return -1;
	p += strlen(pat);
	p = skip_ws(p);
	if(*p!=':') return -1;
	p++;
	p = skip_ws(p);
	if(*p!='\"') return -1;
	p++;
	size_t i=0;
	while(*p && *p!='\"' && i<osz-1){
		out[i++] = *p++;
	}
	if(*p!='\"') return -1;
	out[i]=0;
	return 0;
}

static int json_get_int(const char* json, const char* key){
	if(!json || !key) return -1;
	char pat[64];
	snprintf(pat,sizeof(pat),"\"%s\"",key);
	const char* p = strstr(json, pat);
	if(!p) return -1;
	p += strlen(pat);
	p = skip_ws(p);
	if(*p!=':') return -1;
	p++;
	p = skip_ws(p);
	return (int)strtol(p, NULL, 10);
}

static void basename_noext(const char* path, char* out, size_t osz){
	if(!out || osz==0){ return; }
	out[0]=0;
	if(!path){ return; }
	const char* b=path;
	for(const char* p=path; *p; ++p) if(*p=='/') b=p+1;
	snprintf(out,osz,"%s",b);
	char* dot=NULL;
	for(char* p=out; *p; ++p) if(*p=='.') dot=p;
	if(dot) *dot=0;
}

static int extract_manifest_blob(const void* buf, size_t sz, char* out, size_t osz){
	if(!buf || !sz || !out || !osz) return -1;
	const char* s=(const char*)memchr(buf,'{',sz);
	if(!s) return -1;
	size_t remain = sz - (size_t)(s - (const char*)buf);
	const char* e=(const char*)memchr(s,'}',remain);
	if(!e || e<=s) return -1;
	size_t len=(size_t)(e-s+1);
	if(len+1>osz) return -1;
	memcpy(out,s,len);
	out[len]=0;
	return 0;
}

/* ---------- Entry registry (name -> fn*) ---------- */
typedef void (*agent_entry_t)(void);
#define MAX_ENTRIES 64
static struct { char name[64]; agent_entry_t fn; } g_entries[MAX_ENTRIES];
static size_t g_ec=0;

static void reg_entry(const char* name, agent_entry_t fn){
	if(!name||!fn) return;
	for(size_t i=0;i<g_ec;i++){
		if(strcmp(g_entries[i].name,name)==0){ g_entries[i].fn=fn; return; }
	}
	if(g_ec<MAX_ENTRIES){
		snprintf(g_entries[g_ec].name,sizeof(g_entries[g_ec].name),"%s",name);
		g_entries[g_ec].fn=fn; g_ec++;
	}
}
static agent_entry_t find_entry(const char* name){
	for(size_t i=0;i<g_ec;i++) if(strcmp(g_entries[i].name,name)==0) return g_entries[i].fn;
	return NULL;
}

/* ---------- Arena (OOM fallback) ---------- */
#define AGENT_ARENA_SIZE (1<<20) /* 1 MiB */
static uint8_t g_agent_arena[AGENT_ARENA_SIZE];
static size_t  g_agent_bump = 0;

static void* arena_alloc(size_t sz){
	const size_t align = 0x1000;
	size_t off = (g_agent_bump + (align-1)) & ~(align-1);
	if (off + sz > AGENT_ARENA_SIZE) return NULL;
	void* p = g_agent_arena + off; g_agent_bump = off + sz; return p;
}

/* ---------- Hex dump (entry window, clamped to image) ---------- */
static void hexdump_window(const uint8_t* img_base, size_t img_span,
                           const uint8_t* p, size_t before, size_t after)
{
	if(!p || !img_base || img_span==0) return;
	const uint8_t* img_end = img_base + img_span;
	const uint8_t* start   = p > img_base + before ? p - before : img_base;
	const uint8_t* endwant = p + after;
	const uint8_t* end     = endwant < img_end ? endwant : img_end;
	if(end <= start) return;

	for(const uint8_t* row = start; row < end; row += 16){
		size_t n = (size_t)((end - row) > 16 ? 16 : (end - row));
		kprintf("[dump] %016llx : ", (unsigned long long)(uintptr_t)row);
		for(size_t j=0;j<16;j++){
			if(j<n) kprintf("%02x ", row[j]); else kprintf("   ");
		}
		kprintf(" |");
		for(size_t j=0;j<n;j++){
			uint8_t c = row[j];
			kprintf("%c", (c>=32 && c<127)?c:'.');
		}
		kprintf("|\n");
	}
}

/* ---------- Gate + registry + spawn ---------- */
static int register_and_spawn_from_manifest(const char* json, const char* path, int prio){
	regx_manifest_t m; memset(&m,0,sizeof(m));
	char entry[64]={0};

	(void)path;

	json_get_str(json,"name",m.name,sizeof(m.name));
	m.type = json_get_int(json,"type");
	json_get_str(json,"version",m.version,sizeof(m.version));
	json_get_str(json,"abi",m.abi,sizeof(m.abi));
	json_get_str(json,"capabilities",m.capabilities,sizeof(m.capabilities));
	json_get_str(json,"entry",entry,sizeof(entry));
	if(!entry[0]) snprintf(entry,sizeof(entry),"agent_main");

	LOGV("[loader] reg+spawn name=\"%s\" entry=\"%s\" caps=\"%s\" prio=%d\n",
	     m.name[0]?m.name:"(unnamed)", entry, m.capabilities, prio);

	if(g_gate_fn){
		int ok=g_gate_fn(path?path:"(buffer)", m.name, m.version, m.capabilities, entry);
		if(!ok){
			kprintf("[loader] gate denied %s (entry=%s)\n", m.name[0]?m.name:"(unnamed)", entry);
			return -1;
		}
	}

	uint64_t id = regx_register(&m,0);
	if(!id){
		kprintf("[loader] regx_register failed for %s\n", m.name[0]?m.name:"(unnamed)");
		return -1;
	}

	agent_entry_t fn=find_entry(entry);
	if(!fn){
		kprintf("[loader] entry not resolved: %s\n", entry);
		return -1;
	}

	n2_agent_t agent; memset(&agent,0,sizeof(agent));
	snprintf(agent.name,sizeof(agent.name),"%s", m.name[0]?m.name:"(unnamed)");
	snprintf(agent.version,sizeof(agent.version),"%s", m.version);
	snprintf(agent.capabilities,sizeof(agent.capabilities),"%s", m.capabilities);
	agent.entry=fn;
	n2_agent_register(&agent);

	LOGV("[loader] spawning thread at %p for \"%s\"\n", (void*)fn, agent.name);
	thread_t* t=thread_create_with_priority((void(*)(void))fn, prio>0?prio:200);
	if(!t){
		kprintf("[loader] thread create failed for %s\n", agent.name);
		return -1;
	}
	return 0;
}

/* ---------- Relocations (RELATIVE only) ---------- */
static size_t apply_relocations_rela_table(uint8_t* base, uint64_t lo,
                                           Elf64_Addr tab, Elf64_Addr tsz)
{
	if(!tab || !tsz) return 0;
	if (tab < lo) return 0;
	uint64_t off = tab - lo;
	Elf64_Rela* rela_tbl = (Elf64_Rela*)(base + off);
	size_t cnt = (size_t)(tsz / sizeof(Elf64_Rela));
	size_t applied = 0;

	for(size_t i=0;i<cnt;i++){
		uint32_t type = (uint32_t)ELF64_R_TYPE(rela_tbl[i].r_info);
		if(type == R_X86_64_RELATIVE){
			uint64_t tgt_off = rela_tbl[i].r_offset - lo;
			uint64_t *where = (uint64_t*)(base + tgt_off);
			*where = (uint64_t)(base + rela_tbl[i].r_addend);
			applied++;
		}
	}
	return applied;
}

static size_t apply_relocations_rela(uint8_t* base, uint64_t lo,
                                     const Elf64_Ehdr* eh, const void* img, size_t sz)
{
	(void)sz;
	const Elf64_Phdr* ph = (const Elf64_Phdr*)((const uint8_t*)img + eh->e_phoff);
	Elf64_Addr rela   = 0, rela_sz = 0, rela_ent = sizeof(Elf64_Rela);
	Elf64_Addr jmprel = 0, pltrel_sz = 0;

	for(uint16_t i=0;i<eh->e_phnum;i++){
		if(ph[i].p_type != PT_DYNAMIC) continue;
		const Elf64_Dyn* dyn = (const Elf64_Dyn*)((const uint8_t*)img + ph[i].p_offset);
		size_t n = ph[i].p_filesz / sizeof(Elf64_Dyn);
		for(size_t j=0;j<n;j++){
			switch(dyn[j].d_tag){
				case DT_RELA:    rela      = dyn[j].d_un.d_ptr; break;
				case DT_RELASZ:  rela_sz   = dyn[j].d_un.d_val; break;
				case DT_RELAENT: rela_ent  = dyn[j].d_un.d_val; break;
				case DT_JMPREL:  jmprel    = dyn[j].d_un.d_ptr; break;
				case DT_PLTRELSZ:pltrel_sz = dyn[j].d_un.d_val; break;
				default: break;
			}
		}
	}

	(void)rela_ent; // not needed; we assume sizeof(Elf64_Rela)

	size_t a = 0;
	a += apply_relocations_rela_table(base, lo, rela,     rela_sz);
	a += apply_relocations_rela_table(base, lo, jmprel,   pltrel_sz);
	return a;
}

/* ---------- ELF loader ---------- */
static int load_agent_elf_impl(const void* img, size_t sz, const char* path, int prio){
	if(!img || sz < sizeof(Elf64_Ehdr)) return -1;

	const Elf64_Ehdr* eh=(const Elf64_Ehdr*)img;
	if(memcmp(eh->e_ident, ELFMAG, SELFMAG)!=0 || eh->e_ident[EI_CLASS]!=ELFCLASS64) return -1;
	if(eh->e_phoff==0 || eh->e_phnum==0) return -1;

	LOGV("[loader] ELF: type=%u phnum=%u e_entry=0x%llx\n",
	     (unsigned)eh->e_type, (unsigned)eh->e_phnum, (unsigned long long)eh->e_entry);

	const Elf64_Phdr* ph = (const Elf64_Phdr*)((const uint8_t*)img + eh->e_phoff);

	uint64_t lo=UINT64_MAX, hi=0;
	for(uint16_t i=0;i<eh->e_phnum;i++){
		if(ph[i].p_type!=PT_LOAD) continue;
		if(ph[i].p_vaddr < lo) lo = ph[i].p_vaddr;
		uint64_t end = ph[i].p_vaddr + ph[i].p_memsz;
		if(end > hi) hi = end;
	}
	if(lo==UINT64_MAX || hi<=lo) return -1;

	size_t span = (size_t)(hi - lo);
	LOGV("[loader] span: lo=0x%llx hi=0x%llx span=%zu\n",
	     (unsigned long long)lo, (unsigned long long)hi, span);

	uint8_t* base = (uint8_t*)kalloc(span);
	if(!base){
		kprintf("[loader] kalloc %zu failed, trying arena\n", span);
		base = (uint8_t*)arena_alloc(span);
		if(!base){
			kprintf("[loader] arena OOM (%zu)\n", span);
			return -1;
		}
	}
	memset(base, 0, span);
	LOGV("[loader] load_base=%p\n", base);

	for(uint16_t i=0;i<eh->e_phnum;i++){
		if(ph[i].p_type!=PT_LOAD) continue;
		if(ph[i].p_offset + ph[i].p_filesz > sz){
			kprintf("[loader] ELF bounds\n");
			return -1;
		}
		uintptr_t dst = (uintptr_t)(base + (ph[i].p_vaddr - lo));
		LOGV("[loader] PT_LOAD[%u]: vaddr=0x%llx file=%llu mem=%llu fl=0x%x -> [%p..%p)\n",
		     (unsigned)i,
		     (unsigned long long)ph[i].p_vaddr,
		     (unsigned long long)ph[i].p_filesz,
		     (unsigned long long)ph[i].p_memsz,
		     (unsigned)ph[i].p_flags,
		     (void*)dst, (void*)(dst + ph[i].p_memsz));
		memcpy((void*)dst, (const uint8_t*)img + ph[i].p_offset, ph[i].p_filesz);
	}

	size_t nrel = apply_relocations_rela(base, lo, eh, img, sz);
	LOGV("[loader] relocations: applied %zu R_X86_64_RELATIVE\n", nrel);

	/* Rebase entry for BOTH ET_DYN and ET_EXEC (we map at a fresh base). */
	if (eh->e_entry < lo || eh->e_entry >= hi) {
		kprintf("[loader] FATAL: e_entry (0x%llx) out of PT_LOAD span [0x%llx..0x%llx)\n",
		        (unsigned long long)eh->e_entry,
		        (unsigned long long)lo, (unsigned long long)hi);
		return -1;
	}
	uintptr_t entry_addr = (uintptr_t)(base + (eh->e_entry - lo));
	if(eh->e_type == ET_DYN){
		LOGV("[loader] runtime entry (ET_DYN)  = %p (e_entry-lo=0x%llx)\n",
		     (void*)entry_addr, (unsigned long long)(eh->e_entry - lo));
	}else{
		LOGV("[loader] runtime entry (ET_EXEC) = %p (e_entry-lo=0x%llx, base=%p)\n",
		     (void*)entry_addr, (unsigned long long)(eh->e_entry - lo), base);
	}

	/* Guard: entry must be inside mapped image */
	if(entry_addr < (uintptr_t)base || entry_addr >= (uintptr_t)(base + span)){
		kprintf("[loader] FATAL: entry %p outside image [%p..%p)\n",
		        (void*)entry_addr, base, base+span);
		return -1;
	}

	/* Entry inspection */
	{
		uint8_t *entry_ptr = (uint8_t*)entry_addr;
		uint32_t first4 = *(volatile uint32_t*)entry_ptr;
		if (first4 == 0xFA1E0FF3u){
			LOGV("[loader] entry starts with ENDBR64 (CET)\n");
		}else{
			LOGV("[loader] entry first bytes: %02x %02x %02x %02x\n",
			     entry_ptr[0], entry_ptr[1], entry_ptr[2], entry_ptr[3]);
		}
		LOGV("[loader] dumping 64B around entry %p\n", (void*)entry_addr);
		hexdump_window(base, span, entry_ptr, 32, 32);
	}

	/* Manifest */
	char manifest[512]={0};
	if(extract_manifest_blob(img, sz, manifest, sizeof(manifest))!=0){
		char nm[64]={0}; basename_noext(path?path:"init", nm, sizeof(nm));
		snprintf(manifest,sizeof(manifest),
		         "{\"name\":\"%s\",\"type\":4,\"version\":\"0\",\"entry\":\"agent_main\",\"capabilities\":\"\"}",
		         nm[0]?nm:"init");
	}else{
		LOGV("[loader] manifest found for %s\n", path?path:"(buffer)");
	}

	char entry_name[64]={0};
	if(json_get_str(manifest,"entry",entry_name,sizeof(entry_name))!=0)
		snprintf(entry_name,sizeof(entry_name),"agent_main");

	/* Register concrete pointer and symbol region */
	reg_entry(entry_name, (agent_entry_t)(void*)entry_addr);
	char symname[96]; char nm2[64]={0}; basename_noext(path?path:"elf", nm2, sizeof(nm2));
	snprintf(symname,sizeof(symname),"%s:%s", nm2[0]?nm2:"elf", entry_name);
	symbols_add(symname, (uintptr_t)base, span);

	return register_and_spawn_from_manifest(manifest, path, prio);
}

/* ---------- MACHO2 wrapper (looks for embedded ELF) ---------- */
static int load_agent_macho2_impl(const void* img, size_t sz, const char* path, int prio){
	LOGV("[loader] MACHO2 wrapper for %s\n", path?path:"(buffer)");
	const uint8_t* p = (const uint8_t*)memmem_local(img, sz, "\x7F""ELF", 4);
	if(p){
		size_t remain = sz - (size_t)(p - (const uint8_t*)img);
		LOGV("[loader] embedded ELF detected at +0x%zx (remain=%zu)\n",
		     (size_t)(p - (const uint8_t*)img), remain);
		return load_agent_elf_impl(p, remain, path, prio);
	}
	char manifest[512]={0};
	if(extract_manifest_blob(img, sz, manifest, sizeof(manifest))!=0){
		kprintf("[loader] MACHO2 without ELF and no manifest\n");
		return -1;
	}
	return register_and_spawn_from_manifest(manifest, path, prio);
}

/* ---------- FLAT loader ---------- */
static int load_agent_flat_impl(const void* img, size_t sz, const char* path, int prio){
	if(!img||!sz) return -1;
	char nm[64]={0}; basename_noext(path?path:"flat", nm, sizeof(nm));
	const char* entry="agent_main";
	LOGV("[loader] FLAT image %s at %p (%zu bytes)\n", nm[0]?nm:"(flat)", img, sz);
	reg_entry(entry, (agent_entry_t)img);
	char manifest[256];
	snprintf(manifest,sizeof(manifest),
	         "{\"name\":\"%s\",\"type\":4,\"version\":\"0\",\"entry\":\"%s\",\"capabilities\":\"\"}",
	         nm[0]?nm:"flat", entry);
	return register_and_spawn_from_manifest(manifest, path, prio);
}

/* ---------- Public API ---------- */
/* Use the enum + prototypes from agent_loader.h (no redefinitions here) */

static agent_format_t detect_fmt(const void* img, size_t sz){
	const uint8_t* d=(const uint8_t*)img;
	if(sz>=4 && memcmp(d,"\x7F""ELF",4)==0) return AGENT_FORMAT_ELF;
	if(sz>=4 && (d[0]==0xCF&&d[1]==0xFA&&d[2]==0xED&&d[3]==0xFE)) return AGENT_FORMAT_MACHO2;
	if(sz>=4 && memcmp(d,"NOSM",4)==0) return AGENT_FORMAT_NOSM;
	if(sz>0 && ((const char*)d)[0]=='{') return AGENT_FORMAT_MACHO2;
	return AGENT_FORMAT_FLAT;
}

int load_agent_auto_with_prio(const void* img, size_t sz, int prio){
	agent_format_t f = detect_fmt(img,sz);
	LOGV("[loader] auto fmt=%d size=%zu\n", (int)f, sz);
	switch(f){
		case AGENT_FORMAT_ELF:    return load_agent_elf_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_MACHO2: return load_agent_macho2_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_FLAT:   return load_agent_flat_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_NOSM:   /* handled elsewhere */ return -1;
		default: return -1;
	}
}

int load_agent_with_prio(const void* img, size_t sz, agent_format_t f, int prio){
	LOGV("[loader] fmt=%d size=%zu path=\"(buffer)\" prio=%d\n", (int)f, sz, prio);
	switch(f){
		case AGENT_FORMAT_ELF:    return load_agent_elf_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_MACHO2: return load_agent_macho2_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_FLAT:   return load_agent_flat_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_NOSM:   return -1;
		default: return load_agent_auto_with_prio(img,sz,prio);
	}
}

int load_agent_auto(const void* img, size_t sz){
	return load_agent_auto_with_prio(img,sz,200);
}

int load_agent(const void* img, size_t sz, agent_format_t f){
	return load_agent_with_prio(img,sz,f,200);
}

int agent_loader_run_from_path(const char* path, int prio){
	if(!g_read_file){
		kprintf("[loader] no FS reader for %s\n", path?path:"(null)");
		return -1;
	}
	void* buf=0; size_t sz=0;
	int rc=g_read_file(path,&buf,&sz);
	if(rc<0){
		kprintf("[loader] read failed: %s\n", path?path:"(null)");
		return rc;
	}
	LOGV("[loader] run_from_path \"%s\" size=%zu\n", path?path:"(null)", sz);

	int out = -1;
	switch(detect_fmt(buf,sz)){
		case AGENT_FORMAT_ELF:    out=load_agent_elf_impl(buf,sz,path,prio); break;
		case AGENT_FORMAT_MACHO2: out=load_agent_macho2_impl(buf,sz,path,prio); break;
		case AGENT_FORMAT_FLAT:   out=load_agent_flat_impl(buf,sz,path,prio); break;
		case AGENT_FORMAT_NOSM:   out=-1; break;
		default: out=-1; break;
	}
	if(g_free_fn && buf) g_free_fn(buf);
	return out;
}
