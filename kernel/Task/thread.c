// kernel/Task/thread.c
#include "thread.h"
#include "../IPC/ipc.h"
#include "../../user/libc/libc.h"
#include "../../user/rt/agent_abi.h"
#include "../../nosm/drivers/IO/serial.h"
#include <stdint.h>
#include "../arch/CPU/smp.h"

extern int kprintf(const char *fmt, ...);

// Loader hooks
extern int agent_loader_run_from_path(const char *path, int prio);
extern void agent_loader_set_read(int (*reader)(const char*, const void**, size_t*),
                                  void (*freer)(void*));
__attribute__((weak)) int (*__agent_loader_spawn_fn)(const char *name, void *entry, int prio);

// FS + alloc hooks you already have
extern int fs_read_all(const char *path, void **out, size_t *out_sz);

// Linked-in agent entrypoints
extern void regx_main(void);                         // src/agents/regx/regx.c
extern void nosm_entry(void);                        // user/agents/nosm/nosm.c
extern void nosfs_server(ipc_queue_t*, uint32_t);    // user/agents/nosfs/nosfs.c

// ---- AgentAPI helpers ----
static int api_fs_read_all(const char *path, void *buf, size_t max, size_t *out_len){
    void *tmp = NULL;
    size_t len = 0;
    int rc = fs_read_all(path, &tmp, &len);
    if (rc != 0 || !tmp)
        return rc ? rc : -1;
    if (len > max)
        len = max;
    memcpy(buf, tmp, len);
    if (out_len)
        *out_len = len;
    return 0;
}
static int api_regx_load(const char *path, const char *args, uint32_t *out_tid){ (void)args; int rc=agent_loader_run_from_path(path,200); if(out_tid) *out_tid=0; return rc; }
static int api_regx_ping(void){ return 1; }
static int api_puts(const char *s){ serial_puts(s); return 0; }
static const AgentAPI k_agent_api = {
    .yield     = thread_yield,
    .self      = thread_self,
    .printf    = kprintf,
    .puts      = api_puts,
    .fs_read_all = api_fs_read_all,
    .regx_load = api_regx_load,
    .regx_ping = api_regx_ping,
};

#ifndef STACK_SIZE
#define STACK_SIZE 8192
#endif

#define THREAD_MAGIC 0x74687264UL

// Optional linker symbols; mark weak so build succeeds if n2.ld doesn't export them.
extern char __text_start[] __attribute__((weak));
extern char __text_end[]   __attribute__((weak));

extern void context_switch(uint64_t *prev_rsp, uint64_t next_rsp);

// Forward decl of the trampoline we define later
static void thread_entry(void);

static inline uintptr_t align16(uintptr_t v){ return v & ~0xFULL; }

static thread_t *zombie_list = NULL;
static thread_t thread_pool[MAX_KERNEL_THREADS];
static char     stack_pool[MAX_KERNEL_THREADS][STACK_SIZE] __attribute__((aligned(16)));

thread_t *current_cpu[MAX_CPUS] = {0};
static thread_t *tail_cpu[MAX_CPUS] = {0};
static int next_id = 1;
static thread_t main_thread;

/*
 * Validate that a thread pointer references either the bootstrap
 * main_thread or an element within the static thread_pool.
 */
static inline int thread_ptr_ok(thread_t *t) {
    return t && (t == &main_thread ||
                 (t >= thread_pool && t < thread_pool + MAX_KERNEL_THREADS));
}

ipc_queue_t fs_queue, pkg_queue, upd_queue, init_queue, regx_queue, nosm_queue;
int timer_ready = 0;

#ifdef UNIT_TEST
static inline uint64_t irq_save_disable(void){ return 0; }
static inline void irq_restore(uint64_t rf){ (void)rf; }
#else
static inline uint64_t irq_save_disable(void){ uint64_t rf; __asm__ volatile("pushfq; pop %0; cli":"=r"(rf)::"memory"); return rf; }
static inline void irq_restore(uint64_t rf){ __asm__ volatile("push %0; popfq"::"r"(rf):"memory"); }
#endif

