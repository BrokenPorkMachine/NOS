// user/agents/init/init.c
//
// Standalone init agent loaded and gated by regx.
// Default build is AGENT mode where init *requests* regx to load other agents
// (login, pkg, update, ftp, ssh, vnc) as separate binaries.
// If KERNEL_BUILD is defined, it falls back to directly spawning threads.
//
// Manifest is kept in a Mach-O2 style section so the agent loader can detect it.

#include "init.h"
#include "../../libc/libc.h"

// ---------- Logging shim ----------
#ifndef KERNEL_BUILD
  // Agent mode: use libc printf
  #define LOG(...)  printf(__VA_ARGS__)
  #define LOGS(s)   puts(s)
#else
  // Kernel-linked fallback: use serial
  #include "../../../kernel/drivers/IO/serial.h"
  #define LOG(...)  serial_printf(__VA_ARGS__)
  #define LOGS(s)   serial_puts(s)
#endif

// ---------- Common IPC/Thread headers ----------
#ifdef KERNEL_BUILD
  #include "../../../kernel/IPC/ipc.h"
  #include "../../../kernel/Task/thread.h"
#else
  // Agents still use the same IPC/thread ABI exposed by libc headers
  #include "../../../kernel/IPC/ipc.h"
  #include "../../../kernel/Task/thread.h"
#endif

// ---------- Keep Mach-O2 manifest so regx/loader can discover the entry ----------
__attribute__((section("__O2INFO,__manifest")))
const char mo2_manifest[] =
"{\n"
"  \"name\": \"init\",\n"
"  \"type\": \"service_launcher\",\n"
"  \"version\": \"1.0.0\",\n"
"  \"entry\": \"init_main\"\n"
"}\n";

/* =====================================================================================
 *                                 AGENT MODE (default)
 * ===================================================================================*/
#ifndef KERNEL_BUILD

// In agent mode, init does not link the service servers. Instead it sends
// requests to regx to load those agents by name. We define a minimal control
// protocol to talk to regx. regx should service these messages on a known queue.

// Known well-known control queue exported by the kernel/registry.
// (Kernel should define/initialize this; we just extern it here.)
extern ipc_queue_t regx_queue;

// Simple control opcodes understood by regx (keep in sync with regx.c)
typedef enum {
    REGX_CTL_LOAD = 1,   // payload: agent name (null-terminated)
    REGX_CTL_HEALTH = 2  // payload: agent name -> reply PONG if healthy
} regx_ctl_op_t;

typedef struct {
    uint32_t op;                 // regx_ctl_op_t
    char     name[32];           // agent name, e.g., "login"
} regx_ctl_msg_t;

static uint32_t init_tid = 0;

static int regx_load_agent(const char *name) {
    regx_ctl_msg_t body = {0};
    body.op = REGX_CTL_LOAD;
    // bounded copy
    size_t i = 0;
    while (name[i] && i < sizeof(body.name) - 1) { body.name[i] = name[i]; i++; }
    body.name[i] = '\0';

    ipc_message_t m = {0};
    m.type = 0x52584C44; // 'R','X','L','D' (REGX LOAD), arbitrary but consistent
    m.len  = sizeof(body);
    m.data = (uint8_t*)&body;

    if (ipc_send(&regx_queue, init_tid, &m) != 0) {
        LOG("[init] regx_load_agent('%s'): send failed\n", name);
        return -1;
    }

    // Optional: wait for an ACK from regx (non-blocking-ish)
    for (int tries = 0; tries < 2000; ++tries) {
        ipc_message_t reply = {0};
        if (ipc_receive(&regx_queue, init_tid, &reply) == 0) {
            // Treat any reply to this tid as ack for now
            return 0;
        }
        thread_yield();
    }
    LOG("[init] regx_load_agent('%s'): no ACK (continuing)\n", name);
    return 0;
}

static void init_load_all_via_regx(void) {
    LOGS("[init] requesting regx to load core services...");
    (void)regx_load_agent("pkg");
    (void)regx_load_agent("update");

    LOGS("[init] requesting regx to load optional/user services...");
    (void)regx_load_agent("login");
    (void)regx_load_agent("ftp");
    (void)regx_load_agent("ssh");
    // vnc is optional; leave disabled by default or enable as needed
    // (void)regx_load_agent("vnc");
}

