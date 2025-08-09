// kernel/Task/thread.c
#include "thread.h"
#include "../IPC/ipc.h"
#include "../../user/libc/libc.h"
#include <stdint.h>
#include "../arch/CPU/smp.h"

extern int kprintf(const char *fmt, ...);

// Loader hooks
extern void agent_loader_set_read(int (*reader)(const char*, void**, size_t*),
                                  void (*freer)(void*));
extern int agent_loader_run_from_path(const char *path, int prio);

// FS + alloc hooks you already have
extern int fs_read_all(const char *path, void **out, size_t *out_sz);
extern void kfree(void *p);

// Linked-in agent entrypoints
extern void regx_main(void);                         // src/agents/regx/regx.c
extern void nosm_entry(void);                        // user/agents/nosm/nosm.c
extern void nosfs_server(ipc_queue_t*, uint32_t);    // user/agents/nosfs/nosfs.c

#ifndef STACK_SIZE
#define STACK_SIZE 8192
#endif

#define THREAD_MAGIC 0x74687264UL

extern void context_switch(uint64_t *prev_rsp, uint64_t next_rsp);

typedef struct context_frame {
    uint64_t r15, r14, r13, r12, rbx, rbp;
    uint64_t rflags;
    uint64_t rax_dummy;
    uint64_t rip;
    uint64_t arg_rdi;
} context_frame_t;

static inline uintptr_t align16(uintptr_t v){ return v & ~0xFULL; }

static thread_t *zombie_list = NULL;
static thread_t thread_pool[MAX_KERNEL_THREADS];
static char     stack_pool[MAX_KERNEL_THREADS][STACK_SIZE];

thread_t *current_cpu[MAX_CPUS] = {0};
static thread_t *tail_cpu[MAX_CPUS] = {0};
static int next_id = 1;
static thread_t main_thread;

ipc_queue_t fs_queue, pkg_queue, upd_queue, init_queue, regx_queue;
int timer_ready = 0;

static inline uint64_t irq_save_disable(void){ uint64_t rf; __asm__ volatile("pushfq; pop %0; cli":"=r"(rf)::"memory"); return rf; }
static inline void irq_restore(uint64_t rf){ __asm__ volatile("push %0; popfq"::"r"(rf):"memory"); }

static void enable_sse(void){
#if defined(__x86_64__)
    uint64_t cr0, cr4;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0)); cr0 &= ~(1ULL<<2); cr0 |= (1ULL<<1);
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4)); cr4 |= (1ULL<<9)|(1ULL<<10);
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
        if(p->next==t){ p->next=t->next; if(tail_cpu[cpu]==t) tail_cpu[cpu]=p;
            if(current_cpu[cpu]==t) current_cpu[cpu]=(t->next==t)?NULL:t->next; t->next=NULL; return; }
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
    uint64_t rsp; __asm__ volatile("mov %%rsp,%0":"=r"(rsp)); main_thread.rsp=rsp;
    current_cpu[0]=tail_cpu[0]=&main_thread;
}

thread_t *thread_current(void){ return current_cpu[smp_cpu_index()]; }
uint32_t  thread_self(void){ thread_t *t=thread_current(); return t? t->id:0; }

static void add_to_zombie_list(thread_t *t){ uint64_t rf=irq_save_disable(); t->next=zombie_list; zombie_list=t; irq_restore(rf); }
static void thread_reap(void){ uint64_t rf=irq_save_disable(); thread_t *list=zombie_list; zombie_list=NULL; irq_restore(rf);
    for(thread_t *t=list;t;){ thread_t *n=t->next; memset(t,0,sizeof(thread_t)); t=n; } }

static thread_t *pick_next(int cpu){
    thread_t *start=current_cpu[cpu]; if(!start) return NULL;
    thread_t *t=start,*best=NULL;
    do{ if(t->state==THREAD_READY && (!best || t->priority>best->priority)) best=t; t=t->next; } while(t&&t!=start);
    if(!best){ if(start->state==THREAD_READY||start->state==THREAD_RUNNING) best=start; }
    if(!best) best=&main_thread; return best;
}

