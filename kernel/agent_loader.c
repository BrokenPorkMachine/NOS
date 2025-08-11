// kernel/agent_loader.c — ultra-minimal, quiet loader (no kprintf at all)
// Purpose: avoid crashing inside kprintf/formatting by eliminating all prints.
// Behavior: map ELF (ET_EXEC or ET_DYN), apply RELATIVE relocs, spawn entry directly.
//
// NOTE: Keep this version for bring-up; once stable, reintroduce logging gradually.

#include "agent_loader.h"
#include "Task/thread.h"
#include "VM/kheap.h"
#include "agent.h"
#include "regx.h"

#include <elf.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* Hooks configured elsewhere */
static agent_read_file_fn g_read_file = 0;
static agent_free_fn      g_free_fn   = 0;
static agent_gate_fn      g_gate_fn   = 0;

void agent_loader_set_read(agent_read_file_fn r, agent_free_fn f){ g_read_file=r; g_free_fn=f; }
void agent_loader_set_gate(agent_gate_fn g){ g_gate_fn=g; }

typedef void (*agent_entry_t)(void);

/* Tiny helpers */
static size_t cstr_len(const char* s){ size_t n=0; if(!s) return 0; while(s[n]) n++; return n; }
static void cstr_copy(char* dst, size_t cap, const char* src){
	if(!dst||cap==0){ return; }
	size_t i=0; if(src){ while(src[i] && i+1<cap){ dst[i]=src[i]; i++; } }
	dst[i]=0;
}
static void path_basename_noext(const char* path, char* out, size_t cap){
	if(!out||cap==0){ return; }
	out[0]=0; if(!path){ return; }
	const char* b=path; for(const char* p=path; *p; ++p) if(*p=='/') b=p+1;
	size_t i=0; while(b[i] && i+1<cap){ out[i]=b[i]; i++; } out[i]=0;
	char* dot=NULL; for(char* p=out; *p; ++p) if(*p=='.') dot=p; if(dot) *dot=0;
}

/* 1 MiB arena fallback */
#define AGENT_ARENA_SIZE (1<<20)
static uint8_t g_agent_arena[AGENT_ARENA_SIZE];
static size_t  g_agent_bump = 0;
static void* arena_alloc(size_t sz){
	const size_t align = 0x1000;
	size_t off = (g_agent_bump + (align-1)) & ~(align-1);
	if (off + sz > AGENT_ARENA_SIZE) return NULL;
	void* p = g_agent_arena + off; g_agent_bump = off + sz; return p;
}

/* RELA (RELATIVE) */
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

static int spawn_now(const char* path, agent_entry_t fn, int prio){
	if(!fn) return -1;

	regx_manifest_t m; memset(&m,0,sizeof(m));
	m.type = 4;
	if(path){
		char nm[64]={0};
		path_basename_noext(path, nm, sizeof(nm));
		if(nm[0]) cstr_copy(m.name,sizeof(m.name), nm);
	}
	if(!m.name[0]) cstr_copy(m.name,sizeof(m.name), "agent");

	if(g_gate_fn){
		int ok=g_gate_fn(path?path:"(buffer)", m.name, m.version, m.capabilities, "agent_main");
		if(!ok) return -1;
	}
	uint64_t id = regx_register(&m,0);
	if(!id) return -1;

	n2_agent_t agent; memset(&agent,0,sizeof(agent));
	cstr_copy(agent.name,sizeof(agent.name), m.name);
	agent.entry=fn;
	n2_agent_register(&agent);

	thread_t* t=thread_create_with_priority((void(*)(void))fn, prio>0?prio:200);
	if(!t) return -1;
	return 0;
}

static int load_agent_elf_impl(const void* img, size_t sz, const char* path, int prio){
	if(!img || sz < sizeof(Elf64_Ehdr)) return -1;
	const Elf64_Ehdr* eh=(const Elf64_Ehdr*)img;
	if(memcmp(eh->e_ident, ELFMAG, SELFMAG)!=0 || eh->e_ident[EI_CLASS]!=ELFCLASS64) return -1;
	if(eh->e_phoff==0 || eh->e_phnum==0) return -1;

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
	uint8_t* base = (uint8_t*)kalloc(span);
	if(!base){ base = (uint8_t*)arena_alloc(span); if(!base) return -1; }
	memset(base, 0, span);

	for(uint16_t i=0;i<eh->e_phnum;i++){
		if(ph[i].p_type!=PT_LOAD) continue;
		if(ph[i].p_offset + ph[i].p_filesz > sz){ return -1; }
		uintptr_t dst = (uintptr_t)(base + (ph[i].p_vaddr - lo));
		memcpy((void*)dst, (const uint8_t*)img + ph[i].p_offset, ph[i].p_filesz);
	}

	(void)apply_relocations_rela(base, lo, eh, img, sz);

	if (eh->e_entry < lo || eh->e_entry >= hi) return -1;
	uintptr_t entry_addr = (uintptr_t)(base + (eh->e_entry - lo));

	return spawn_now(path, (agent_entry_t)(void*)entry_addr, prio);
}

/* Fmt detection kept minimal */
static agent_format_t detect_fmt(const void* img, size_t sz){
	const uint8_t* d=(const uint8_t*)img;
	if(sz>=4 && d[0]==0x7F && d[1]=='E' && d[2]=='L' && d[3]=='F') return AGENT_FORMAT_ELF;
	return AGENT_FORMAT_FLAT;
}

int load_agent_auto_with_prio(const void* img, size_t sz, int prio){
	agent_format_t f = detect_fmt(img,sz);
	switch(f){
		case AGENT_FORMAT_ELF:  return load_agent_elf_impl(img,sz,"(buffer)",prio);
		default: return -1;
	}
}
int load_agent_with_prio(const void* img, size_t sz, agent_format_t f, int prio){
	switch(f){
		case AGENT_FORMAT_ELF:  return load_agent_elf_impl(img,sz,"(buffer)",prio);
		default: return load_agent_auto_with_prio(img,sz,prio);
	}
}
int load_agent_auto(const void* img, size_t sz){ return load_agent_auto_with_prio(img,sz,200); }
int load_agent(const void* img, size_t sz, agent_format_t f){ return load_agent_with_prio(img,sz,f,200); }

int agent_loader_run_from_path(const char* path, int prio){
	if(!g_read_file) return -1;
	void* buf=0; size_t sz=0;
	int rc=g_read_file(path,&buf,&sz);
	if(rc<0) return rc;
	int out = -1;
	if(detect_fmt(buf,sz)==AGENT_FORMAT_ELF) out=load_agent_elf_impl(buf,sz,path,prio);
	if(g_free_fn && buf) g_free_fn(buf);
	return out;
}
