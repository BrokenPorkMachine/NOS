#include "../../libc/libc.h"
#include "../../../kernel/IPC/ipc.h"
#include "../../../include/nosm_ipc.h"
#include "../../../nosm/drivers/IO/serial.h"

/* Very simple trust policy:
 * - Read __O2INFO manifest (JSON) from the blob
 * - Check "type":"nmod" and a "signature" == HMAC_SHA256(manifest,cfg_key) (stubbed)
 * - Translate "capabilities":[...] -> bitmap
 */

static uint64_t caps_from_manifest(const char *json) {
    uint64_t caps = 0;
    /* naive substring checks; replace with your JSON helper */
    if (strstr(json, "IOPORT"))   caps |= NOSM_CAP_IOPORT;
    if (strstr(json, "MMIO"))     caps |= NOSM_CAP_MMIO;
    if (strstr(json, "IRQ"))      caps |= NOSM_CAP_IRQ;
    if (strstr(json, "PCI_ENUM")) caps |= NOSM_CAP_PCI_ENUM;
    if (strstr(json, "NETSTACK")) caps |= NOSM_CAP_NETSTACK;
    if (strstr(json, "FS"))       caps |= NOSM_CAP_FS;
    return caps;
}

static int extract_manifest(const void *blob, uint32_t len, char *out, uint32_t outsz) {
    /* Your Mach-O2 manifests appear verbatim in the image; find first '{'..'}' pair. */
    const unsigned char *p = (const unsigned char*)blob;
    const unsigned char *s = NULL, *e = NULL;
    for (uint32_t i=0;i<len;i++) if (p[i]=='{'){ s=&p[i]; break; }
    if (!s) return -1;
    for (uint32_t i=(uint32_t)(s-p); i<len; i++) if (p[i]=='}'){ e=&p[i]; break; }
    if (!e || e<=s) return -1;
    uint32_t n = (uint32_t)(e - s + 2);
    if (n > outsz) return -1;
    memcpy(out, s, n-1);
    out[n-1] = 0;
    return 0;
}

void nosm_server(ipc_queue_t *q, uint32_t self_id) {
    (void)self_id;
    serial_puts("[nosm] security agent online\n");
    for (;;) {
        ipc_message_t m = {0};
        if (ipc_receive_blocking(q, self_id, &m) != 0) { thread_yield(); continue; }

        if (m.type == NOSM_IPC_HEALTH_PING) {
            ipc_message_t r = {0}; r.type = NOSM_IPC_HEALTH_PONG;
            ipc_send(q, self_id, &r); continue;
        }
        if (m.type != NOSM_IPC_VERIFY_REQ || m.len < 8) { continue; }

        uint32_t mod_id = ((uint32_t*)m.data)[0];
        const void *blob = m.data + 8;
        uint32_t blob_len = m.len - 8;

        ipc_message_t resp = {0};
        resp.type = NOSM_IPC_VERIFY_RESP;

        nosm_capset_t cs = { .mod_id = mod_id, .caps = 0 };
        nosm_verify_status_t st = { .status = 1 };
        strcpy(st.reason, "deny");

        char manifest[1024];
        if (extract_manifest(blob, blob_len, manifest, sizeof(manifest)) == 0) {
            /* Example guardrails:
             * - require type "nmod"
             * - require "name" and basic signature field present
             */
            if (strstr(manifest, "\"type\":\"nmod\"")) {
                /* TODO: verify signature/HMAC here; for now accept. */
                cs.caps = caps_from_manifest(manifest);
                st.status = 0;
                st.reason[0] = 0;
            } else {
                strcpy(st.reason, "type!=nmod");
            }
        } else {
            strcpy(st.reason, "manifest");
        }

        /* pack response */
        memcpy(resp.data, &cs, sizeof(cs));
        resp.data[8] = st.status;
        uint32_t off = 9;
        if (st.reason[0]) {
            size_t rlen = strlen(st.reason)+1;
            memcpy(resp.data+off, st.reason, rlen);
            off += (uint32_t)rlen;
        }
        resp.len = off;
        ipc_send(q, self_id, &resp);
    }
}

