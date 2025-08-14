#include "../../libc/libc.h"
#include "../../../kernel/IPC/ipc.h"
#include "../../../include/nosm_ipc.h"
#include "../../../nosm/drivers/IO/serial.h"
#include <stdint.h>

/* --- Minimal SHA-256 + HMAC implementation (public domain) ---------------- */

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  data[64];
    uint32_t datalen;
} sha256_ctx_t;

static uint32_t rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static void sha256_init(sha256_ctx_t *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t data[]) {
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t m[64];
    uint32_t a,b,c,d,e,f,g,h,i,j,t1,t2;

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j+1] << 16) | (data[j+2] << 8) | (data[j+3]);
    for (; i < 64; ++i)
        m[i] = (rotr32(m[i-2],17) ^ rotr32(m[i-2],19) ^ (m[i-2] >> 10)) + m[i-7] +
               (rotr32(m[i-15],7) ^ rotr32(m[i-15],18) ^ (m[i-15] >> 3)) + m[i-16];

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + (rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25)) + ((e & f) ^ (~e & g)) + k[i] + m[i];
        t2 = (rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t hash[32]) {
    uint32_t i = ctx->datalen;

    /* Pad whatever data is left in the buffer. */
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}

static void hmac_sha256(const uint8_t *key, size_t keylen,
                        const uint8_t *data, size_t datalen,
                        uint8_t out[32]) {
    uint8_t k_ipad[64], k_opad[64], tk[32];
    if (keylen > 64) {
        sha256_ctx_t tctx;
        sha256_init(&tctx);
        sha256_update(&tctx, key, keylen);
        sha256_final(&tctx, tk);
        key = tk;
        keylen = 32;
    }
    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5c, 64);
    for (size_t i=0;i<keylen;i++) {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, k_ipad, 64);
    sha256_update(&ctx, data, datalen);
    sha256_final(&ctx, tk);

    sha256_init(&ctx);
    sha256_update(&ctx, k_opad, 64);
    sha256_update(&ctx, tk, 32);
    sha256_final(&ctx, out);
}

/* ------------------------------------------------------------------------- */

static const uint8_t system_key[] = {
    'n','o','s','m','_','s','y','s','_','k','e','y'
};
static const uint8_t user_key[] = {
    'n','o','s','m','_','u','s','r','_','k','e','y'
};

/* Very simple trust policy:
 * - Read __O2INFO manifest (JSON) from the blob
 * - Check "type":"nmod"
 * - Verify HMAC_SHA256(manifest,key) where key is based on "system":true
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

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int verify_signature(const char *manifest) {
    const char *sig = strstr(manifest, "\"signature\":\"");
    if (!sig) return -1;
    sig += strlen("\"signature\":\"");
    uint8_t expected[32];
    for (int i = 0; i < 32; ++i) {
        int hi = hexval(sig[i*2]);
        int lo = hexval(sig[i*2+1]);
        if (hi < 0 || lo < 0) return -1;
        expected[i] = (uint8_t)((hi << 4) | lo);
    }
    if (sig[64] != '"') return -1;
    const char *sig_end = sig + 64;
    size_t prefix = (size_t)(sig - strlen("\"signature\":\"") - manifest);
    const char *after = sig_end + 1;
    size_t suffix = strlen(after);
    char buf[1024];
    if (prefix + suffix >= sizeof(buf)) return -1;
    memcpy(buf, manifest, prefix);
    memcpy(buf + prefix, after, suffix + 1); /* include NUL */
    int is_system = strstr(manifest, "\"system\":true") != NULL;
    const uint8_t *key = is_system ? system_key : user_key;
    size_t keylen = is_system ? sizeof(system_key) : sizeof(user_key);
    uint8_t digest[32];
    hmac_sha256(key, keylen, (const uint8_t*)buf, strlen(buf), digest);
    return memcmp(digest, expected, 32) == 0 ? 0 : -1;
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
        if (ipc_receive_blocking(q, self_id, &m) != 0)
            continue;

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
                if (verify_signature(manifest) == 0) {
                    cs.caps = caps_from_manifest(manifest);
                    st.status = 0;
                    st.reason[0] = 0;
                } else {
                    strcpy(st.reason, "sig");
                }
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

