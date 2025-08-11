// kernel/agent_loader.c — fixed PIE entry handling (ET_DYN)
// Maps PT_LOAD, computes entry = load_base + e_entry, registers & spawns.

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

/* ------------------- Gate / FS hooks provided elsewhere ------------------- */
static agent_read_file_fn g_read_file = 0;
static agent_free_fn      g_free_fn   = 0;
static agent_gate_fn      g_gate_fn   = 0;

void agent_loader_set_read(agent_read_file_fn r, agent_free_fn f){ g_read_file=r; g_free_fn=f; }
void agent_loader_set_gate(agent_gate_fn g){ g_gate_fn=g; }

/* --------------------------- Tiny utilities ------------------------------- */

static const void* memmem_local(const void* h, size_t hl, const void* n, size_t nl){
	if(!h || !n || nl==0 || hl<nl) return NULL;
	const uint8_t* H=(const uint8_t*)h, *N=(const uint8_t*)n;
	for(size_t i=0;i+nl<=hl;i++) if(!memcmp(H+i,N,nl)) return H+i;
	return NULL;
}

static const char* skip_ws(const char* p){ while(p && (*p==' '||*p=='\t'||*p=='\n'||*p=='\r')) p++; return p; }

static int json_get_str(const char* json, const char* key, char* out, size_t osz){
	if(!json||!key||!out||!osz) return -1;
	char pat[64]; snprintf(pat,sizeof(pat),"\"%s\"",key);
	const char* p=strstr(json,pat); if(!p) return -1;
	p += strlen(pat); p = skip_ws(p); if(*p!=':') return -1; p++; p=skip_ws(p);
	if(*p!='"') return -1; p++; size_t i=0; while(*p && *p!='"' && i<osz-1) out[i++]=*p++; if(*p!='"') return -1;
	out[i]=0; return 0;
}

static int json_get_int(const char* json, const char* key){
	if(!json||!key) return -1;
	char pat[64]; snprintf(pat,sizeof(pat),"\"%s\"",key);
	const char* p=strstr(json,pat); if(!p) return -1;
	p += strlen(pat); p = skip_ws(p); if(*p!=':') return -1; p++; p=skip_ws(p);
	return (int)strtol(p,NULL,10);
}

static void basename_noext(const char* path, char* out, size_t osz){
	if(!out||!osz){return;} out[0]=0; if(!path){return;}
	const char* b=path; for(const char* p=path; *p; ++p) if(*p=='/') b=p+1;
	snprintf(out,osz,"%s",b); char* dot=NULL; for(char* p=out; *p; ++p) if(*p=='.') dot=p; if(dot) *dot=0;
}

/* --------------------- Entry registry (name -> fn*) ----------------------- */

typedef void (*agent_entry_t)(void);
#define MAX_ENTRIES 64
static struct { char name[64]; agent_entry_t fn; } g_entries[MAX_ENTRIES];
static size_t g_ec=0;

static void reg_entry(const char* name, agent_entry_t fn){
	if(!name||!fn) return;
	for(size_t i=0;i<g_ec;i++) if(strcmp(g_entries[i].name,name)==0){ g_entries[i].fn=fn; return; }
	if(g_ec<MAX_ENTRIES){ snprintf(g_entries[g_ec].name,sizeof(g_entries[g_ec].name),"%s",name); g_entries[g_ec].fn=fn; g_ec++; }
}

static agent_entry_t find_entry(const char* name){
	for(size_t i=0;i<g_ec;i++) if(strcmp(g_entries[i].name,name)==0) return g_entries[i].fn;
	return NULL;
}

/* ---------------------- Manifest extraction helpers ----------------------- */

static int extract_manifest_blob(const void* buf, size_t sz, char* out, size_t osz){
	if(!buf||!sz||!out||!osz) return -1;
	/* Heuristic: find first '{' then the next '}' (manifests are tiny) */
	const char* s=(const char*)memchr(buf,'{',sz);
	if(!s) return -1;
	size_t remain = sz - (size_t)(s - (const char*)buf);
	const char* e=(const char*)memchr(s,'}',remain);
	if(!e || e<=s) return -1;
	size_t len=(size_t)(e-s+1); if(len+1>osz) return -1;
	memcpy(out,s,len); out[len]=0; return 0;
}

/* --------------------- Gate + registry + spawning ------------------------- */

