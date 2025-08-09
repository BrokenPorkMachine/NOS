/*
 * init agent (standalone, gated by regx)
 *
 * Responsibilities:
 *   - Read a manifest (/agents/init.json) listing agents to launch
 *   - Ask regx (via NOS->regx_load) to load each agent
 *   - Basic logging and optional health pings
 *
 * NO kernel symbols, NO direct thread_* or serial_* usage.
 */

#include "../../rt/agent_abi.h"
#include "../../libc/libc.h"     /* fopen/fread/fclose, memset, memcpy, etc. */
#include <stdint.h>
#include <stddef.h>

/* ---------- Embedded manifest section for Mach-O2 style (optional) ---------- */
__attribute__((section("__O2INFO,__manifest")))
static const char mo2_manifest[] =
"{\n"
"  \"name\": \"init\",\n"
"  \"type\": \"service_launcher\",\n"
"  \"version\": \"1.0.0\",\n"
"  \"entry\": \"agent_main\"\n"
"}\n";

/* ---------- Simple JSON manifest parser ------------------------------------
   We expect a very small JSON like:

{
  "services": [
    { "path": "/agents/login.bin",  "args": "" },
    { "path": "/agents/pkg.bin",    "args": "" },
    { "path": "/agents/update.bin", "args": "" },
    { "path": "/agents/ssh.bin",    "args": "" },
    { "path": "/agents/ftp.bin",    "args": "" }
  ]
}

   The parser below is tolerant and only extracts "path" and "args" pairs.
---------------------------------------------------------------------------- */
typedef struct {
    char path[160];
    char args[160];
} svc_spec_t;

#define MAX_SERVICES 16

static int json_skip_ws(const char *s, int i) {
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n') i++;
    return i;
}

static int json_match(const char *s, int i, const char *kw) {
    int j = 0;
    while (kw[j] && s[i+j] == kw[j]) j++;
    return kw[j] ? 0 : 1;
}

static int json_copy_quoted(const char *s, int i, char *out, size_t out_sz) {
    i = json_skip_ws(s, i);
    if (s[i] != '"') return -1;
    i++;
    size_t w = 0;
    while (s[i] && s[i] != '"' && w + 1 < out_sz) {
        out[w++] = s[i++];
    }
    if (s[i] != '"') return -1;
    out[w] = 0;
    return i + 1;
}

static int parse_manifest_services(const char *buf, size_t len, svc_spec_t *list, int max_list) {
    int n = 0;
    int i = 0;
    (void)len;

    /* find "services" : [ ... ] */
    while (buf[i]) {
        i = json_skip_ws(buf, i);
        if (json_match(buf, i, "\"services\"")) {
            i += 10;
            i = json_skip_ws(buf, i);
            if (buf[i] != ':') break;
            i++;
            i = json_skip_ws(buf, i);
            if (buf[i] != '[') break;
            i++;

            /* parse array of { "path": "...", "args": "..." } */
            while (buf[i]) {
                i = json_skip_ws(buf, i);
                if (buf[i] == ']') return n; /* end array */

                if (buf[i] != '{') break;
                i++;

                char path[160] = {0}, args[160] = {0};
                int have_path = 0, have_args = 0;

                for (;;) {
                    i = json_skip_ws(buf, i);
                    if (buf[i] == '}') { i++; break; }

                    /* key */
                    char key[16] = {0};
                    int ni = json_copy_quoted(buf, i, key, sizeof(key));
                    if (ni < 0) return n;
                    i = json_skip_ws(buf, ni);
                    if (buf[i] != ':') return n;
                    i++;
                    i = json_skip_ws(buf, i);

                    /* value (string only) */
                    char val[160] = {0};
                    ni = json_copy_quoted(buf, i, val, sizeof(val));
                    if (ni < 0) return n;
                    i = ni;

                    if (!have_path && !strcmp(key, "path")) {
                        snprintf(path, sizeof(path), "%s", val);
                        have_path = 1;
                    } else if (!have_args && !strcmp(key, "args")) {
                        snprintf(args, sizeof(args), "%s", val);
                        have_args = 1;
                    }

                    i = json_skip_ws(buf, i);
                    if (buf[i] == ',') { i++; continue; }
                    if (buf[i] == '}') { i++; break; }
                }

                if (have_path && n < max_list) {
                    snprintf(list[n].path, sizeof(list[n].path), "%s", path);
                    snprintf(list[n].args, sizeof(list[n].args), "%s", args);
                    n++;
                }

                i = json_skip_ws(buf, i);
                if (buf[i] == ',') { i++; continue; }
                if (buf[i] == ']') return n;
            }
        }
        if (buf[i]) i++;
    }
    return n;
}