static void enable_sse(void){
#if defined(__x86_64__)
    uint64_t cr0, cr4;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL<<2);  // clear EM
    cr0 |=  (1ULL<<1);  // set MP
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL<<9)|(1ULL<<10); // OSFXSR|OSXMMEXCPT
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
#endif
}

static inline void rq_insert_tail(int cpu, thread_t *t){
    if (!current_cpu[cpu]){ current_cpu[cpu]=tail_cpu[cpu]=t; t->next=t; return; }
    t->next=current_cpu[cpu]; tail_cpu[cpu]->next=t; tail_cpu[cpu]=t;
}
static inline void rq_remove(int cpu, thread_t *t){
    thread_t *cur=current_cpu[cpu]; if(!cur||!t) return;
    thread_t *p=cur; do{
        if(p->next==t){
            p->next=t->next;
            if(tail_cpu[cpu]==t) tail_cpu[cpu]=p;
            if(current_cpu[cpu]==t)
                current_cpu[cpu]=(t->next==t)?NULL:t->next;
            t->next=NULL;
            return;
        }
        p=p->next;
    }while(p&&p!=cur);
}
static inline void rq_requeue_tail(int cpu, thread_t *t){ if(!t||!current_cpu[cpu]) return; rq_remove(cpu,t); rq_insert_tail(cpu,t); }

void threads_early_init(void){
    enable_sse();
    zombie_list=NULL; next_id=1;
    for(int i=0;i<MAX_CPUS;++i){ current_cpu[i]=NULL; tail_cpu[i]=NULL; }
    memset(thread_pool,0,sizeof(thread_pool)); memset(stack_pool,0,sizeof(stack_pool));
    memset(&main_thread,0,sizeof(main_thread));
    main_thread.magic=THREAD_MAGIC; main_thread.id=0; main_thread.state=THREAD_RUNNING; main_thread.started=1;
    main_thread.priority=MIN_PRIORITY; main_thread.next=&main_thread;
    uint64_t rsp; __asm__ volatile("mov %%rsp,%0":"=r"(rsp));
    main_thread.rsp=rsp;
    current_cpu[0]=tail_cpu[0]=&main_thread;
}

thread_t *thread_current(void){ return current_cpu[smp_cpu_index()]; }
uint32_t  thread_self(void){ thread_t *t=thread_current(); return t? t->id:0; }

static void add_to_zombie_list(thread_t *t){ uint64_t rf=irq_save_disable(); t->next=zombie_list; zombie_list=t; irq_restore(rf); }
static void thread_reap(void){
    uint64_t rf=irq_save_disable(); thread_t *list=zombie_list; zombie_list=NULL; irq_restore(rf);
    for(thread_t *t=list;t;){ thread_t *n=t->next; memset(t,0,sizeof(thread_t)); t=n; }
}

static thread_t *pick_next(int cpu){
    thread_t *start=current_cpu[cpu];
    if(!thread_ptr_ok(start))
        return NULL;

    thread_t *t=start,*best=NULL;
    int iter=0;
    do{
        if(!thread_ptr_ok(t))
            break;
        if(t->state==THREAD_READY && (!best || t->priority>best->priority))
            best=t;
        t=t->next;
        if(++iter > MAX_KERNEL_THREADS)
            break; /* prevent infinite loops on corruption */
    }while(t && t!=start);

    if(!best){
        if(thread_ptr_ok(start) &&
           (start->state==THREAD_READY || start->state==THREAD_RUNNING))
            best=start;
    }
    if(!best) best=&main_thread;
    return best;
}