static int register_and_spawn_from_manifest(const char* json, const char* path, int prio){
	regx_manifest_t m=(regx_manifest_t){0};
	char entry[64]={0};

	json_get_str(json,"name",m.name,sizeof(m.name));
	m.type = json_get_int(json,"type");
	json_get_str(json,"version",m.version,sizeof(m.version));
	json_get_str(json,"abi",m.abi,sizeof(m.abi));
	json_get_str(json,"capabilities",m.capabilities,sizeof(m.capabilities));
	json_get_str(json,"entry",entry,sizeof(entry));
	if(!entry[0]) snprintf(entry,sizeof(entry),"agent_main");

	if(g_gate_fn){
		int ok=g_gate_fn(path?path:"(buffer)", m.name, m.version, m.capabilities, entry);
		if(!ok){ kprintf("[loader] gate denied %s (entry=%s)\n", m.name, entry); return -1; }
	}

	uint64_t id = regx_register(&m,0);
	if(!id){ kprintf("[loader] regx_register failed for %s\n", m.name[0]?m.name:"(unnamed)"); return -1; }

	agent_entry_t fn=find_entry(entry);
	if(!fn){ kprintf("[loader] entry not resolved: %s\n", entry); return -1; }

	n2_agent_t agent=(n2_agent_t){0};
	snprintf(agent.name,sizeof(agent.name),"%s", m.name[0]?m.name:"(unnamed)");
	snprintf(agent.version,sizeof(agent.version),"%s", m.version);
	snprintf(agent.capabilities,sizeof(agent.capabilities),"%s", m.capabilities);
	agent.entry=fn;
	n2_agent_register(&agent);

	thread_t* t=thread_create_with_priority((void(*)(void))fn, prio>0?prio:200);
	if(!t){ kprintf("[loader] thread create failed for %s\n", agent.name); return -1; }
	return 0;
}

/* ------------------------------ ELF loader -------------------------------- */

static int load_agent_elf_impl(const void* img, size_t sz, const char* path, int prio){
	if(!img || sz < sizeof(Elf64_Ehdr)) return -1;

	const Elf64_Ehdr* eh=(const Elf64_Ehdr*)img;
	if(memcmp(eh->e_ident, ELFMAG, SELFMAG)!=0 || eh->e_ident[EI_CLASS]!=ELFCLASS64) return -1;
	if(eh->e_phoff==0 || eh->e_phnum==0) return -1;

	const Elf64_Phdr* ph = (const Elf64_Phdr*)((const uint8_t*)img + eh->e_phoff);

	/* Compute segment span and choose a load base (page aligned). */
	uint64_t lo=UINT64_MAX, hi=0;
	for(uint16_t i=0;i<eh->e_phnum;i++){
		if(ph[i].p_type!=PT_LOAD) continue;
		if(ph[i].p_vaddr < lo) lo = ph[i].p_vaddr;
		uint64_t end = ph[i].p_vaddr + ph[i].p_memsz;
		if(end > hi) hi = end;
	}
	if(lo==UINT64_MAX || hi<=lo) return -1;

	size_t span = (size_t)(hi - lo);
	uint8_t* base = (uint8_t*)kalloc(span);
	if(!base){ kprintf("[loader] kalloc %zu failed\n", span); return -1; }
	memset(base, 0, span);

	/* Map PT_LOADs: dst = base + (p_vaddr - lo) */
	for(uint16_t i=0;i<eh->e_phnum;i++){
		if(ph[i].p_type!=PT_LOAD) continue;
		if(ph[i].p_offset + ph[i].p_filesz > sz){ kprintf("[loader] ELF bounds\n"); return -1; }
		memcpy(base + (ph[i].p_vaddr - lo),
		       (const uint8_t*)img + ph[i].p_offset,
		       ph[i].p_filesz);
	}

	/* Compute runtime entry:
	   - ET_DYN (PIE): entry = base + (eh->e_entry - lo)  [= load_base + e_entry']
	   - ET_EXEC:       entry = (void*)(uintptr_t)eh->e_entry (already absolute)
	*/
	uintptr_t entry_addr;
	if(eh->e_type == ET_DYN){
		entry_addr = (uintptr_t)(base + (eh->e_entry - lo));
	}else{
		entry_addr = (uintptr_t)eh->e_entry;
	}

	/* Best-effort manifest extraction (for name/entry/caps) */
	char manifest[512]={0};
	if(extract_manifest_blob(img, sz, manifest, sizeof(manifest))!=0){
		/* fabricate a minimal manifest */
		char nm[64]={0}; basename_noext(path?path:"init", nm, sizeof(nm));
		snprintf(manifest,sizeof(manifest),
		         "{\"name\":\"%s\",\"type\":4,\"version\":\"0\",\"entry\":\"agent_main\",\"capabilities\":\"\"}", nm[0]?nm:"init");
	}else{
		kprintf("[loader] manifest found for %s\n", path?path:"(buffer)");
	}

	char entry_name[64]={0};
	if(json_get_str(manifest,"entry",entry_name,sizeof(entry_name))!=0)
		snprintf(entry_name,sizeof(entry_name),"agent_main");

	/* Register this concrete function pointer under the manifest entry name. */
	reg_entry(entry_name, (agent_entry_t)(void*)entry_addr);

	/* Add a symbol region for debugging */
	char symname[96]; char nm[64]={0}; basename_noext(path?path:"elf", nm, sizeof(nm));
	snprintf(symname,sizeof(symname),"%s:%s", nm[0]?nm:"elf", entry_name);
	symbols_add(symname, (uintptr_t)base, span);

	/* Now enforce gate + registry + spawn */
	return register_and_spawn_from_manifest(manifest, path, prio);
}

