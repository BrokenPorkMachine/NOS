#include "init.h"
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/IPC/ipc.h"
#include "../../libc/libc.h"
#include "../../../kernel/drivers/IO/serial.h"

#include "../login/login.h"
#include "../vnc/vnc.h"
#include "../ssh/ssh.h"
#include "../ftp/ftp.h"
#include "../pkg/server.h"
#include "../update/server.h"

__attribute__((section("__O2INFO,__manifest")))
const char mo2_manifest[] =
"{\n"
"  \"name\": \"init\",\n"
"  \"type\": \"service_launcher\",\n"
"  \"version\": \"1.0.0\",\n"
"  \"entry\": \"init_main\"\n"
"}\n";


// --------- Service enable/disable flags (simulated config, replace with file or cmdline) -----------
static int enable_ftp   = 1;
static int enable_login = 1;
static int enable_vnc   = 0; // vnc disabled as example
static int enable_ssh   = 1;

// --------- External queues -----------
extern ipc_queue_t fs_queue;
extern ipc_queue_t pkg_queue;
extern ipc_queue_t upd_queue;

// --------- Service thread wrappers -----------
static void login_thread(void)  { login_server(&fs_queue, thread_self()); }
static void vnc_thread(void)    { vnc_server(&fs_queue, thread_self()); }
static void ssh_thread(void)    { ssh_server(&fs_queue, thread_self()); }
static void ftp_thread(void)    { ftp_server(&fs_queue, thread_self()); }
static void pkg_thread(void)    { pkg_server(&pkg_queue, thread_self()); }
static void update_thread(void) { update_server(&upd_queue, &pkg_queue, thread_self()); }

// --------- Per-service grant queues -----------
static ipc_queue_t *login_grants[]   = { &fs_queue, &pkg_queue, &upd_queue };
static ipc_queue_t *vnc_grants[]     = { &fs_queue };
static ipc_queue_t *ssh_grants[]     = { &fs_queue };
static ipc_queue_t *ftp_grants[]     = { &fs_queue };
static ipc_queue_t *pkg_grants[]     = { &pkg_queue };
static ipc_queue_t *update_grants[]  = { &upd_queue, &pkg_queue };

// --------- Health check IPC protocol ----------
#define HEALTH_RETRIES 1000

static uint32_t init_tid = 0;

// Send a ping to a service and wait briefly for a pong response.
static int ipc_ping(ipc_queue_t *q, uint32_t svc_tid) {
    if (!q)
        return 0;
    ipc_message_t ping = {0};
    ping.type = IPC_HEALTH_PING;
    if (ipc_send(q, init_tid, &ping) != 0)
        return 0;

    for (int i = 0; i < HEALTH_RETRIES; ++i) {
        ipc_message_t reply = {0};
        if (ipc_receive(q, init_tid, &reply) == 0) {
            if (reply.type == IPC_HEALTH_PONG && reply.sender == svc_tid)
                return 1;
        }
        thread_yield();
    }
    return 0;
}

// --------- Service Descriptor Struct ----------
typedef struct {
    const char *name;
    void (*entry)(void);
    ipc_queue_t **grant_queues;
    int num_grants;
    int is_core;
    int *enabled;
    int respawn_limit;
} service_desc_t;

// --------- Service Table ----------
static service_desc_t services[] = {
    { "pkg",    pkg_thread,    pkg_grants,     1, 1, NULL,         10 },
    { "update", update_thread, update_grants,  2, 1, NULL,         10 },
    { "ftp",    ftp_thread,    ftp_grants,     1, 0, &enable_ftp,   5 },
    { "login",  login_thread,  login_grants,   3, 0, &enable_login, 5 },
    { "vnc",    vnc_thread,    vnc_grants,     1, 0, &enable_vnc,   3 },
    { "ssh",    ssh_thread,    ssh_grants,     1, 0, &enable_ssh,   5 },
};
#define NUM_SERVICES (sizeof(services)/sizeof(services[0]))

