// ============================================================================
// loader/dyld2.c  -- minimal, allocator + parser + relocator
// ============================================================================
#include "dyld2.h"
#include "mo2_format.h"
#include "../user/libc/libc.h"   // malloc/free/memcpy/memset/strcmp/snprintf
#include "../user/rt/agent_abi.h"
#include <stdint.h>
#include <string.h>

typedef struct ExportKV {
    const char* name;
    void*       addr;
    struct ExportKV* next;
} ExportKV;

typedef struct Module {
    char      name[96];

    /* Raw file image (+ size) for the .mo2 */
    uint8_t*  image;
    size_t    image_sz;

    /* Parsed headers */
    mo2_hdr_t   hdr;
    mo2_sect_t  sects[MO2_MAX_SECTS];

    /* Flat in-memory layout (single malloc): [text|ro|data|bss|got|plt] */
    uint8_t* base;
    uint8_t* text;  size_t text_sz;
    uint8_t* ro;    size_t ro_sz;
    uint8_t* data;  size_t data_sz;
    uint8_t* bss;   size_t bss_sz;
    uint8_t* plt;   size_t plt_sz;
    uint8_t* got;   size_t got_sz;

    /* Dyn/relocs */
    mo2_dyninfo_t dyn;
    mo2_reloc_t*  relocs; size_t n_reloc;

    /* Symbols/strings */
    mo2_sym_t*    sym; size_t nsym;
    const char*   str; size_t str_sz;

    /* Simple export table (hashed) */
    ExportKV* exports[128];

    /* Deps (not yet wired) */
    struct Module** deps; size_t ndep;

    int refcnt;
} Module;

static const AgentAPI* G = 0;

#define MAX_MODULES 64
static Module* g_modules[MAX_MODULES];
static size_t  g_module_count = 0;

#define LOG(msg)           do { if (G && G->puts)   G->puts(msg); } while (0)
#define LOGF(fmt, ...)     do { if (G && G->printf) G->printf(fmt, __VA_ARGS__); } while (0)

/* ----------------------------------------------------------------------------
 * Small utils
 * --------------------------------------------------------------------------*/
static void register_module(Module* m){
    if (g_module_count < MAX_MODULES) g_modules[g_module_count++] = m;
}

static void unregister_module(Module* m){
    for (size_t i = 0; i < g_module_count; i++){
        if (g_modules[i] == m){
            g_modules[i] = g_modules[--g_module_count];
            break;
        }
    }
}

static uint32_t djb2(const char* s){
    uint32_t h = 5381; int c;
    while ((c = *s++)) h = ((h << 5) + h) ^ (uint32_t)c;
    return h;
}

static void exports_insert(Module* m, const char* name, void* addr){
    uint32_t h = djb2(name) & 127;
    ExportKV* e = (ExportKV*)malloc(sizeof(ExportKV));
    if (!e) return;
    e->name = name; e->addr = addr; e->next = m->exports[h]; m->exports[h] = e;
}

static void* exports_find(Module* m, const char* name){
    uint32_t h = djb2(name) & 127;
    for (ExportKV* e = m->exports[h]; e; e = e->next){
        if (!strcmp(e->name, name)) return e->addr;
    }
    return 0;
}

/* ----------------------------------------------------------------------------
 * File I/O via AgentAPI
 * --------------------------------------------------------------------------*/
static int read_entire(const char* path, uint8_t** out, size_t* out_sz){
    if (!G || !out || !out_sz) return -1;
    if (!G->fs_read_all){
        LOG("[dyld2] fs_read_all not available\n");
        return -1;
    }
    const size_t cap = 256 * 1024;  // 256 KiB cap for now
    size_t got = 0;
    uint8_t* buf = (uint8_t*)malloc(cap + 1);
    if (!buf) return -1;
    int rc = G->fs_read_all(path, (char*)buf, cap, &got);
    if (rc != 0 || got == 0){
        free(buf);
        LOGF("[dyld2] fs_read_all failed rc=%d path=%s\n", rc, path ? path : "(null)");
        return -1;
    }
    buf[got] = 0;
    *out = buf;
    *out_sz = got;
    return 0;
}

/* ----------------------------------------------------------------------------
 * Parse & classify
 * --------------------------------------------------------------------------*/