void schedule(void){
    uint64_t rf; __asm__ volatile("pushfq; pop %0; cli":"=r"(rf)::"memory");
    int cpu=smp_cpu_index(); thread_t *prev=current_cpu[cpu];
    if(!prev){ __asm__ volatile("push %0; popfq"::"r"(rf):"memory"); __asm__ volatile("hlt"); return; }
    if(prev->state==THREAD_RUNNING) prev->state=THREAD_READY;
    thread_t *next=pick_next(cpu);
    if(!next){ prev->state=THREAD_RUNNING; __asm__ volatile("push %0; popfq"::"r"(rf):"memory"); __asm__ volatile("hlt"); return; }
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

__attribute__((noreturn)) void thread_exit(void){ thread_t *t=thread_current(); if(t) t->state=THREAD_EXITED; schedule(); __builtin_unreachable(); }
__attribute__((noreturn,used)) static void thread_start(void(*f)(void)){ f(); thread_exit(); }
static void __attribute__((naked,noreturn)) thread_entry(void){ __asm__ volatile("pop %rdi\ncall thread_start\njmp .\n"); }

thread_t *thread_create_with_priority(void(*func)(void), int priority){
    if(priority<MIN_PRIORITY) priority=MIN_PRIORITY; if(priority>MAX_PRIORITY) priority=MAX_PRIORITY;
    thread_t *t=NULL; int idx=-1;
    for(int i=0;i<(int)MAX_KERNEL_THREADS;i++){ if(thread_pool[i].magic==0){ t=&thread_pool[i]; idx=i; break; } }
    if(!t) return NULL; if((uintptr_t)t<0x1000) return NULL;
    memset(t,0,sizeof(thread_t)); t->magic=THREAD_MAGIC; t->stack=stack_pool[idx];
    uintptr_t top=align16((uintptr_t)t->stack+STACK_SIZE);
    context_frame_t *cf=(context_frame_t*)(top-sizeof(*cf));
    *cf=(context_frame_t){ .r15=0,.r14=0,.r13=0,.r12=0,.rbx=0,.rbp=0, .rflags=0x202,.rax_dummy=0, .rip=(uint64_t)thread_entry, .arg_rdi=(uint64_t)func };
    t->rsp=(uint64_t)cf; t->func=func; t->id=__atomic_fetch_add(&next_id,1,__ATOMIC_RELAXED);
    t->state=THREAD_READY; t->started=0; t->priority=priority; t->next=NULL;
    uint64_t rf=irq_save_disable(); int cpu=smp_cpu_index(); rq_insert_tail(cpu,t); irq_restore(rf); return t;
}
thread_t *thread_create(void(*func)(void)){ return thread_create_with_priority(func,(MAX_PRIORITY+MIN_PRIORITY)/2); }
void thread_block(thread_t *t){ if(!t||t->magic!=THREAD_MAGIC) return; uint64_t rf=irq_save_disable(); t->state=THREAD_BLOCKED; irq_restore(rf); if(t==thread_current()) schedule(); }
void thread_unblock(thread_t *t){ if(!t||t->magic!=THREAD_MAGIC) return; uint64_t rf=irq_save_disable(); t->state=THREAD_READY; thread_t *cur=thread_current(); int pre=(cur&&cur->state==THREAD_RUNNING&&t->priority>cur->priority); irq_restore(rf); if(pre) schedule(); }
int  thread_is_alive(thread_t *t){ return t && t->magic==THREAD_MAGIC && t->state!=THREAD_EXITED; }
void thread_kill(thread_t *t){ if(!t||t->magic!=THREAD_MAGIC) return; uint64_t rf=irq_save_disable(); t->state=THREAD_EXITED; int cpu=smp_cpu_index(); thread_t *cur=current_cpu[cpu];
    if(t==cur){ irq_restore(rf); schedule(); return; } rq_remove(cpu,t); add_to_zombie_list(t); irq_restore(rf); }
static void rq_on_priority_change(int cpu, thread_t *t){ rq_requeue_tail(cpu,t); }
void thread_set_priority(thread_t *t,int prio){
    if(!t||t->magic!=THREAD_MAGIC) return; if(prio<MIN_PRIORITY) prio=MIN_PRIORITY; if(prio>MAX_PRIORITY) prio=MAX_PRIORITY;
    uint64_t rf=irq_save_disable(); int old=t->priority; t->priority=prio; int cpu=smp_cpu_index(); rq_on_priority_change(cpu,t);
    thread_t *cur=thread_current(); int yield=((t!=cur && t->state==THREAD_READY && t->priority>(cur?cur->priority:MIN_PRIORITY)) || (t==cur && prio<old));
    irq_restore(rf); if(yield) schedule();
}
void thread_join(thread_t *t){ if(!t||t->magic!=THREAD_MAGIC) return; while(thread_is_alive(t)){ __asm__ volatile("pause"); thread_yield(); } }
void thread_yield(void){ schedule(); }

// Wrappers
static void regx_thread_wrapper(void){ regx_main(); thread_exit(); }
static void nosm_thread_wrapper(void){ nosm_entry(); thread_exit(); }
static void nosfs_thread_wrapper(void){ nosfs_server(&fs_queue, thread_self()); thread_exit(); }

// FS hook used by agent_loader_run_from_path()
static int agentfs_read_all(const char *path, void **out, size_t *out_sz){ return fs_read_all(path, out, out_sz); }
static void agentfs_free(void *p){ kfree(p); }

void threads_init(void){
    ipc_init(&fs_queue); ipc_init(&pkg_queue); ipc_init(&upd_queue); ipc_init(&init_queue); ipc_init(&regx_queue);

    agent_loader_set_read(agentfs_read_all, agentfs_free);

    // Start the security gate first
    thread_t *t_regx  = thread_create_with_priority(regx_thread_wrapper, 220);
    // Bring core helpers alongside
    thread_t *t_nosm  = thread_create_with_priority(nosm_thread_wrapper, 210);
    thread_t *t_nosfs = thread_create_with_priority(nosfs_thread_wrapper, 200);

    if(!t_regx){ kprintf("[boot] failed to spawn regx\n"); for(;;)__asm__ volatile("hlt"); }
    if(!t_nosm)  kprintf("[boot] failed to spawn nosm\n");
    if(!t_nosfs) kprintf("[boot] failed to spawn nosfs\n");

    ipc_grant(&regx_queue, t_regx->id, IPC_CAP_SEND | IPC_CAP_RECV);
    if (t_nosfs){
        ipc_grant(&fs_queue,   t_nosfs->id, IPC_CAP_SEND | IPC_CAP_RECV);
        ipc_grant(&regx_queue, t_nosfs->id, IPC_CAP_SEND | IPC_CAP_RECV);
    }
    if (t_nosm){
        ipc_grant(&regx_queue, t_nosm->id, IPC_CAP_SEND | IPC_CAP_RECV);
    }

    if (timer_ready) thread_yield();
}