/* ------------------------- MACHO2 wrapper support ------------------------- */

static int load_agent_macho2_impl(const void* img, size_t sz, const char* path, int prio){
	/* Many .mo2 are ELF payloads with a small header/manifest. Find ELF. */
	const uint8_t* p = (const uint8_t*)memmem_local(img, sz, "\x7F""ELF", 4);
	if(p){
		size_t remain = sz - (size_t)(p - (const uint8_t*)img);
		return load_agent_elf_impl(p, remain, path, prio);
	}

	/* If no ELF payload, try manifest-only (registry entry must be kernel symbol). */
	char manifest[512]={0};
	if(extract_manifest_blob(img, sz, manifest, sizeof(manifest))!=0){
		kprintf("[loader] MACHO2 without ELF and no manifest\n");
		return -1;
	}
	return register_and_spawn_from_manifest(manifest, path, prio);
}

/* ------------------------------ FLAT loader ------------------------------- */

static int load_agent_flat_impl(const void* img, size_t sz, const char* path, int prio){
	if(!img||!sz) return -1;
	char nm[64]={0}; basename_noext(path?path:"flat", nm, sizeof(nm));
	const char* entry="agent_main";
	reg_entry(entry, (agent_entry_t)img);
	char manifest[256];
	snprintf(manifest,sizeof(manifest),
	         "{\"name\":\"%s\",\"type\":4,\"version\":\"0\",\"entry\":\"%s\",\"capabilities\":\"\"}",
	         nm[0]?nm:"flat", entry);
	return register_and_spawn_from_manifest(manifest, path, prio);
}

/* ------------------------------ Public API -------------------------------- */

static agent_format_t detect_fmt(const void* img, size_t sz){
	const uint8_t* d=(const uint8_t*)img;
	if(sz>=4 && !memcmp(d,"\x7F""ELF",4)) return AGENT_FORMAT_ELF;
	if(sz>=4 && (d[0]==0xCF&&d[1]==0xFA&&d[2]==0xED&&d[3]==0xFE)) return AGENT_FORMAT_MACHO2;
	if(sz>=4 && !memcmp(d,"NOSM",4)) return AGENT_FORMAT_NOSM;
	if(sz>0 && ((const char*)d)[0]=='{') return AGENT_FORMAT_MACHO2;
	return AGENT_FORMAT_FLAT;
}

int load_agent_auto_with_prio(const void* img, size_t sz, int prio){
	agent_format_t f = detect_fmt(img,sz);
	switch(f){
		case AGENT_FORMAT_ELF:    return load_agent_elf_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_MACHO2: return load_agent_macho2_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_FLAT:   return load_agent_flat_impl(img,sz,"(buffer)",prio);
		default: return -1;
	}
}

int load_agent_with_prio(const void* img, size_t sz, agent_format_t f, int prio){
	switch(f){
		case AGENT_FORMAT_ELF:    return load_agent_elf_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_MACHO2: return load_agent_macho2_impl(img,sz,"(buffer)",prio);
		case AGENT_FORMAT_FLAT:   return load_agent_flat_impl(img,sz,"(buffer)",prio);
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
	int out = -1;
	switch(detect_fmt(buf,sz)){
		case AGENT_FORMAT_ELF:    out=load_agent_elf_impl(buf,sz,path,prio); break;
		case AGENT_FORMAT_MACHO2: out=load_agent_macho2_impl(buf,sz,path,prio); break;
		case AGENT_FORMAT_FLAT:   out=load_agent_flat_impl(buf,sz,path,prio); break;
		default: out=-1; break;
	}
	if(g_free_fn && buf) g_free_fn(buf);
	return out;
}
