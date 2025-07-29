#ifndef SHA256_H
#define SHA256_H
#include <efi.h>
#include <efilib.h>

typedef struct {
    UINT32 state[8];
    UINT64 bitlen;
    UINTN datalen;
    UINT8 data[64];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const UINT8 *data, UINTN len);
void sha256_final(SHA256_CTX *ctx, UINT8 hash[32]);

#endif // SHA256_H