void schedule(void){
    uint64_t rf; __asm__ volatile("pushfq; pop %0; cli":"=r"(rf)::"memory");
    int cpu=smp_cpu_index(); thread_t *prev=current_cpu[cpu];
    if(!prev){ __asm__ volatile("push %0; popfq"::"r"(rf):"memory"); __asm__ volatile("hlt"); return; }
    if(prev->state==THREAD_RUNNING) prev->state=THREAD_READY;
    thread_t *next=pick_next(cpu);
    if(!next){ prev->state=THREAD_RUNNING; __asm__ volatile("push %0; popfq"::"r"(rf):"memory"); __asm__ volatile("hlt"); return; }

    /* Defensive fix: if this is the first run of `next`, verify its RET slot. */
    if (!next->started) {
        /* new stack layout we seeded:
           [r15][r14][r13][r12][rbx][rbp][rflags][rax][RET=thread_entry][ARG=func]
           RET is 64 bytes above the current new_sp.
         */
        uint64_t *new_sp   = (uint64_t *)next->rsp;
        uint64_t *ret_slot = (uint64_t *)((uintptr_t)new_sp + 8*8);

        /* Determine kernel text range. If linker symbols absent, use a conservative fallback. */
        uintptr_t text_lo = (uintptr_t)__text_start;
        uintptr_t text_hi = (uintptr_t)__text_end;
        if (text_lo == 0 || text_hi == 0 || text_hi <= text_lo) {
            text_lo = 0x0000000000100000ULL;        // typical base
            text_hi = 0x0000000002000000ULL;        // generous upper cap
        }

        /* If RET is outside .text, heal it to thread_entry. */
        uint64_t ret_val = *ret_slot;
        if (!(ret_val >= text_lo && ret_val < text_hi)) {
            *ret_slot = (uint64_t)(uintptr_t)&thread_entry;
        }
    }

    next->state=THREAD_RUNNING; next->started=1; current_cpu[cpu]=next;
    context_switch(&prev->rsp,next->rsp);
    if(prev->state==THREAD_EXITED) add_to_zombie_list(prev);
    thread_reap();
}

uint64_t schedule_from_isr(uint64_t *old_rsp){
    int cpu=smp_cpu_index(); thread_t *prev=current_cpu[cpu]; if(!prev) return (uint64_t)old_rsp;
    prev->rsp=(uint64_t)old_rsp; if(prev->state==THREAD_RUNNING) prev->state=THREAD_READY;
    thread_t *next=pick_next(cpu);
    if(!next){ current_cpu[cpu]=prev; prev->state=THREAD_RUNNING; return (uint64_t)old_rsp; }
    next->state=THREAD_RUNNING; next->started=1; current_cpu[cpu]=next; return next->rsp;
}

__attribute__((noreturn)) void thread_exit(void){
    thread_t *t = thread_current();
    if (t)
        t->state = THREAD_EXITED;
    schedule();
    __builtin_unreachable();
}

__attribute__((noreturn,used)) static void thread_start(void (*f)(void)){
    void (* volatile entry)(const AgentAPI*, uint32_t) = (void(*)(const AgentAPI*, uint32_t))f;
    uint32_t tid = thread_self();
    kprintf("[thread] id=%u first timeslice\n", tid);
    entry(&k_agent_api, tid);
    thread_exit();
}

/* Safer trampoline: tail-jump into thread_start (no stray return address). */
static void __attribute__((naked, noinline, used))
thread_entry(void){
    __asm__ volatile(
        "pop %rdi\n"        // rdi = func
        "xor %rbp, %rbp\n"  // clean frame root
        "jmp thread_start\n"
    );
}

#ifdef UNIT_TEST
uintptr_t thread_debug_get_entry_trampoline(void) { return (uintptr_t)thread_entry; }
#endif

