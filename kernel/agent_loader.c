// kernel/agent_loader.c — BYPASS version (no regx, no gate, no prints)
// Goal: rule out faults in regx/gate/manifest/formatting by avoiding them entirely.
// This just maps the ELF, applies RELATIVE relocs, and creates a thread at entry.
//
// Public API kept identical so the rest of the kernel links the same.

#include "agent_loader.h"
#include "Task/thread.h"
#include "VM/kheap.h"
#include <elf.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

typedef void (*agent_entry_t)(void);

/* FS hooks wired elsewhere */
static agent_read_file_fn g_read_file = 0;
static agent_free_fn      g_free_fn   = 0;
void agent_loader_set_read(agent_read_file_fn r, agent_free_fn f){ g_read_file=r; g_free_fn=f; }

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
static void apply_relocations_rela(uint8_t* base, uint64_t lo, const Elf64_Ehdr* eh, const void* img, size_t sz){
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
	(void)apply_relocations_rela_table(base, lo, rela, rela_sz);
	(void)apply_relocations_rela_table(base, lo, jmprel, pltrel_sz);
}

static int elf_map_and_spawn(const void* img, size_t sz, int prio){
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

	apply_relocations_rela(base, lo, eh, img, sz);

	if (eh->e_entry < lo || eh->e_entry >= hi) return -1;
	uintptr_t entry_addr = (uintptr_t)(base + (eh->e_entry - lo));

	/* Bypass registry/gate: spawn thread directly */
	thread_t* t=thread_create_with_priority((void(*)(void))(void*)entry_addr, prio>0?prio:200);
	return t ? 0 : -1;
}

/* Public API */
int load_agent_auto_with_prio(const void* img, size_t sz, int prio){
	return elf_map_and_spawn(img, sz, prio);
}
int load_agent_with_prio(const void* img, size_t sz, agent_format_t f, int prio){
	(void)f; return elf_map_and_spawn(img, sz, prio);
}
int load_agent_auto(const void* img, size_t sz){ return elf_map_and_spawn(img, sz, 200); }
int load_agent(const void* img, size_t sz, agent_format_t f){ (void)f; return elf_map_and_spawn(img, sz, 200); }

int agent_loader_run_from_path(const char* path, int prio){
	if(!g_read_file) return -1;
	void* buf=0; size_t sz=0;
	int rc=g_read_file(path,&buf,&sz);
	if(rc<0) return rc;
	int out = elf_map_and_spawn(buf, sz, prio);
	if(g_free_fn && buf) g_free_fn(buf);
	return out;
}