static int parse_headers(Module* m){
    if (m->image_sz < sizeof(mo2_hdr_t)) return -1;

    memcpy(&m->hdr, m->image, sizeof(mo2_hdr_t));
    if (m->hdr.magic != MO2_MAGIC) return -1;
    if (m->hdr.nsects > MO2_MAX_SECTS) return -1;

    if (m->hdr.off_sections + m->hdr.nsects * sizeof(mo2_sect_t) > m->image_sz) return -1;
    memcpy(m->sects, m->image + m->hdr.off_sections, m->hdr.nsects * sizeof(mo2_sect_t));

    if (m->hdr.off_sym && m->hdr.nsym){
        if (m->hdr.off_sym + m->hdr.nsym * sizeof(mo2_sym_t) > m->image_sz) return -1;
        m->sym = (mo2_sym_t*)(m->image + m->hdr.off_sym);
        m->nsym = m->hdr.nsym;
    }

    if (m->hdr.off_str && m->hdr.str_sz){
        if (m->hdr.off_str + m->hdr.str_sz > m->image_sz) return -1;
        m->str = (const char*)(m->image + m->hdr.off_str);
        m->str_sz = m->hdr.str_sz;
    }

    if (m->hdr.off_dyn){
        if (m->hdr.off_dyn + sizeof(mo2_dyninfo_t) > m->image_sz) return -1;
        memcpy(&m->dyn, m->image + m->hdr.off_dyn, sizeof(mo2_dyninfo_t));

        if (m->dyn.off_reloc && m->dyn.n_reloc){
            if (m->dyn.off_reloc + m->dyn.n_reloc * sizeof(mo2_reloc_t) > m->image_sz) return -1;
            m->relocs = (mo2_reloc_t*)(m->image + m->dyn.off_reloc);
            m->n_reloc = m->dyn.n_reloc;
        }
    }

    return 0;
}

static void classify_sections(Module* m){
    m->text = m->ro = m->data = m->bss = m->plt = m->got = 0;
    m->text_sz = m->ro_sz = m->data_sz = m->bss_sz = m->plt_sz = m->got_sz = 0;

    for (uint32_t i = 0; i < m->hdr.nsects; i++){
        mo2_sect_t* s = &m->sects[i];
        switch (s->kind){
            case MO2_SECT_TEXT:   m->text = m->image + s->foffset; m->text_sz = (size_t)s->fsize; break;
            case MO2_SECT_RODATA: m->ro   = m->image + s->foffset; m->ro_sz   = (size_t)s->fsize; break;
            case MO2_SECT_DATA:   m->data = m->image + s->foffset; m->data_sz = (size_t)s->fsize; break;
            case MO2_SECT_BSS:    m->bss_sz = (size_t)s->vsize; break;
            case MO2_SECT_PLT:    m->plt  = m->image + s->foffset; m->plt_sz  = (size_t)s->fsize; break;
            case MO2_SECT_GOT:    m->got  = m->image + s->foffset; m->got_sz  = (size_t)s->fsize; break;
            default: break;
        }
    }
}

/* ----------------------------------------------------------------------------
 * Flat allocation & addressing
 * --------------------------------------------------------------------------*/
static void* alloc_segs(size_t text, size_t ro, size_t data, size_t bss,
                        size_t got, size_t plt){
    size_t total = text + ro + data + bss + got + plt + 64; // small pad
    uint8_t* base = (uint8_t*)malloc(total);
    if (!base) return 0;
    memset(base, 0, total);
    return base;
}

static void* sect_ptr(Module* m, uint32_t kind, uint32_t off){
    switch (kind){
        case MO2_SECT_TEXT:   return m->base + off; // [text]
        case MO2_SECT_RODATA: return m->base + m->text_sz + off; // [text|ro]
        case MO2_SECT_DATA:   return m->base + m->text_sz + m->ro_sz + off; // [text|ro|data]
        case MO2_SECT_BSS:    return m->base + m->text_sz + m->ro_sz + m->data_sz + off; // +bss
        case MO2_SECT_GOT:    return m->base + m->text_sz + m->ro_sz + m->data_sz + m->bss_sz + off; // +got
        case MO2_SECT_PLT:    return m->base + m->text_sz + m->ro_sz + m->data_sz + m->bss_sz + m->got_sz + off; // +plt
    }
    return 0;
}

/* ----------------------------------------------------------------------------
 * Relocation
 * --------------------------------------------------------------------------*/
static int relocate_one(Module* m, mo2_reloc_t* r, uintptr_t S){
    (void)m;
    uint8_t* P = (uint8_t*)sect_ptr(m, r->sect_id, r->r_offset);
    int64_t A = r->addend;
    if (!P) return -1;

    switch (r->r_type){
        case MO2_R_ABS64: {
            uint64_t v = (uint64_t)(S + A);
            memcpy(P, &v, sizeof(v));
            return 0;
        }
        case MO2_R_REL32: {
            /* PC-relative: target - (P + 4) */
            int64_t  v   = (int64_t)(S + A - ((uintptr_t)P + 4));
            int32_t  v32 = (int32_t)v;
            memcpy(P, &v32, sizeof(v32));
            return 0;
        }
        case MO2_R_GOT_LOAD: {
            uint64_t v = (uint64_t)(S + A);
            memcpy(P, &v, sizeof(v));
            return 0;
        }
        case MO2_R_PLT_CALL: {
            /* For now, leave PLT as is (lazy binding stub). */
            return 0;
        }
        default:
            return -1;
    }
}