// --------- Service Status Tracking ----------
typedef struct {
    thread_t *t;
    int restarts;
} service_state_t;

static service_state_t service_state[NUM_SERVICES];

// --------- Service filter/config ----------
static void apply_service_config(void) {
    // Replace with file or boot arg parsing
    // For demo, see static variables above
}

// --------- Service spawn and health ----------
static int spawn_service(size_t i) {
    service_desc_t *svc = &services[i];

    if (svc->enabled && !*(svc->enabled)) {
        serial_printf("[init] config: '%s' disabled by config\n", svc->name);
        service_state[i].t = NULL;
        service_state[i].restarts = 0;
        return 0;
    }

    thread_t *t = thread_create(svc->entry);
    if (!t) {
        if (svc->is_core) {
            serial_printf("[init] FATAL: could not create core service '%s'.\n", svc->name);
            return -1;
        } else {
            serial_printf("[init] WARNING: could not create optional service '%s'.\n", svc->name);
            return 0;
        }
    }

    for (int g = 0; g < svc->num_grants; ++g)
        ipc_grant(svc->grant_queues[g], t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    serial_printf("[init] launched service '%s' (tid=%u, restarts=%d)\n",
                  svc->name, t->id, service_state[i].restarts);
    service_state[i].t = t;
    return 1;
}

// --------- Health check: IPC ping ----------
static int service_healthcheck(size_t i) {
    thread_t *t = service_state[i].t;
    if (!t || !thread_is_alive(t))
        return 0;
    ipc_queue_t *q = NULL;
    if (services[i].num_grants > 0)
        q = services[i].grant_queues[0];
    return ipc_ping(q, t->id);
}

// --------- Periodic Health Check and Respawn ----------
static void health_check_and_respawn(void) {
    for (size_t i = 0; i < NUM_SERVICES; ++i) {
        service_desc_t *svc = &services[i];
        service_state_t *ss = &service_state[i];

        if (svc->is_core || (svc->enabled && *(svc->enabled))) {
            // If thread missing or dead, respawn (limit restarts)
            if (!ss->t || !thread_is_alive(ss->t)) {
                if (ss->restarts < svc->respawn_limit) {
                    serial_printf("[init] respawning service '%s' (restart #%d)\n", svc->name, ss->restarts+1);
                    ss->restarts++;
                    spawn_service(i);
                } else {
                    serial_printf("[init] WARNING: service '%s' exceeded respawn limit (%d). Giving up.\n",
                                   svc->name, svc->respawn_limit);
                }
                continue;
            }
            // Health check with IPC ping
            if (!service_healthcheck(i)) {
                serial_printf("[init] healthcheck: service '%s' is unhealthy, restarting...\n", svc->name);
                thread_kill(ss->t);
                ss->t = NULL;
            }
        }
    }
}

// --------- Main Init Entrypoint ----------
void init_main(ipc_queue_t *q, uint32_t self_id) {
    (void)q;
    init_tid = self_id;

    serial_puts("[init] reading service config...\n");
    apply_service_config();

    for (size_t i = 0; i < NUM_SERVICES; ++i) {
        service_state[i].restarts = 0;
        int r = spawn_service(i);
        if (r == -1) goto fail;
        thread_yield();
    }

    serial_puts("[init] all requested system services launched\n");

    /*
     * Lower our priority now that boot-time work is done so that equal-priority
     * services (like the login server) are actually scheduled.  Otherwise this
     * init thread's default high priority would keep pre-empting them and the
     * user would never see the login prompt.
     */
    // Use the scheduler API to adjust our priority so other services can run.
    thread_set_priority(thread_current(), MIN_PRIORITY);

    while (1) {
        thread_yield();
        health_check_and_respawn();
        for (volatile int i = 0; i < 100000; ++i) __asm__ __volatile__("pause");
    }

fail:
    serial_puts("[init] SYSTEM HALT: core service startup failure.\n");
    for (;;) __asm__ volatile ("hlt");
}
