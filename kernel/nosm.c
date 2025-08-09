#include "nosm.h"
#include "nosm_ipc.h"
#include "IPC/ipc.h"
#include "Task/thread.h"
#include "printf.h"
#include "macho2.h"          /* your minimal parser to locate symbols/sections */

extern int printf(const char *fmt, ...);

extern ipc_queue_t nosm_queue;  /* create/init this in threads_init; grant nosm agent */
#define MAX_NMODS 64

typedef struct {
    uint32_t mod_id;
    uint8_t  active;
    uint64_t caps;
    nosm_module_ops_t ops;
} nmod_slot_t;

static nmod_slot_t g_mods[MAX_NMODS];
static uint32_t g_next_mod_id = 1;

static nmod_slot_t* find_slot(uint32_t id) {
    for (int i=0;i<MAX_NMODS;i++) if (g_mods[i].active && g_mods[i].mod_id==id) return &g_mods[i];
    return NULL;
}
static nmod_slot_t* alloc_slot(uint32_t id) {
    for (int i=0;i<MAX_NMODS;i++) if (!g_mods[i].active) { g_mods[i].mod_id=id; g_mods[i].active=1; return &g_mods[i]; }
    return NULL;
}

int nosm_cap_check(uint32_t mod_id, uint64_t need_caps) {
    nmod_slot_t *s = find_slot(mod_id);
    return (s && ( (s->caps & need_caps) == need_caps)) ? 0 : -1;
}

void nosm_revoke(uint32_t mod_id) {
    nmod_slot_t *s = find_slot(mod_id);
    if (!s) return;
    printf("[nosm] revoking module %u\n", mod_id);
    if (s->ops.suspend) s->ops.suspend();
    if (s->ops.fini)    s->ops.fini();
    s->active = 0;
}

int nosm_unload(uint32_t mod_id) {
    nmod_slot_t *s = find_slot(mod_id);
    if (!s) return -1;
    if (s->ops.fini) s->ops.fini();
    s->active = 0;
    return 0;
}

int nosm_request_verify_and_load(const void *blob, uint32_t len, uint32_t *out_mod_id) {
    if (!blob || len < 16) return -1;

    uint32_t mod_id = __atomic_fetch_add(&g_next_mod_id, 1, __ATOMIC_RELAXED);
    ipc_message_t req = {0}, resp = {0};
    nosm_ipc_build_verify(&req, mod_id, blob, len);
    if (ipc_send(&nosm_queue, 0 /*kernel*/, &req) != 0) {
        printf("[nosm] IPC send failed\n"); return -1;
    }
    /* Wait for response */
    for (int i=0;i<20000;i++){
        if (ipc_receive(&nosm_queue, 0, &resp)==0 && resp.type==NOSM_IPC_VERIFY_RESP) break;
        thread_yield();
    }
    if (resp.type!=NOSM_IPC_VERIFY_RESP || resp.len < 9) { printf("[nosm] bad verify resp\n"); return -1; }

    nosm_capset_t cs = {0};
    __builtin_memcpy(&cs, resp.data, sizeof(cs));
    nosm_verify_status_t st = {0};
    st.status = resp.data[8];

    if (st.status != 0) {
        printf("[nosm] verify denied: %s\n", (resp.len>9 ? (char*)&resp.data[9] : "no reason"));
        return -1;
    }

    /* parse Mach-O2, locate symbol nosm_module_ops and copy ops table */
    nosm_module_ops_t *ops = (nosm_module_ops_t*)macho2_find_symbol(blob, len, NOSM_MODULE_ENTRY_SYMBOL);
    if (!ops || !ops->init) { printf("[nosm] invalid module ops\n"); return -1; }

    nmod_slot_t *slot = alloc_slot(mod_id);
    if (!slot) { printf("[nosm] no slots\n"); return -1; }

    slot->caps = cs.caps;
    /* Copy ops by value so module can be discarded after REL reloc applied if you do in-place */
    slot->ops = *ops;

    nosm_env_t env = { .mod_id = mod_id, .caps = cs.caps };
    int rc = slot->ops.init(&env);
    if (rc != 0) {
        printf("[nosm] init() failed rc=%d\n", rc);
        slot->active = 0;
        return -1;
    }
    if (out_mod_id) *out_mod_id = mod_id;
    printf("[nosm] module %u loaded (caps=0x%llx)\n", mod_id, (unsigned long long)cs.caps);
    return 0;
}