thread_t *thread_create_with_priority(void(*func)(void), int priority){
    if(priority<MIN_PRIORITY) priority=MIN_PRIORITY;
    if(priority>MAX_PRIORITY) priority=MAX_PRIORITY;
    thread_t *t=NULL; int idx=-1;
    for(int i=0;i<(int)MAX_KERNEL_THREADS;i++){ if(thread_pool[i].magic==0){ t=&thread_pool[i]; idx=i; break; } }
    if(!t) return NULL;
    if((uintptr_t)t<0x1000) return NULL;

    memset(t,0,sizeof(thread_t));
    t->magic=THREAD_MAGIC;
    t->stack=stack_pool[idx];

    uintptr_t top = align16((uintptr_t)t->stack + STACK_SIZE);
    uint64_t *sp = (uint64_t *)top;

    /* Seed stack to match context_switch's restore order:
       pop r15,r14,r13,r12,rbx,rbp; popfq; pop rax; ret
       So stack (top→bottom): [r15][r14][r13][r12][rbx][rbp][rflags][rax][RET=thread_entry][ARG=func]
    */
    *(--sp) = (uint64_t)func;           // argument for thread_entry (rdi)
    *(--sp) = (uint64_t)thread_entry;   // return address for first ret
    *(--sp) = 0;                        // rax placeholder
    *(--sp) = 0x202;                    // rflags with IF set
    *(--sp) = 0;                        // rbp
    *(--sp) = 0;                        // rbx
    *(--sp) = 0;                        // r12
    *(--sp) = 0;                        // r13
    *(--sp) = 0;                        // r14
    *(--sp) = 0;                        // r15

    t->rsp=(uint64_t)sp;
    t->func=func;
    t->id=__atomic_fetch_add(&next_id,1,__ATOMIC_RELAXED);
    t->state=THREAD_READY;
    t->started=0;
    t->priority=priority;
    t->next=NULL;

    kprintf("[thread] spawn id=%d entry=%p stack=%p-%p prio=%d\n",
            t->id, func, t->stack, t->stack+STACK_SIZE, priority);

    uint64_t rf=irq_save_disable(); int cpu=smp_cpu_index(); rq_insert_tail(cpu,t); irq_restore(rf);
    return t;
}

thread_t *thread_create(void(*func)(void)){ return thread_create_with_priority(func,(MAX_PRIORITY+MIN_PRIORITY)/2); }

// Bridge for the agent loader: spawn a thread for the loaded agent image.
static int loader_spawn_bridge(const char *name, void *entry, int prio) {
    (void)name; // name currently unused
    thread_t *t = thread_create_with_priority((void(*)(void))entry, prio);
    return t ? (int)t->id : -1;
}

void thread_block(thread_t *t){
    if(!t||t->magic!=THREAD_MAGIC) return;
    uint64_t rf=irq_save_disable();
    t->state=THREAD_BLOCKED;
    irq_restore(rf);
    if(t==thread_current()) schedule();
}

void thread_unblock(thread_t *t){
    if(!t||t->magic!=THREAD_MAGIC) return;
    uint64_t rf=irq_save_disable();
    t->state=THREAD_READY;
    thread_t *cur=thread_current();
    int pre=(cur&&cur->state==THREAD_RUNNING&&t->priority>cur->priority);
    irq_restore(rf);
    if(pre) schedule();
}

int  thread_is_alive(thread_t *t){ return t && t->magic==THREAD_MAGIC && t->state!=THREAD_EXITED; }

void thread_kill(thread_t *t){
    if(!t||t->magic!=THREAD_MAGIC) return;
    uint64_t rf=irq_save_disable();
    t->state=THREAD_EXITED;
    int cpu=smp_cpu_index(); thread_t *cur=current_cpu[cpu];
    if(t==cur){ irq_restore(rf); schedule(); return; }
    rq_remove(cpu,t);
    add_to_zombie_list(t);
    irq_restore(rf);
}

static void rq_on_priority_change(int cpu, thread_t *t){ rq_requeue_tail(cpu,t); }

