#include "acpi.h"
#include "../IO/serial.h"
#include "../src/libc.h"

struct rsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oemid[6];
    uint8_t  revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed));

struct sdt_header {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oemid[6];
    char     oemtableid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

static const struct sdt_header *g_dsdt = NULL;

const void *acpi_get_dsdt(void) { return g_dsdt; }

static uint8_t sum(const uint8_t *p, size_t len) {
    uint8_t v = 0; for (size_t i = 0; i < len; ++i) v += p[i]; return v;
}

static void print_sig(const char *s) {
    char buf[5];
    buf[0] = s[0]; buf[1] = s[1]; buf[2] = s[2]; buf[3] = s[3]; buf[4] = 0;
    serial_puts(buf);
}

void acpi_init(const bootinfo_t *bootinfo) {
    serial_puts("[acpi] init\n");
    if (!bootinfo || !bootinfo->acpi_rsdp) {
        serial_puts("[acpi] no RSDP\n");
        return;
    }
    struct rsdp *rsdp = (struct rsdp*)(uintptr_t)bootinfo->acpi_rsdp;
    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        serial_puts("[acpi] bad signature\n");
        return;
    }
    size_t len = (rsdp->revision < 2) ? 20 : rsdp->length;
    if (sum((const uint8_t*)rsdp, len)) {
        serial_puts("[acpi] checksum fail\n");
        return;
    }
    serial_puts("[acpi] RSDP ok\n");
    struct sdt_header *rsdt = (struct sdt_header*)(uintptr_t)rsdp->rsdt_addr;
    if (!rsdt) { serial_puts("[acpi] no RSDT\n"); return; }
    if (sum((const uint8_t*)rsdt, rsdt->length)) {
        serial_puts("[acpi] RSDT checksum fail\n");
        return;
    }
    int entries = (rsdt->length - sizeof(*rsdt)) / 4;
    uint32_t *ptrs = (uint32_t*)((uintptr_t)rsdt + sizeof(*rsdt));
    g_dsdt = NULL;
    for (int i = 0; i < entries && i < 16; ++i) {
        struct sdt_header *hdr = (struct sdt_header*)(uintptr_t)ptrs[i];
        serial_puts("[acpi] table ");
        print_sig(hdr->signature);
        serial_puts("\n");
        if (!memcmp(hdr->signature, "FACP", 4)) {
            serial_puts("[acpi] FADT found\n");
            if (hdr->length >= 44) {
                uint32_t dsdt32 = *(uint32_t*)((uint8_t*)hdr + 40);
                uint64_t dsdt = dsdt32;
                if (hdr->length >= 148) {
                    uint64_t dsdt64 = *(uint64_t*)((uint8_t*)hdr + 140);
                    if (dsdt64) dsdt = dsdt64;
                }
                struct sdt_header *d = (struct sdt_header*)(uintptr_t)dsdt;
                if (d && sum((const uint8_t*)d, d->length) == 0 && !memcmp(d->signature, "DSDT", 4)) {
                    g_dsdt = d;
                    serial_puts("[acpi] DSDT loaded\n");
                } else {
                    serial_puts("[acpi] DSDT invalid\n");
                }
            }
        }
    }
}
