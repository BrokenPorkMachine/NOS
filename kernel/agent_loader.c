// kernel/agent_loader.c
#include "agent_loader.h"
#include "agent.h"
#include "Task/thread.h"
#include "../../user/libc/libc.h"
#include "symbols.h"

#include <stdint.h>
#include <string.h>   // memcmp, memchr, strstr, strlen, strcmp, snprintf
#include <limits.h>   // UINT32_MAX
#include <nosm.h>
#include <elf.h>

extern int kprintf(const char *fmt, ...);

/* ------------------------------------------------------------------------- */
/*                              Local plumbing                               */
/* ------------------------------------------------------------------------- */

typedef int  (*agent_read_file_fn)(const char *path, void **out, size_t *out_sz);
typedef void (*agent_free_fn)(void *ptr);

// Security gate: regx can install this to allow/deny loads
typedef int (*agent_gate_fn)(const char *path,
                             const char *name,
                             const char *version,
                             const char *capabilities,
                             const char *entry);

static agent_read_file_fn g_read_file = 0;
static agent_free_fn      g_free_fn   = 0;
static agent_gate_fn      g_gate_fn   = 0;

void agent_loader_set_read(agent_read_file_fn reader, agent_free_fn freer){ g_read_file=reader; g_free_fn=freer; }
void agent_loader_set_gate(agent_gate_fn gate){ g_gate_fn = gate; }

/* ------------------------------------------------------------------------- */
/*                                  memmem                                   */
/* ------------------------------------------------------------------------- */

static const void *memmem_local(const void *hay, size_t haylen,
                                const void *need, size_t needlen)
{
    const unsigned char *h = (const unsigned char*)hay;
    const unsigned char *n = (const unsigned char*)need;

    if (needlen == 0)
        return hay;

    for (size_t i = 0; i + needlen <= haylen; ++i)
        if (memcmp(h + i, n, needlen) == 0)
            return h + i;

    return NULL;
}

/* ------------------------------------------------------------------------- */
/*                             Minimal JSON helpers                          */
/* ------------------------------------------------------------------------- */

static const char *skip_ws(const char *p){
    while(*p==' '||*p=='\t'||*p=='\n'||*p=='\r') p++;
    return p;
}

static int json_extract_string(const char *json, const char *key, char *out, size_t out_sz){
    if (!json || !key || !out || out_sz == 0) return -1;
    char pattern[64];
    snprintf(pattern,sizeof(pattern),"\"%s\"",key);
    const char *p = strstr(json,pattern);
    if(!p) return -1;
    p += strlen(pattern);
    p = skip_ws(p);
    if(*p!=':') return -1;
    p++;
    p = skip_ws(p);
    if(*p!='"') return -1;
    p++;
    size_t i=0;
    while(*p && *p!='"' && i<out_sz-1) out[i++]=*p++;
    if(*p!='"') return -1;
    out[i]=0;
    return 0;
}

static int json_extract_int(const char *json, const char *key){
    if (!json || !key) return -1;
    char pattern[64];
    snprintf(pattern,sizeof(pattern),"\"%s\"",key);
    const char *p = strstr(json,pattern);
    if(!p) return -1;
    p += strlen(pattern);
    p = skip_ws(p);
    if(*p!=':') return -1;
    p++;
    p = skip_ws(p);
    return (int)strtol(p,NULL,10);
}

/* ------------------------------------------------------------------------- */
/*                              Symbol registry                              */
/* ------------------------------------------------------------------------- */

#define MAX_ENTRIES 32
static struct { const char *name; agent_entry_t fn; } entry_registry[MAX_ENTRIES];
static size_t entry_count=0;

void agent_loader_register_entry(const char *name, agent_entry_t fn){
    if(!name||!fn) return;
    for(size_t i=0;i<entry_count;++i)
        if(strcmp(entry_registry[i].name,name)==0) return;
    if(entry_count<MAX_ENTRIES){
        entry_registry[entry_count].name=name;
        entry_registry[entry_count].fn=fn;
        entry_count++;
    }
}

static agent_entry_t find_entry_fn(const char *name){
    for(size_t i=0;i<entry_count;++i)
        if(strcmp(entry_registry[i].name,name)==0) return entry_registry[i].fn;
    return NULL;
}

