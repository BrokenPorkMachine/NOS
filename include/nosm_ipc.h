#pragma once
#include <stdint.h>
#include "IPC/ipc.h"

/* Message types (req->type) */
enum {
    NOSM_IPC_VERIFY_REQ = 0x4000,  /* kernel→nosm: verify a module blob */
    NOSM_IPC_VERIFY_RESP,          /* nosm→kernel: result + capset */
    NOSM_IPC_REVOKE,               /* nosm→kernel: revoke module (policy/runtime) */
    NOSM_IPC_HEALTH_PING,          /* health check ping/pong */
    NOSM_IPC_HEALTH_PONG
};

/* Capabilities a module can be granted (bitmap). Extend freely. */
typedef enum {
    NOSM_CAP_NONE      = 0,
    NOSM_CAP_IOPORT    = 1u<<0,
    NOSM_CAP_MMIO      = 1u<<1,
    NOSM_CAP_IRQ       = 1u<<2,
    NOSM_CAP_PCI_ENUM  = 1u<<3,
    NOSM_CAP_NETSTACK  = 1u<<4,
    NOSM_CAP_FS        = 1u<<5,
} nosm_cap_t;

typedef struct {
    uint32_t  mod_id;        /* chosen by kernel on request */
    uint32_t  reserved;
    uint64_t  caps;          /* bitmap (nosm_cap_t) */
} nosm_capset_t;

/* VERIFY request layout:
 *   data[0..3]   = mod_id (uint32_t)
 *   data[4..7]   = reserved
 *   data[8..]    = module payload (Mach-O2 blob with __O2INFO)
 */
static inline void nosm_ipc_build_verify(ipc_message_t *m, uint32_t mod_id,
                                         const void *blob, uint32_t blob_len) {
    m->type = NOSM_IPC_VERIFY_REQ;
    m->len  = 8 + blob_len;
    ((uint32_t*)m->data)[0] = mod_id;
    ((uint32_t*)m->data)[1] = 0;
    if (blob_len) __builtin_memcpy(m->data+8, blob, blob_len);
}

/* VERIFY response:
 *   data[0..7]   = nosm_capset_t (mod_id,caps)
 *   data[8]      = status (0=OK, else error)
 *   data[9..]    = optional reason string (NUL-terminated)
 */
typedef struct {
    uint8_t  status;     /* 0=OK */
    char     reason[63]; /* short reason */
} nosm_verify_status_t;

