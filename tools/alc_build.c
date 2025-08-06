#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/alc.h"

/* Minimal cache builder: alc_build manifest.json blob.bin output.alc */
int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s manifest.json blob.bin output.alc\n", argv[0]);
        return 1;
    }
    const char *manifest_path = argv[1];
    const char *blob_path = argv[2];
    const char *out_path = argv[3];

    /* Load manifest */
    FILE *mf = fopen(manifest_path, "rb");
    if (!mf) {
        perror("manifest");
        return 1;
    }
    fseek(mf, 0, SEEK_END);
    long manifest_sz = ftell(mf);
    fseek(mf, 0, SEEK_SET);
    char *manifest = malloc(manifest_sz);
    if (!manifest) return 1;
    fread(manifest, 1, manifest_sz, mf);
    fclose(mf);

    /* Load blob */
    FILE *bf = fopen(blob_path, "rb");
    if (!bf) {
        perror("blob");
        return 1;
    }
    fseek(bf, 0, SEEK_END);
    long blob_sz = ftell(bf);
    fseek(bf, 0, SEEK_SET);
    char *blob = malloc(blob_sz);
    if (!blob) return 1;
    fread(blob, 1, blob_sz, bf);
    fclose(bf);

    /* Allocate output image */
    long index_off = sizeof(alc_header_t);
    long manifest_off = index_off + sizeof(alc_entry_t);
    long blob_off = manifest_off + manifest_sz;
    long total = blob_off + blob_sz;

    char *image = calloc(1, total);
    alc_header_t *hdr = (alc_header_t *)image;
    hdr->magic = ALC_MAGIC;
    hdr->version = 1;
    strncpy(hdr->arch, "x86_64", sizeof(hdr->arch));
    strncpy(hdr->abi, "N2-1.0", sizeof(hdr->abi));
    hdr->entry_count = 1;
    hdr->index_offset = index_off;

    alc_entry_t *e = (alc_entry_t *)(image + index_off);
    e->type = ALC_TYPE_LIBRARY;
    e->blob_offset = blob_off;
    e->blob_size = blob_sz;
    e->manifest_off = manifest_off;
    e->manifest_sz = manifest_sz;
    /* In a real builder we would fill hash and signature here. */
    strncpy(e->name, "example", sizeof(e->name));
    strncpy(e->version, "1.0", sizeof(e->version));
    strncpy(e->abi, "N2-1.0", sizeof(e->abi));

    memcpy(image + manifest_off, manifest, manifest_sz);
    memcpy(image + blob_off, blob, blob_sz);

    FILE *out = fopen(out_path, "wb");
    if (!out) {
        perror("output");
        return 1;
    }
    fwrite(image, 1, total, out);
    fclose(out);

    free(image);
    free(manifest);
    free(blob);
    return 0;
}
