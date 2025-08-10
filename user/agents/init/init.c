/*
 * init agent (standalone, gated by regx)
 * - NO kernel headers
 * - NO stdio / thread_* / serial_* deps
 * - Uses AgentAPI passed by regx via agent_entry.c (api + self_tid)
 */

#include "../../rt/agent_abi.h"
#include "../../libc/libc.h"   /* memset, memcpy, strcmp, snprintf, malloc/free */
#include <stdint.h>
#include <stddef.h>

/* Optional embedded manifest for Mach-O2 discovery (won’t be referenced in code) */
__attribute__((used, section("\"__O2INFO,__manifest\"")))
static const char mo2_manifest[] =
"{\n"
"  \"name\": \"init\",\n"
"  \"type\": 4,\n"
"  \"version\": \"1.0.0\",\n"
"  \"entry\": \"agent_main\"\n"
"}\n";

/* ---------- minimal JSON helpers (string-only) ---------- */
typedef struct {
    char path[160];
    char args[160];
} svc_spec_t;

#define MAX_SERVICES 16

static int json_skip_ws(const char *s, int i) {
    while (s[i]==' '||s[i]=='\t'||s[i]=='\r'||s[i]=='\n') i++;
    return i;
}
static int json_match(const char *s, int i, const char *kw) {
    int j=0; while (kw[j] && s[i+j]==kw[j]) j++; return kw[j]?0:1;
}
static int json_copy_quoted(const char *s, int i, char *out, size_t out_sz) {
    i = json_skip_ws(s,i);
    if (s[i]!='"') return -1;
    i++;
    size_t w=0;
    while (s[i] && s[i]!='"' && w+1<out_sz) out[w++]=s[i++];
    if (s[i]!='"') return -1;
    out[w]=0;
    return i+1;
}
static int parse_manifest_services(const char *buf, size_t len,
                                   svc_spec_t *list, int max_list) {
    (void)len;
    int n=0, i=0;
    while (buf[i]) {
        i = json_skip_ws(buf,i);
        if (json_match(buf,i,"\"services\"")) {
            i+=10;
            i = json_skip_ws(buf,i);
            if (buf[i++]!=':') break;
            i = json_skip_ws(buf,i);
            if (buf[i++]!='[') break;

            while (buf[i]) {
                i = json_skip_ws(buf,i);
                if (buf[i]==']') return n;
                if (buf[i++]!='{') break;

                char path[160]={0}, args[160]={0};
                int have_path=0, have_args=0;

                for (;;) {
                    i = json_skip_ws(buf,i);
                    if (buf[i]=='}') { i++; break; }

                    char key[16]={0}, val[160]={0};
                    int ni = json_copy_quoted(buf,i,key,sizeof(key));
                    if (ni<0) return n;
                    i = json_skip_ws(buf,ni);
                    if (buf[i++]!=':') return n;
                    i = json_skip_ws(buf,i);
                    ni = json_copy_quoted(buf,i,val,sizeof(val));
                    if (ni<0) return n;
                    i = ni;

                    if (!have_path && !strcmp(key,"path")) {
                        snprintf(path,sizeof(path),"%s",val); have_path=1;
                    } else if (!have_args && !strcmp(key,"args")) {
                        snprintf(args,sizeof(args),"%s",val); have_args=1;
                    }

                    i = json_skip_ws(buf,i);
                    if (buf[i]==',') { i++; continue; }
                    if (buf[i]=='}') { i++; break; }
                }

                if (have_path && n<max_list) {
                    snprintf(list[n].path,sizeof(list[n].path),"%s",path);
                    snprintf(list[n].args,sizeof(list[n].args),"%s",args);
                    n++;
                }

                i = json_skip_ws(buf,i);
                if (buf[i]==',') { i++; continue; }
                if (buf[i]==']') return n;
            }
        }
        if (buf[i]) i++;
    }
    return n;
}

/* ---------- manifest loading via AgentAPI ---------- */
static int load_manifest_from_disk(const AgentAPI *api,
                                   svc_spec_t *out, int max_out) {
    if (!api || !api->fs_read_all) return 0;

    /* Read file into a temporary buffer */
    size_t got = 0;
    /* read up to 64 KiB */
    size_t cap = 64*1024;
    char *buf = (char*)malloc(cap+1);
    if (!buf) return 0;

    int rc = api->fs_read_all("/agents/init.json", buf, cap, &got);
    if (rc!=0 || got==0) { free(buf); return 0; }
    buf[got]=0;

    int n = parse_manifest_services(buf, got, out, max_out);
    free(buf);
    return n;
}

/* ---------- fallback defaults ---------- */
static int default_manifest(svc_spec_t *out, int max_out) {
    static const char *paths[] = {
        "/agents/pkg.bin",
        "/agents/update.bin",
        "/agents/login.bin",
        /* add more as they’re ready:
           "/agents/ssh.bin",
           "/agents/ftp.bin",
           "/agents/vnc.bin",
        */
    };
    int n=0;
    for (size_t i=0; i<sizeof(paths)/sizeof(paths[0]) && n<max_out; ++i) {
        snprintf(out[n].path,sizeof(out[n].path),"%s",paths[i]);
        out[n].args[0]=0;
        n++;
    }
    return n;
}

/* ---------- public entry for agent (called by agent_entry.c) ---------- */
void init_main(const AgentAPI *api, uint32_t self_tid)
{
    (void)self_tid;
    if (!api) return;

    if (api->puts)   api->puts("[init] starting\n");
    if (api->regx_ping && !api->regx_ping())
        if (api->puts) api->puts("[init] WARNING: regx did not answer ping\n");

    svc_spec_t svcs[MAX_SERVICES];
    int count = load_manifest_from_disk(api, svcs, MAX_SERVICES);
    if (count<=0) {
        if (api->puts) api->puts("[init] /agents/init.json missing; using defaults\n");
        count = default_manifest(svcs, MAX_SERVICES);
    }

    for (int i=0; i<count; ++i) {
        uint32_t tid = 0;
        int rc = api->regx_load ? api->regx_load(svcs[i].path,
                        svcs[i].args[0]?svcs[i].args:NULL, &tid) : -1;
        if (rc==0) {
            if (api->printf) api->printf("[init] launched %s tid=%u\n", svcs[i].path, tid);
        } else {
            if (api->printf) api->printf("[init] FAILED %s rc=%d\n", svcs[i].path, rc);
        }
        if (api->yield) api->yield();
    }

    if (api->puts) api->puts("[init] bootstrap complete\n");

    /* Light monitor loop */
    for (;;) {
        if (api->yield) api->yield();
        for (volatile int i=0;i<200000;i++) __asm__ __volatile__("pause");
    }
}