static void collect_exports(Module* m){
    for (size_t i = 0; i < m->hdr.nsym; i++){
        mo2_sym_t* s = &m->sym[i];
        if (s->bind == MO2_SB_GLOBAL && s->sect_id != 0 && s->name_off < m->str_sz){
            const char* nm = m->str + s->name_off;
            void* addr = sect_ptr(m, s->sect_id, (uint32_t)s->value);
            if (nm && addr) exports_insert(m, nm, addr);
        }
    }
}

static uintptr_t resolve_import(Module* self, uint32_t sym_idx){
    if (sym_idx >= self->hdr.nsym) return 0;
    mo2_sym_t* s = &self->sym[sym_idx];
    if (s->name_off >= self->str_sz) return 0;
    const char* name = self->str + s->name_off;

    /* self first */
    void* a = exports_find(self, name);
    if (a) return (uintptr_t)a;

    /* then others */
    for (size_t i = 0; i < g_module_count; i++){
        Module* m = g_modules[i];
        if (m == self) continue;
        a = exports_find(m, name);
        if (a) return (uintptr_t)a;
    }
    return 0;
}

static int apply_relocs(Module* m){
    for (size_t i = 0; i < m->n_reloc; i++){
        mo2_reloc_t* r = &m->relocs[i];
        uintptr_t S = 0;

        if (r->sym_idx < m->hdr.nsym){
            S = resolve_import(m, r->sym_idx);
            if (!S){
                const char* nm = (r->sym_idx < m->hdr.nsym && m->sym && m->str)
                                 ? (m->str + m->sym[r->sym_idx].name_off)
                                 : "(?)";
                LOGF("[dyld2] unresolved symbol idx=%u name=%s\n",
                     (unsigned)r->sym_idx, nm ? nm : "(?)");
                return -1;
            }
        }

        if (relocate_one(m, r, S) != 0){
            LOGF("[dyld2] relocate_one failed type=%u off=%u\n",
                 (unsigned)r->r_type, (unsigned)r->r_offset);
            return -1;
        }
    }
    return 0;
}

/* ----------------------------------------------------------------------------
 * Load/Run
 * --------------------------------------------------------------------------*/
static Module* load_one(const char* path){
    Module* m = (Module*)malloc(sizeof(Module));
    if (!m) return 0;
    memset(m, 0, sizeof(*m));

    snprintf(m->name, sizeof(m->name), "%s", path ? path : "(null)");

    if (read_entire(path, &m->image, &m->image_sz) != 0){ free(m); return 0; }
    if (parse_headers(m) != 0){ free(m->image); free(m); return 0; }

    classify_sections(m);

    /* Allocate flat image: [text|ro|data|bss|got|plt] */
    m->base = (uint8_t*)alloc_segs(m->text_sz, m->ro_sz, m->data_sz, m->bss_sz,
                                   m->got_sz, m->plt_sz);
    if (!m->base){ free(m->image); free(m); return 0; }

    /* Copy file-backed sections into place */
    if (m->text  && m->text_sz)  memcpy(m->base, m->text, m->text_sz);
    if (m->ro    && m->ro_sz)    memcpy(m->base + m->text_sz, m->ro, m->ro_sz);
    if (m->data  && m->data_sz)  memcpy(m->base + m->text_sz + m->ro_sz, m->data, m->data_sz);
    if (m->got   && m->got_sz)   memcpy(m->base + m->text_sz + m->ro_sz + m->data_sz + m->bss_sz,
                                        m->got, m->got_sz);
    if (m->plt   && m->plt_sz)   memcpy(m->base + m->text_sz + m->ro_sz + m->data_sz + m->bss_sz + m->got_sz,
                                        m->plt, m->plt_sz);
    /* bss is already zeroed via alloc_segs */

    collect_exports(m);

    if (apply_relocs(m) != 0){
        LOG("[dyld2] reloc failed\n");
        free(m->base); free(m->image); free(m);
        return 0;
    }

    register_module(m);
    return m;
}

/* API surface --------------------------------------------------------------*/
int dyld2_init(const AgentAPI* api){
    G = api;
    LOG("[dyld2] init\n");
    return 0;
}

mo2_handle_t     dyld2_dlopen(const char* path, int flags){ (void)flags; return (mo2_handle_t)load_one(path); }
mo2_sym_handle_t dyld2_dlsym (mo2_handle_t h, const char* name){ Module* m=(Module*)h; return exports_find(m, name); }
int              dyld2_dlclose(mo2_handle_t h){ Module* m=(Module*)h; unregister_module(m); return 0; }

int dyld2_run_exec(const char* path, int argc, const char** argv){
    Module* m = load_one(path);
    if (!m) return -1;

    uintptr_t entry = (uintptr_t)m->base + (uintptr_t)m->hdr.entry_rva;
    mo2_main_entry entry_fn = (mo2_main_entry)entry;
    if (!entry_fn){
        LOGF("[dyld2] entry NULL for %s (rva=%u)\n", path ? path : "(?)", (unsigned)m->hdr.entry_rva);
        unregister_module(m);
        free(m->base); free(m->image); free(m);
        return -2;
    }
    return entry_fn(argc, argv);
}
