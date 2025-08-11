// kernel/agent_loader.c — manifestless, libc-minimal loader
// - No snprintf/strcmp/strstr/memchr/strtol
// - No temporary entry registry; we spawn directly with the function pointer
// - Rebase ET_EXEC like PIE; apply RELATIVE relocs
// - Arena fallback; ultra-verbose breadcrumbs

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

extern int kprintf(const char *fmt, ...);

/* ---------- Tuning ---------- */
#ifndef LOADER_VERBOSE
#define LOADER_VERBOSE 1
#endif
#ifndef LOADER_ADD_SYMBOLS
#define LOADER_ADD_SYMBOLS 0
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

/* ---------- Tiny safe string helpers ---------- */
static size_t cstr_len(const char* s){ size_t n=0; if(!s) return 0; while(s[n]) n++; return n; }
static void cstr_copy(char* dst, size_t cap, const char* src){
	if(!dst||cap==0){ return; }
	size_t i=0; if(src){ while(src[i] && i+1<cap){ dst[i]=src[i]; i++; } }
	dst[i]=0;
}
static void cstr_append(char* dst, size_t cap, const char* src){
	if(!dst||cap==0||!src) return;
	size_t d=cstr_len(dst), i=0;
	while(src[i] && d+1<cap){ dst[d++]=src[i++]; }
	dst[d]=0;
}
static void path_basename_noext(const char* path, char* out, size_t cap){
	if(!out||cap==0){ return; }
	out[0]=0; if(!path){ return; }
	const char* b=path; for(const char* p=path; *p; ++p) if(*p=='/') b=p+1;
	size_t i=0; while(b[i] && i+1<cap){ out[i]=b[i]; i++; } out[i]=0;
	char* dot=NULL; for(char* p=out; *p; ++p) if(*p=='.') dot=p; if(dot) *dot=0;
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
                           const uint8_t* p, size_t before, size_t after){
	if(!p || !img_base || img_span==0) return;
	const uint8_t* img_end = img_base + img_span;
	const uint8_t* start   = (p > img_base + before) ? (p - before) : img_base;
	const uint8_t* endwant = p + after;
	const uint8_t* end     = (endwant < img_end) ? endwant : img_end;
	if(end <= start) return;
	for(const uint8_t* row = start; row < end; row += 16){
		size_t n = (size_t)((end - row) > 16 ? 16 : (end - row));
		kprintf("[dump] %016llx : ", (unsigned long long)(uintptr_t)row);
		for(size_t j=0;j<16;j++){ if(j<n) kprintf("%02x ", row[j]); else kprintf("   "); }
		kprintf(" |");
		for(size_t j=0;j<n;j++){ uint8_t c=row[j]; kprintf("%c", (c>=32 && c<127)?c:'.'); }
		kprintf("|\n");
	}
}

/* ---------- Minimal register + spawn (no lookups) ---------- */
typedef void (*agent_entry_t)(void);
static int register_and_spawn_direct_fn(const char* name,
                                        const char* version,
                                        const char* capabilities,
                                        agent_entry_t fn,
                                        const char* entry_name,
                                        const char* path,
                                        int prio)
{
	if(!fn){ kprintf("[loader] register_and_spawn: null fn\n"); return -1; }

	regx_manifest_t m; memset(&m,0,sizeof(m));
	cstr_copy(m.name, sizeof(m.name), name?name:"(unnamed)");
	m.type = 4;
	cstr_copy(m.version, sizeof(m.version), version?version:"0");
	cstr_copy(m.abi, sizeof(m.abi), "");
	cstr_copy(m.capabilities, sizeof(m.capabilities), capabilities?capabilities:"");

	if(g_gate_fn){
		int ok=g_gate_fn(path?path:"(buffer)", m.name, m.version, m.capabilities, entry_name?entry_name:"agent_main");
		if(!ok){ kprintf("[loader] gate denied %s (entry=%s)\n", m.name, entry_name?entry_name:"agent_main"); return -1; }
	}

	uint64_t id = regx_register(&m,0);
	if(!id){ kprintf("[loader] regx_register failed for %s\n", m.name); return -1; }

	n2_agent_t agent; memset(&agent,0,sizeof(agent));
	cstr_copy(agent.name, sizeof(agent.name), m.name);
	cstr_copy(agent.version, sizeof(agent.version), m.version);
	cstr_copy(agent.capabilities, sizeof(agent.capabilities), m.capabilities);
	agent.entry=fn;
	n2_agent_register(&agent);

	LOGV("[loader] spawning thread at %p for \"%s\"\n", (void*)fn, agent.name);
	thread_t* t=thread_create_with_priority((void(*)(void))fn, prio>0?prio:200);
	if(!t){ kprintf("[loader] thread create failed for %s\n", agent.name); return -1; }
	return 0;
}