/* ---------- Load manifest from disk (optional) ----------------------------- */
static int load_manifest_from_disk(svc_spec_t *out, int max_out) {
    char *buf = NULL;
    size_t sz = 0;

    FILE *fp = fopen("/agents/init.json", "r");
    if (!fp) return 0;

    /* Read whole file */
    fseek(fp, 0, 2);
    long fsz = ftell(fp);
    if (fsz <= 0 || fsz > 64 * 1024) { fclose(fp); return 0; }
    fseek(fp, 0, 0);

    buf = (char *)malloc((size_t)fsz + 1);
    if (!buf) { fclose(fp); return 0; }

    sz = fread(buf, 1, (size_t)fsz, fp);
    fclose(fp);
    buf[sz] = 0;

    int n = parse_manifest_services(buf, sz, out, max_out);
    free(buf);
    return n;
}

/* ---------- Built-in fallback list ---------------------------------------- */
static int default_manifest(svc_spec_t *out, int max_out) {
    static const char *paths[] = {
        "/agents/pkg.bin",
        "/agents/update.bin",
        "/agents/login.bin",
        /* Optional agents (uncomment as they become available): */
        /* "/agents/ssh.bin", */
        /* "/agents/ftp.bin", */
        /* "/agents/vnc.bin", */
    };
    int n = 0;
    for (size_t i = 0; i < sizeof(paths)/sizeof(paths[0]) && n < max_out; ++i) {
        snprintf(out[n].path, sizeof(out[n].path), "%s", paths[i]);
        out[n].args[0] = 0;
        n++;
    }
    return n;
}

/* ---------- Agent main ----------------------------------------------------- */
__attribute__((noreturn))
void agent_main(void)
{
    if (!NOS) {
        /* no API? nothing to do */
        for (;;) {}
    }

    NOS->puts("[init] starting (user agent)\n");

    /* Load services list */
    svc_spec_t svcs[MAX_SERVICES];
    int count = load_manifest_from_disk(svcs, MAX_SERVICES);
    if (count <= 0) {
        NOS->puts("[init] /agents/init.json not found or empty; using defaults\n");
        count = default_manifest(svcs, MAX_SERVICES);
    }

    /* Try a quick handshake with regx (optional) */
    if (NOS->regx_ping && !NOS->regx_ping()) {
        NOS->puts("[init] WARNING: regx did not answer ping, proceeding anyway\n");
    }

    /* Ask regx to load each service */
    for (int i = 0; i < count; ++i) {
        uint32_t tid = 0;
        int rc = -1;

        if (NOS->regx_load) {
            rc = NOS->regx_load(svcs[i].path, svcs[i].args[0] ? svcs[i].args : NULL, &tid);
        }

        if (rc == 0) {
            if (tid)
                NOS->printf("[init] launched '%s' tid=%u\n", svcs[i].path, tid);
            else
                NOS->printf("[init] launched '%s'\n", svcs[i].path);
        } else {
            NOS->printf("[init] FAILED to launch '%s' (rc=%d)\n", svcs[i].path, rc);
        }

        if (NOS->yield) NOS->yield();
    }

    NOS->puts("[init] bootstrap complete; entering monitor loop\n");

    /* Very light monitor loop: just yield; future: poll health, reload manifest, etc. */
    for (;;) {
        if (NOS->yield) NOS->yield();
        /* sleeping spin */
        for (volatile int i = 0; i < 200000; ++i) __asm__ __volatile__("pause");
    }
}
