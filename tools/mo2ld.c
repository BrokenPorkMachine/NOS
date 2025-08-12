// ============================================================================
// tools/mo2ld.c  -- tiny linker (static + shared skeleton)
// ============================================================================
#include "mo2util.h"

/* NOTE: This is a *teaching* linker: it handles a few .o2 files, emits one EXEC or DYLIB.
   Missing: full archive symbol scanning, COMDAT folding, relaxation, TLS, version scripts. */

typedef struct { char* name; mo2_sym_t* syms; size_t nsym; uint8_t* data; size_t sz; } Obj;

typedef struct { char* name; uint64_t value; uint16_t bind; uint16_t sect; Obj* def_obj; } GSym;

typedef struct { Obj** objs; size_t nobjs; int out_kind; int pie; char* soname; char** needs; size_t nneeds; } Ctx;

static Obj* read_o2(const char* path){ FILE* f=fopen(path,"rb"); if(!f){ perror("open"); return 0; }
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET); uint8_t* buf=(uint8_t*)xmalloc(sz); fread(buf,1,sz,f); fclose(f);
    mo2_hdr_t* H=(mo2_hdr_t*)buf; if(H->magic!=MO2_MAGIC||H->ftype!=MO2_FTYPE_RELOC){ fprintf(stderr,"not .o2: %s\n", path); free(buf); return 0; }
    Obj* o=(Obj*)xmalloc(sizeof(Obj)); o->name=xstrdup(path); o->data=buf; o->sz=sz; o->nsym=H->nsym; o->syms=(mo2_sym_t*)(buf + H->off_sym); return o; }

static void usage(){ fprintf(stderr,"mo2ld [-static|-pie|-shared -soname NAME -lFoo -Ldir -o out.mo2] objs...\n"); }

int main(int argc, char** argv){
    Ctx cx={0}; cx.out_kind=MO2_FTYPE_EXEC; cx.pie=0;
    const char* out="a.mo2";
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"-o") && i+1<argc){ out=argv[++i]; }
        else if(!strcmp(argv[i],"-shared")){ cx.out_kind=MO2_FTYPE_DYLIB; }
        else if(!strcmp(argv[i],"-pie")){ cx.pie=1; }
        else if(!strcmp(argv[i],"-soname") && i+1<argc){ cx.soname=argv[++i]; }
        else if(argv[i][0]=='-'){ /* ignore -l/-L for this skeleton */ }
        else { Obj* o=read_o2(argv[i]); if(o){ cx.objs=(Obj**)realloc(cx.objs,(cx.nobjs+1)*sizeof(Obj*)); cx.objs[cx.nobjs++]=o; } }
    }
    if(cx.nobjs==0){ usage(); return 1; }

    // Extremely simplified layout: concatenate sections by kind across inputs; write header + sections + trivial symtab
    mo2_hdr_t H={0}; H.magic=MO2_MAGIC; H.ftype=cx.out_kind; H.cpu=MO2_CPU_X86_64; H.hdr_size=sizeof(H);
    H.nsects=4; // TEXT, RODATA, DATA, BSS
    size_t off=sizeof(H) + H.nsects*sizeof(mo2_sect_t);
    mo2_sect_t S[MO2_MAX_SECTS]={0};

    // In a real linker, compute sizes & copy/relocate code; here we emit empty stubs
    for(int i=0;i<4;i++){ S[i].kind=i+1; S[i].prot=(i==0)?(MO2_PF_X|MO2_PF_R):MO2_PF_R; if(i==3) S[i].prot=MO2_PF_R|MO2_PF_W; S[i].align=16; S[i].foffset=0; S[i].fsize=0; }

    H.off_sections=(uint32_t)sizeof(H);
    H.off_dyn=0; H.off_sym=0; H.nsym=0; H.off_str=0; H.str_sz=0;

    FILE* f=fopen(out,"wb"); if(!f){ perror("fopen out"); return 1; }
    fwrite(&H,1,sizeof(H),f); fwrite(S,1,H.nsects*sizeof(mo2_sect_t),f);
    fclose(f);
    fprintf(stderr,"wrote %s (stub)\n", out);
    return 0;
}