/* ------------------------------------------------------------------------- */
/*                             Format detection                              */
/* ------------------------------------------------------------------------- */

agent_format_t detect_agent_format(const void *image, size_t size){
    const unsigned char *d=(const unsigned char*)image;
    if(!d || size==0) return AGENT_FORMAT_UNKNOWN;

    if(size>=4 && memcmp(d,"\x7F""ELF",4)==0) return AGENT_FORMAT_ELF;

    if(size>=4 && ((d[0]==0xCF&&d[1]==0xFA&&d[2]==0xED&&d[3]==0xFE) || (d[0]==0xFE&&d[1]==0xED&&d[2]==0xFA&&d[3]==0xCF))){
        if(memmem_local(d,size,"__O2INFO",8)) return AGENT_FORMAT_MACHO2;
        return AGENT_FORMAT_MACHO;
    }

    // NOSM: we don't parse headers; treat it as an opaque "NOSM" blob if it begins with the tag
    if(size>=4 && memcmp(d,"NOSM",4)==0) return AGENT_FORMAT_NOSM;

    // Heuristic: if it looks like a JSON manifest is embedded as top-level, treat as MACHO2
    if(size>0 && ((const char*)d)[0]=='{') return AGENT_FORMAT_MACHO2;

    return AGENT_FORMAT_FLAT;
}

/* ------------------------------------------------------------------------- */
/*                          Manifest extractors (JSON)                       */
/* ------------------------------------------------------------------------- */

int extract_manifest_macho2(const void *image,size_t size,char *out_json,size_t out_sz){
    if(!image || !out_json || out_sz==0) return -1;
    const unsigned char *d=(const unsigned char*)image;
    const char *s=(const char*)memchr(d,'{',size);
    if(!s) return -1;
    size_t remain = size - (size_t)(s - (const char*)d);
    const char *e=(const char*)memchr(s,'}',remain);
    if(!e || e<=s || (size_t)(e-s+2)>out_sz) return -1;
    memcpy(out_json,s,(size_t)(e-s+1));
    out_json[e-s+1]=0;
    return 0;
}

int extract_manifest_elf(const void *image,size_t size,char *out_json,size_t out_sz){
    if(!image || !out_json || out_sz==0) return -1;
    const unsigned char *d=(const unsigned char*)image;
    const char *s=(const char*)memchr(d,'{',size);
    if(!s) return -1;
    size_t remain = size - (size_t)(s - (const char*)d);
    const char *e=(const char*)memchr(s,'}',remain);
    if(!e || e<=s || (size_t)(e-s+2)>out_sz) return -1;
    memcpy(out_json,s,(size_t)(e-s+1));
    out_json[e-s+1]=0;
    return 0;
}

/* ------------------------------------------------------------------------- */
/*                  Register + spawn (with security gate)                    */
/* ------------------------------------------------------------------------- */