/* ---------- Relocations (RELATIVE only) ---------- */
static size_t apply_relocations_rela_table(uint8_t* base, uint64_t lo, Elf64_Addr tab, Elf64_Addr tsz){
	if(!tab || !tsz) return 0;
	if (tab < lo) return 0;
	uint64_t off = tab - lo;
	Elf64_Rela* rela_tbl = (Elf64_Rela*)(base + off);
	size_t cnt = (size_t)(tsz / sizeof(Elf64_Rela)), applied=0;
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
static size_t apply_relocations_rela(uint8_t* base, uint64_t lo, const Elf64_Ehdr* eh, const void* img, size_t sz){
	(void)sz;
	const Elf64_Phdr* ph = (const Elf64_Phdr*)((const uint8_t*)img + eh->e_phoff);
	Elf64_Addr rela=0,rela_sz=0,rela_ent=sizeof(Elf64_Rela), jmprel=0,pltrel_sz=0;
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
	(void)rela_ent;
	return apply_relocations_rela_table(base, lo, rela, rela_sz)
	     + apply_relocations_rela_table(base, lo, jmprel, pltrel_sz);
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
		if(!base){ kprintf("[loader] arena OOM (%zu)\n", span); return -1; }
	}
	memset(base, 0, span);
	LOGV("[loader] load_base=%p\n", base);

	for(uint16_t i=0;i<eh->e_phnum;i++){
		if(ph[i].p_type!=PT_LOAD) continue;
		if(ph[i].p_offset + ph[i].p_filesz > sz){ kprintf("[loader] ELF bounds\n"); return -1; }
		uintptr_t dst = (uintptr_t)(base + (ph[i].p_vaddr - lo));
		LOGV("[loader] PT_LOAD[%u]: vaddr=0x%llx file=%llu mem=%llu fl=0x%x -> [%p..%p)\n",
		     (unsigned)i,(unsigned long long)ph[i].p_vaddr,
		     (unsigned long long)ph[i].p_filesz,(unsigned long long)ph[i].p_memsz,
		     (unsigned)ph[i].p_flags,(void*)dst,(void*)(dst + ph[i].p_memsz));
		memcpy((void*)dst, (const uint8_t*)img + ph[i].p_offset, ph[i].p_filesz);
	}

	size_t nrel = apply_relocations_rela(base, lo, eh, img, sz);
	LOGV("[loader] relocations: applied %zu R_X86_64_RELATIVE\n", nrel);

	/* Rebase entry for BOTH ET_DYN and ET_EXEC. */
	if (eh->e_entry < lo || eh->e_entry >= hi){
		kprintf("[loader] FATAL: e_entry (0x%llx) out of PT_LOAD span [0x%llx..%llx)\n",
		        (unsigned long long)eh->e_entry,(unsigned long long)lo,(unsigned long long)hi);
		return -1;
	}
	uintptr_t entry_addr = (uintptr_t)(base + (eh->e_entry - lo));
	if(eh->e_type == ET_DYN)
		LOGV("[loader] runtime entry (ET_DYN)  = %p (e_entry-lo=0x%llx)\n",(void*)entry_addr,(unsigned long long)(eh->e_entry - lo));
	else
		LOGV("[loader] runtime entry (ET_EXEC) = %p (e_entry-lo=0x%llx, base=%p)\n",(void*)entry_addr,(unsigned long long)(eh->e_entry - lo),base);

	/* Guard */
	if(entry_addr < (uintptr_t)base || entry_addr >= (uintptr_t)(base + span)){
		kprintf("[loader] FATAL: entry %p outside image [%p..%p)\n",(void*)entry_addr, base, base+span);
		return -1;
	}

	/* Entry inspection */
	{
		uint8_t *entry_ptr = (uint8_t*)entry_addr;
		uint32_t first4 = *(volatile uint32_t*)entry_ptr;
		if (first4 == 0xFA1E0FF3u) LOGV("[loader] entry starts with ENDBR64 (CET)\n");
		else LOGV("[loader] entry first bytes: %02x %02x %02x %02x\n", entry_ptr[0],entry_ptr[1],entry_ptr[2],entry_ptr[3]);
		LOGV("[loader] dumping 64B around entry %p\n", (void*)entry_addr);
		hexdump_window(base, span, entry_ptr, 32, 32);
	}

	/* Build minimal info for registry/gate */
	char agent_name[64]={0};
	path_basename_noext(path?path:"init", agent_name, sizeof(agent_name));
	if(!agent_name[0]) cstr_copy(agent_name,sizeof(agent_name),"init");

	const char* entry_name = "agent_main";
	LOGV("[loader] invoking register_and_spawn (name=\"%s\", entry=\"%s\")...\n", agent_name, entry_name);
	int rc = register_and_spawn_direct_fn(agent_name, "0", "", (agent_entry_t)(void*)entry_addr, entry_name, path, prio);
	LOGV("[loader] register_and_spawn rc=%d\n", rc);
	if (rc != 0) return rc;

	/* Optional: add symbol region AFTER successful spawn (guarded) */
#if LOADER_ADD_SYMBOLS
	{
		char symname[128]={0};
		cstr_copy(symname,sizeof(symname), agent_name);
		cstr_append(symname,sizeof(symname), ":");
		cstr_append(symname,sizeof(symname), entry_name);
		LOGV("[loader] symbols_add %s base=%p span=%zu\n", symname, base, span);
		symbols_add(symname, (uintptr_t)base, span);
	}
#endif
	return 0;
}