void thread_set_priority(thread_t *t,int prio){
    if(!t||t->magic!=THREAD_MAGIC) return;
    if(prio<MIN_PRIORITY) prio=MIN_PRIORITY;
    if(prio>MAX_PRIORITY) prio=MAX_PRIORITY;
    uint64_t rf=irq_save_disable();
    int old=t->priority;
    t->priority=prio;
    int cpu=smp_cpu_index();
    rq_on_priority_change(cpu,t);
    thread_t *cur=thread_current();
    int yield=((t!=cur && t->state==THREAD_READY && t->priority>(cur?cur->priority:MIN_PRIORITY)) || (t==cur && prio<old));
    irq_restore(rf);
    if(yield) schedule();
}

void thread_join(thread_t *t){
    if(!t||t->magic!=THREAD_MAGIC) return;
    while(thread_is_alive(t)){
        __asm__ volatile("pause");
        thread_yield();
    }
}

void thread_yield(void){ schedule(); }

int thread_runqueue_length(int cpu){
    thread_t *start=current_cpu[cpu];
    if(!start) return 0;
    int count=0; thread_t *t=start;
    do{ count++; t=t->next; }while(t&&t!=start);
    return count;
}

// Wrappers
static void regx_thread_wrapper(void){ regx_main(); thread_exit(); }
static void nosm_thread_wrapper(void){ nosm_entry(); thread_exit(); }
static void nosfs_thread_wrapper(void){ nosfs_server(&fs_queue, thread_self()); thread_exit(); }

// FS hook used by agent_loader_run_from_path()
static int agentfs_read_all(const char *path, const void **out, size_t *out_sz){
    return fs_read_all(path, (void**)out, out_sz);
}
static void agentfs_free(void *p){ (void)p; }

void threads_init(void){
    ipc_init(&fs_queue); ipc_init(&pkg_queue); ipc_init(&upd_queue); ipc_init(&init_queue); ipc_init(&regx_queue); ipc_init(&nosm_queue);

    agent_loader_set_read(agentfs_read_all, agentfs_free);
    __agent_loader_spawn_fn = loader_spawn_bridge;

    // Bring up NOSFS before other core agents so init.mo2 can be served.
    thread_t *t_nosfs = thread_create_with_priority(nosfs_thread_wrapper, 230);

    // Then bring up security gate and other helpers
    thread_t *t_regx  = thread_create_with_priority(regx_thread_wrapper, 220);
    thread_t *t_nosm  = thread_create_with_priority(nosm_thread_wrapper, 210);

    if(!t_nosfs) kprintf("[boot] failed to spawn nosfs\n");
    if(!t_regx){ kprintf("[boot] failed to spawn regx\n"); for(;;)__asm__ volatile("hlt"); }
    if(!t_nosm)  kprintf("[boot] failed to spawn nosm\n");

    // Capabilities: ensure clients that need the FS queue have access
    if (t_nosfs){
        ipc_grant(&fs_queue,   t_nosfs->id, IPC_CAP_SEND | IPC_CAP_RECV);
        ipc_grant(&regx_queue, t_nosfs->id, IPC_CAP_SEND | IPC_CAP_RECV);
    }
    if (t_regx){
        ipc_grant(&regx_queue, t_regx->id, IPC_CAP_SEND | IPC_CAP_RECV);
        ipc_grant(&fs_queue,   t_regx->id, IPC_CAP_SEND | IPC_CAP_RECV);   // regx needs FS to load /agents/init.mo2
    }
    if (t_nosm){
        ipc_grant(&nosm_queue, t_nosm->id, IPC_CAP_SEND | IPC_CAP_RECV);
        ipc_grant(&regx_queue, t_nosm->id, IPC_CAP_SEND | IPC_CAP_RECV);
        ipc_grant(&fs_queue,   t_nosm->id, IPC_CAP_SEND | IPC_CAP_RECV);   // optional: if nosm needs FS
    }

    if (timer_ready) thread_yield();
}