static int register_and_spawn_from_manifest(const char *json, const char *path, int prio){
    regx_manifest_t m=(regx_manifest_t){0};
    char entry[64]={0};

    (void)path;

    json_extract_string(json,"name",m.name,sizeof(m.name));
    m.type=json_extract_int(json,"type");
    json_extract_string(json,"version",m.version,sizeof(m.version));
    json_extract_string(json,"abi",m.abi,sizeof(m.abi));
    json_extract_string(json,"capabilities",m.capabilities,sizeof(m.capabilities));
    json_extract_string(json,"entry",entry,sizeof(entry));

    // Security gate decision
    if (g_gate_fn){
        int ok = g_gate_fn(path ? path : "(buffer)", m.name, m.version, m.capabilities, entry);
        if (!ok){
            kprintf("[loader] gate denied \"%s\" (entry=%s caps=%s)\n",
                    m.name[0]?m.name:"(unnamed)", entry, m.capabilities);
            return -1;
        }
    }

    uint64_t id=regx_register(&m,0);
    if(!id){
        kprintf("[loader] regx_register failed for \"%s\"\n",
                m.name[0]?m.name:"(unnamed)");
        return -1;
    }

    agent_entry_t fn=find_entry_fn(entry);
    if(!fn){
        kprintf("[loader] entry \"%s\" not found for \"%s\"\n",
                entry, m.name[0]?m.name:"(unnamed)");
        return -1;
    }

    n2_agent_t agent=(n2_agent_t){0};
    snprintf(agent.name,sizeof(agent.name),"%s",m.name);
    snprintf(agent.version,sizeof(agent.version),"%s",m.version);
    snprintf(agent.capabilities,sizeof(agent.capabilities),"%s",m.capabilities);
    agent.entry=fn;
    agent.manifest=NULL;
    n2_agent_register(&agent);

    thread_t *t=thread_create_with_priority((void(*)(void))fn, prio>0?prio:200);
    if(!t){
        kprintf("[loader] thread create failed \"%s\"\n", m.name);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/*                    Internal per-format implementations                    */
/* ------------------------------------------------------------------------- */

static int load_agent_macho2_impl(const void *image,size_t size,const char *path,int prio){
    (void)path;
    char manifest[512];
    if (extract_manifest_macho2(image,size,manifest,sizeof(manifest))==0){
        char name[64]={0};
        json_extract_string(manifest,"name",name,sizeof(name));
        symbols_add(name[0]?name:"(anon)",(uintptr_t)image,size);
        return register_and_spawn_from_manifest(manifest, path, prio);
    }
    return -1;
}

static int load_agent_elf_impl(const void *image,size_t size,const char *path,int prio){
    if (!image || size < sizeof(Elf64_Ehdr))
        return -1;

    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)image;
    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0 || eh->e_ident[EI_CLASS] != ELFCLASS64)
        return -1;
    if (eh->e_phoff == 0 || eh->e_phnum == 0)
        return -1;

    const Elf64_Phdr *ph = (const Elf64_Phdr *)((const uint8_t *)image + eh->e_phoff);
    uint64_t min_vaddr = UINT64_MAX, max_vaddr = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD)
            continue;
        if (ph[i].p_vaddr < min_vaddr)
            min_vaddr = ph[i].p_vaddr;
        uint64_t end = ph[i].p_vaddr + ph[i].p_memsz;
        if (end > max_vaddr)
            max_vaddr = end;
    }
    if (min_vaddr == UINT64_MAX || max_vaddr <= min_vaddr)
        return -1;

    size_t memsz = (size_t)(max_vaddr - min_vaddr);
    uint8_t *mem = (uint8_t *)malloc(memsz);
    if (!mem)
        return -1;
    memset(mem, 0, memsz);

    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD)
            continue;
        if (ph[i].p_offset + ph[i].p_filesz > size) {
            free(mem);
            return -1;
        }
        memcpy(mem + (ph[i].p_vaddr - min_vaddr),
               (const uint8_t *)image + ph[i].p_offset,
               ph[i].p_filesz);
    }

    uint64_t entry = (uint64_t)(mem + (eh->e_entry - min_vaddr));

    char manifest[512];
    if (extract_manifest_elf(image, size, manifest, sizeof(manifest)) != 0) {
        free(mem);
        return -1;
    }

    char entry_name[64] = {0};
    json_extract_string(manifest, "entry", entry_name, sizeof(entry_name));
    if (!entry_name[0])
        snprintf(entry_name, sizeof(entry_name), "agent_main");

    agent_loader_register_entry(entry_name, (agent_entry_t)entry);
    symbols_add(entry_name, (uintptr_t)mem, memsz);
    return register_and_spawn_from_manifest(manifest, path, prio);
}

static int load_agent_macho_impl(const void *image,size_t size,const char *path,int prio){
    (void)image;(void)size;(void)path;(void)prio;
    kprintf("[loader] Mach-O not implemented\n");
    return -1;
}

static int load_agent_flat_impl(const void *image, size_t size, const char *path, int prio){
    if (!image || size == 0)
        return -1;

    /* derive name from path: basename without extension */
    char name[32] = {0};
    if (path){
        const char *base = path;
        for (const char *p = path; *p; ++p)
            if (*p == '/') base = p + 1;
        snprintf(name, sizeof(name), "%s", base);
        char *dot = NULL;
        for (char *p = name; *p; ++p)
            if (*p == '.') dot = p;
        if (dot) *dot = 0;
    } else {
        snprintf(name, sizeof(name), "flat");
    }

    /* Security gate decision with placeholder metadata */
    if (g_gate_fn){
        int ok = g_gate_fn(path ? path : "(buffer)", name, "0", "", "agent_main");
        if (!ok)
            return -1;
    }

    /* Register entry and fabricate minimal manifest */
    agent_loader_register_entry("agent_main", (agent_entry_t)image);
    symbols_add(name, (uintptr_t)image, size);
    char manifest[128];
    snprintf(manifest, sizeof(manifest),
             "{\"name\":\"%s\",\"type\":4,\"version\":\"0\",\"entry\":\"agent_main\",\"capabilities\":\"\"}",
             name);

    return register_and_spawn_from_manifest(manifest, path, prio);
}