/* ---------- MACHO2 wrapper (embedded ELF) ---------- */
static int load_agent_macho2_impl(const void* img, size_t sz, const char* path, int prio){
	LOGV("[loader] MACHO2 wrapper for %s\n", path?path:"(buffer)");
	/* Look for ELF magic without memmem */
	const uint8_t* d=(const uint8_t*)img;
	size_t i=0; for(; i+4<=sz; i++){ if(d[i]==0x7F && d[i+1]=='E' && d[i+2]=='L' && d[i+3]=='F') break; }
	if(i+4<=sz){
		size_t remain = sz - i;
		LOGV("[loader] embedded ELF detected at +0x%zx (remain=%zu)\n", (size_t)i, remain);
		return load_agent_elf_impl(d+i, remain, path, prio);
	}
	return -1; /* stay safe */
}

/* ---------- FLAT loader ---------- */
static int load_agent_flat_impl(const void* img, size_t sz, const char* path, int prio){
	if(!img||!sz) return -1;
	char nm[64]={0}; path_basename_noext(path?path:"flat", nm, sizeof(nm));
	const char* entry_name="agent_main";
	LOGV("[loader] FLAT image %s at %p (%zu bytes)\n", nm[0]?nm:"(flat)", img, sz);
	return register_and_spawn_direct_fn(nm[0]?nm:"flat", "0", "", (agent_entry_t)img, entry_name, path, prio);
}

/* ---------- Public API ---------- */
static agent_format_t detect_fmt(const void* img, size_t sz){
	const uint8_t* d=(const uint8_t*)img;
	if(sz>=4 && d[0]==0x7F && d[1]=='E' && d[2]=='L' && d[3]=='F') return AGENT_FORMAT_ELF;
	if(sz>=4 && (d[0]==0xCF&&d[1]==0xFA&&d[2]==0xED&&d[3]==0xFE)) return AGENT_FORMAT_MACHO2;
	if(sz>=4 && d[0]=='N' && d[1]=='O' && d[2]=='S' && d[3]=='M') return AGENT_FORMAT_NOSM;
	if(sz>0 && ((const char*)d)[0]=='{') return AGENT_FORMAT_MACHO2; /* historical wrapper */
	return AGENT_FORMAT_FLAT;
}

int load_agent_auto_with_prio(const void* img, size_t sz, int prio){
	agent_format_t f = detect_fmt(img,sz);
	LOGV("[loader] auto fmt=%d size=%zu\n", (int)f, sz);
	switch(f){
		case AGENT_FORMAT_ELF:    return load_agent_elf_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_MACHO2: return load_agent_macho2_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_FLAT:   return load_agent_flat_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_NOSM:   return -1;
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
int load_agent_auto(const void* img, size_t sz){ return load_agent_auto_with_prio(img,sz,200); }
int load_agent(const void* img, size_t sz, agent_format_t f){ return load_agent_with_prio(img,sz,f,200); }

int agent_loader_run_from_path(const char* path, int prio){
	if(!g_read_file){ kprintf("[loader] no FS reader for %s\n", path?path:"(null)"); return -1; }
	void* buf=0; size_t sz=0;
	int rc=g_read_file(path,&buf,&sz);
	if(rc<0){ kprintf("[loader] read failed: %s\n", path?path:"(null)"); return rc; }
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