// Agent entrypoint — called by loader with our TID
void init_main(ipc_queue_t *unused_q, uint32_t self_id) {
    (void)unused_q;
    init_tid = self_id;

    LOGS("[init] (agent) starting up; delegating to regx...");

    // Ask regx to load/launch the rest
    init_load_all_via_regx();

    // Drop our runtime priority to get out of the way
    thread_set_priority(thread_current(), MIN_PRIORITY);

    // Lightweight supervise loop: give CPU back, optionally we could ping regx.
    while (1) {
        thread_yield();
        for (volatile int i = 0; i < 100000; ++i) __asm__ __volatile__("pause");
    }
}

/* =====================================================================================
 *                               KERNEL FALLBACK MODE
 * ===================================================================================*/
#else  // KERNEL_BUILD defined

// Kernel-linked fallback: direct thread entry points for services.
#include "../login/login.h"
#include "../vnc/vnc.h"
#include "../ssh/ssh.h"
#include "../ftp/ftp.h"
#include "../pkg/server.h"
#include "../update/server.h"

// External queues created by the kernel
extern ipc_queue_t fs_queue;
extern ipc_queue_t pkg_queue;
extern ipc_queue_t upd_queue;

static void login_thread(void)  { login_server(&fs_queue, thread_self()); }
static void vnc_thread(void)    { vnc_server(&fs_queue, thread_self()); }
static void ssh_thread(void)    { ssh_server(&fs_queue, thread_self()); }
static void ftp_thread(void)    { ftp_server(&fs_queue, thread_self()); }
static void pkg_thread(void)    { pkg_server(&pkg_queue, thread_self()); }
static void update_thread(void) { update_server(&upd_queue, &pkg_queue, thread_self()); }

typedef struct {
    const char *name;
    void (*entry)(void);
    ipc_queue_t **grants;
    int num_grants;
    int is_core;
    int respawn_limit;
} svc_t;

static ipc_queue_t *login_grants[]  = { &fs_queue, &pkg_queue, &upd_queue };
static ipc_queue_t *vnc_grants[]    = { &fs_queue };
static ipc_queue_t *ssh_grants[]    = { &fs_queue };
static ipc_queue_t *ftp_grants[]    = { &fs_queue };
static ipc_queue_t *pkg_grants[]    = { &pkg_queue };
static ipc_queue_t *upd_grants[]    = { &upd_queue, &pkg_queue };

static svc_t svcs[] = {
    { "pkg",    pkg_thread,    pkg_grants,  1, 1, 10 },
    { "update", update_thread, upd_grants,  2, 1, 10 },
    { "login",  login_thread,  login_grants,3, 0,  5 },
    { "ftp",    ftp_thread,    ftp_grants,  1, 0,  5 },
    { "ssh",    ssh_thread,    ssh_grants,  1, 0,  5 },
    // { "vnc", vnc_thread, vnc_grants, 1, 0, 3 }, // optional
};
#define N_SVCS (int)(sizeof(svcs)/sizeof(svcs[0]))

typedef struct { thread_t *t; int restarts; } sstate_t;
static sstate_t st[N_SVCS];
static uint32_t init_tid;

static int spawn(int i) {
    thread_t *t = thread_create(svcs[i].entry);
    if (!t) {
        if (svcs[i].is_core) {
            LOG("[init] FATAL: cannot start '%s'\n", svcs[i].name);
            return -1;
        }
        LOG("[init] WARN: cannot start '%s'\n", svcs[i].name);
        return 0;
    }
    for (int g = 0; g < svcs[i].num_grants; ++g)
        ipc_grant(svcs[i].grants[g], t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    LOG("[init] launched '%s' tid=%u\n", svcs[i].name, t->id);
    st[i].t = t;
    return 1;
}

void init_main(ipc_queue_t *q, uint32_t self_id) {
    (void)q;
    init_tid = self_id;

    for (int i = 0; i < N_SVCS; ++i) {
        st[i].restarts = 0;
        if (spawn(i) == -1) goto fail;
        thread_yield();
    }
    LOGS("[init] services launched");

    thread_set_priority(thread_current(), MIN_PRIORITY);

    while (1) {
        thread_yield();
        for (volatile int i = 0; i < 100000; ++i) __asm__ __volatile__("pause");
    }

fail:
    LOGS("[init] SYSTEM HALT");
    for (;;) __asm__ __volatile__("hlt");
}

#endif /* KERNEL_BUILD */
