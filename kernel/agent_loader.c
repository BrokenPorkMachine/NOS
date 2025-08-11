// kernel/agent_loader.c — PROBE IDT[6] + BYPASS loader + C trampoline
// - No kprintf/varargs. All output uses COM1 directly (0x3F8).
// - Before spawning, dumps IDTR and vector 6 gate, and patches it if it
//   points into 0xA0000–0xFFFFF (VRAM/BIOS area), which matches RIP=0xB0000 you saw.
// - Maps ET_EXEC as PIE, applies RELATIVE RELA, spawns agent via trampoline
//   that calls thread_exit() so it never "falls off".
// - No regx/gate calls; provides a no-op gate setter for linkage.

#include "agent_loader.h"
#include "Task/thread.h"
#include "VM/kheap.h"
#include <elf.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

typedef void (*agent_entry_t)(void);

/* ===== Raw serial (COM1, 0x3F8) ===== */
static inline void outb(uint16_t port, uint8_t val){ __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port)); }
static inline uint8_t inb(uint16_t port){ uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port)); return v; }
static void serial_init_once(void){
	static int init=0; if(init) return; init=1;
	const uint16_t b=0x3F8;
	outb(b+1,0x00);       // IER=0
	outb(b+3,0x80);       // LCR: DLAB=1
	outb(b+0,0x01);       // DLL=1 (115200)
	outb(b+1,0x00);       // DLM=0
	outb(b+3,0x03);       // LCR: 8N1
	outb(b+2,0xC7);       // FCR: enable, clear, 14-byte
	outb(b+4,0x0B);       // MCR: IRQs, RTS/DSR set
}
static void serial_putc(char c){
	const uint16_t b=0x3F8;
	for(;;){ if(inb(b+5)&0x20) break; } // THR empty
	outb(b, (uint8_t)c);
}
static void serial_puts(const char* s){ if(!s) return; serial_init_once(); while(*s) serial_putc(*s++); }
static void serial_hex64(uint64_t v){
	for(int i=60;i>=0;i-=4){
		uint8_t n=(v>>i)&0xF; char c=(n<10)?('0'+n):('A'+(n-10)); serial_putc(c);
	}
}
static void serial_hex16(uint16_t v){
	for(int i=12;i>=0;i-=4){
		uint8_t n=(v>>i)&0xF; char c=(n<10)?('0'+n):('A'+(n-10)); serial_putc(c);
	}
}

/* ===== IDT probe/patch ===== */
struct __attribute__((packed)) idt_desc64 { uint16_t limit; uint64_t base; };
struct __attribute__((packed)) idt_entry64 {
	uint16_t off_lo;
	uint16_t sel;
	uint8_t  ist;
	uint8_t  type_attr;
	uint16_t off_mid;
	uint32_t off_hi;
	uint32_t zero;
};
static void idt_dump_ud_and_maybe_patch(void){
	serial_puts("[idt] probe v6\n");
	struct idt_desc64 idtr = {0};
	__asm__ volatile("sidt %0" : "=m"(idtr));
	serial_puts("[idt] base="); serial_hex64(idtr.base);
	serial_puts(" limit=");     serial_hex16(idtr.limit); serial_puts("\n");

	if(!idtr.base) { serial_puts("[idt] no base\n"); return; }
	if(idtr.limit < sizeof(struct idt_entry64)*7 - 1){ serial_puts("[idt] short\n"); return; }

	volatile struct idt_entry64* idt = (volatile struct idt_entry64*)(uintptr_t)idtr.base;
	volatile struct idt_entry64 e6 = idt[6];
	uint64_t off = ((uint64_t)e6.off_hi<<32) | ((uint64_t)e6.off_mid<<16) | e6.off_lo;

	serial_puts("[idt] v6 off="); serial_hex64(off);
	serial_puts(" sel=");         serial_hex16(e6.sel);
	serial_puts(" attr=");        serial_hex16(e6.type_attr);
	serial_puts(" ist=");         serial_hex16(e6.ist); serial_puts("\n");

	/* If handler is in legacy range, patch to safe stub */
	if(off>=0x00000000000A0000ULL && off<=0x00000000000FFFFFULL){
		serial_puts("[idt] v6 BAD range; patching\n");
		extern void ud_safe_stub(void);
		uint64_t h = (uint64_t)(uintptr_t)&ud_safe_stub;
		uint16_t cs; __asm__ volatile("mov %%cs,%0":"=r"(cs));
		struct idt_entry64 ne;
		ne.off_lo   = (uint16_t)(h & 0xFFFF);
		ne.sel      = cs;
		ne.ist      = 0;                // IST=0
		ne.type_attr= 0x8E;             // present, DPL=0, 64-bit interrupt gate
		ne.off_mid  = (uint16_t)((h>>16)&0xFFFF);
		ne.off_hi   = (uint32_t)((h>>32)&0xFFFFFFFF);
		ne.zero     = 0;
		idt[6] = ne;
		serial_puts("[idt] v6 new off="); serial_hex64(h); serial_puts("\n");
	}
}

/* Minimal UD handler stub: acknowledge and return or park */
__attribute__((naked)) void ud_safe_stub(void){
	__asm__ __volatile__(
		"push %rax; push %rcx; push %rdx; push %rsi; push %rdi; push %r8; push %r9; push %r10; push %r11;\n\t"
		/* If you have a per-CPU ack or logging, call it here (omitted) */
		"pop %r11; pop %r10; pop %r9; pop %r8; pop %rdi; pop %rsi; pop %rdx; pop %rcx; pop %rax;\n\t"
		"iretq\n\t"
	);
}

/* ===== Loader internals ===== */

/* FS hooks wired elsewhere */
static agent_read_file_fn g_read_file = 0;
static agent_free_fn      g_free_fn   = 0;
void agent_loader_set_read(agent_read_file_fn r, agent_free_fn f){ g_read_file=r; g_free_fn=f; }
/* Provide a no-op gate setter to satisfy callers */
void agent_loader_set_gate(agent_gate_fn g){ (void)g; }

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
	(void)apply_relocations_rela_table(base, lo, rela,     rela_sz);
	(void)apply_relocations_rela_table(base, lo, jmprel,   pltrel_sz);
}

/* Trampoline that ends with thread_exit() */
static agent_entry_t g_agent_entry = 0;
extern void thread_exit(void) __attribute__((weak));
static void agent_trampoline(void){
	agent_entry_t fn = g_agent_entry;
	if(fn) fn();
	if(thread_exit) thread_exit();
	for(;;){ __asm__ __volatile__("cli; hlt" ::: "memory"); }
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

	/* Probe/patch IDT[6] once before we run agent code */
	static int probed=0; if(!probed){ probed=1; idt_dump_ud_and_maybe_patch(); }

	/* Use a C trampoline so the entry can never return to garbage */
	g_agent_entry = (agent_entry_t)(void*)entry_addr;
	thread_t* t=thread_create_with_priority(agent_trampoline, prio>0?prio:200);
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
int load_agent(const void* img, size_t sz, agent_format_t f){ (void)f; return elf_map_and_spawn(img,  sz, 200); }

int agent_loader_run_from_path(const char* path, int prio){
	if(!g_read_file) return -1;
	void* buf=0; size_t sz=0;
	int rc=g_read_file(path,&buf,&sz);
	if(rc<0) return rc;
	int out = elf_map_and_spawn(buf, sz, prio);
	if(g_free_fn && buf) g_free_fn(buf);
	return out;
}