static int load_agent_nosm_impl(const void *image, size_t size, const char *path, int prio)
{
    (void)prio;

    if (!image || size == 0 || size > UINT32_MAX)
        return -1;

    // No explicit manifest with this ABI; gate using placeholders.
    if (g_gate_fn) {
        int ok = g_gate_fn(path ? path : "(buffer)", "(nosm)", "(unknown)", "", "nosm");
        if (!ok) {
            kprintf("[loader] gate denied %s\n", path ? path : "(buffer)");
            return -1;
        }
    }

    uint32_t mod_id = 0;
    int rc = nosm_request_verify_and_load(image, (uint32_t)size, &mod_id);
    if (rc != 0) {
        kprintf("[loader] nosm_request_verify_and_load failed (rc=%d) for %s\n",
                rc, path ? path : "(buffer)");
        return -1;
    }

    symbols_add(path ? path : "(nosm)", (uintptr_t)image, size);

    // If you need to unload later:
    // nosm_unload(mod_id);

    return 0;
}

/* ------------------------------------------------------------------------- */
/*                         Public API (header-matching)                      */
/* ------------------------------------------------------------------------- */

int load_agent_auto_with_prio(const void *image,size_t size,int prio){
    // Prefer O2-style; detection/fallback is handled below
    (void)prio;
    return load_agent_macho2_impl(image,size,"(buffer)",prio);
}

int load_agent_with_prio(const void *image,size_t size,agent_format_t fmt,int prio){
    switch(fmt){
        case AGENT_FORMAT_MACHO2: return load_agent_macho2_impl(image,size,"(buffer)",prio);
        case AGENT_FORMAT_MACHO:  return load_agent_macho_impl (image,size,"(buffer)",prio);
        case AGENT_FORMAT_ELF:    return load_agent_elf_impl   (image,size,"(buffer)",prio);
        case AGENT_FORMAT_FLAT:   return load_agent_flat_impl  (image,size,"(buffer)",prio);
        case AGENT_FORMAT_NOSM:   return load_agent_nosm_impl  (image,size,"(buffer)",prio);
        default:                  return load_agent_auto_with_prio(image,size,prio);
    }
}

int load_agent_auto(const void *image,size_t size){
    return load_agent_auto_with_prio(image,size,200);
}

int load_agent(const void *image,size_t size,agent_format_t fmt){
    return load_agent_with_prio(image,size,fmt,200);
}

int load_agent_nosm(const void *image,size_t size){
    return load_agent_nosm_impl(image,size,"(buffer)",200);
}

/* ------------------------------------------------------------------------- */
/*                       Filesystem-based convenience                        */
/* ------------------------------------------------------------------------- */

int agent_loader_run_from_path(const char *path,int prio){
    if(!g_read_file){
        kprintf("[loader] no FS reader; cannot load \"%s\"\n", path?path:"(null)");
        return -1;
    }
    void *buf=0; size_t sz=0;
    int rc=g_read_file(path,&buf,&sz);
    if(rc<0){
        kprintf("[loader] read failed \"%s\"\n", path?path:"(null)");
        return rc;
    }

    switch (detect_agent_format(buf, sz)){
        case AGENT_FORMAT_MACHO2: rc=load_agent_macho2_impl(buf,sz,path,prio); break;
        case AGENT_FORMAT_MACHO:  rc=load_agent_macho_impl (buf,sz,path,prio); break;
        case AGENT_FORMAT_ELF:    rc=load_agent_elf_impl   (buf,sz,path,prio); break;
        case AGENT_FORMAT_FLAT:   rc=load_agent_flat_impl  (buf,sz,path,prio); break;
        case AGENT_FORMAT_NOSM:   rc=load_agent_nosm_impl  (buf,sz,path,prio); break;
        default:                  rc=-1; break;
    }

    if(g_free_fn && buf) g_free_fn(buf);
    return rc;
}
