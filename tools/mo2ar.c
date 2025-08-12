// ============================================================================
// tools/mo2ar.c  -- minimal .a2 archiver with ranlib index
// ============================================================================
#include "mo2util.h"

/* .a2 layout:
   [magic="!<arch>\n"]
   [member][member]...
   ranlib index at end as special member "__.SYMIDX" storing:
     uint32_t n; then n entries of: uint32_t name_off; uint64_t member_off; followed by names blob
*/

int main(int argc, char** argv){
    if(argc<4){ fprintf(stderr,"usage: mo2ar rcs libfoo.a2 a.o2 b.o2 ...\n"); return 1; }
    const char* out=argv[2]; FILE* f=fopen(out,"wb"); if(!f){ perror("fopen"); return 1; }
    fwrite("!<arch>\n",1,8,f);
    // write members
    typedef struct { char* name; long off; } Mem;
    Mem* mems=0; size_t nmem=0, cap=0; size_t names_sz=0;
    for(int i=3;i<argc;i++){
        const char* path=argv[i]; FILE* in=fopen(path,"rb"); if(!in){ perror("open member"); return 1; }
        fseek(in,0,SEEK_END); long sz=ftell(in); fseek(in,0,SEEK_SET);
        char hdr[60]; memset(hdr,' ',sizeof(hdr)); memcpy(hdr,"##########/",12); // simple name slot
        snprintf(hdr,16,"%-16s", "member.o2"); snprintf(hdr+16,12,"%010ld", 0L); // date
        snprintf(hdr+28,6,"%05d", 0); snprintf(hdr+34,6,"%05d", 0); snprintf(hdr+40,10,"%010ld", sz);
        memcpy(hdr+50,"\x60\n",2);
        mems = (Mem*)realloc(mems, (nmem+1)*sizeof(Mem)); mems[nmem].name=xstrdup(path); mems[nmem].off=ftell(f); nmem++;
        fwrite(hdr,1,60,f);
        char* buf=(char*)xmalloc(sz); fread(buf,1,sz,in); fwrite(buf,1,sz,f); free(buf); fclose(in);
        if(sz & 1) fputc('\n',f); // even alignment
        names_sz += strlen(path)+1;
    }

    // build simple index (symbol -> member). Here we just store names->member offsets to be resolved by linker.
    long idx_off=ftell(f);
    char hdr[60]; memset(hdr,' ',sizeof(hdr)); snprintf(hdr,16,"%-16s", "__.SYMIDX/"); snprintf(hdr+40,10,"%010d", 0); memcpy(hdr+50,"\x60\n",2);
    fwrite(hdr,1,60,f);
    uint32_t n=(uint32_t)nmem; fwrite(&n,4,1,f);
    // name table: we just record member names; real ranlib would record exported symbols.
    uint32_t name_off=0;
    for(size_t i=0;i<nmem;i++){
        uint32_t no=name_off; fwrite(&no,4,1,f);
        uint64_t mo=(uint64_t)mems[i].off; fwrite(&mo,8,1,f);
        name_off += (uint32_t)(strlen(mems[i].name)+1);
    }
    for(size_t i=0;i<nmem;i++){ fwrite(mems[i].name,1,strlen(mems[i].name)+1,f); }
    long end=ftell(f);
    fseek(f,idx_off+40,SEEK_SET); fprintf(f,"%010ld", (long)(end - (idx_off+60)) );
    fclose(f);
    for(size_t i=0;i<nmem;i++) free(mems[i].name); free(mems);
    return 0;
}
