#include "acpi.h"
#include "../../drivers/IO/serial.h"
#include "../../../user/libc/libc.h"
#include "../CPU/lapic.h"

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

struct madt {
    struct sdt_header header;
    uint32_t lapic_addr;
    uint32_t flags;
    uint8_t  entries[];
} __attribute__((packed));

struct madt_lapic {
    uint8_t type;
    uint8_t length;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

void acpi_init(bootinfo_t *bootinfo) {
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
    struct sdt_header *sdt;
    int entry_size;
    if (rsdp->revision >= 2 && rsdp->xsdt_addr) {
        sdt = (struct sdt_header*)(uintptr_t)rsdp->xsdt_addr;
        entry_size = 8;
    } else {
        sdt = (struct sdt_header*)(uintptr_t)rsdp->rsdt_addr;
        entry_size = 4;
    }
    if (!sdt) { serial_puts("[acpi] no RSDT/XSDT\n"); return; }
    if (sum((const uint8_t*)sdt, sdt->length)) {
        serial_puts("[acpi] RSDT/XSDT checksum fail\n");
        return;
    }
    int entries = (sdt->length - sizeof(*sdt)) / entry_size;
    g_dsdt = NULL;
    for (int i = 0; i < entries && i < 16; ++i) {
        uint64_t addr;
        if (entry_size == 8)
            addr = ((uint64_t*)((uintptr_t)sdt + sizeof(*sdt)))[i];
        else
            addr = ((uint32_t*)((uintptr_t)sdt + sizeof(*sdt)))[i];
        struct sdt_header *hdr = (struct sdt_header*)(uintptr_t)addr;
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
        } else if (!memcmp(hdr->signature, "APIC", 4)) {
            serial_puts("[acpi] MADT found\n");
            struct madt *m = (struct madt *)hdr;
            lapic_init(m->lapic_addr);
            uint32_t count = 0;
            uint8_t *p = m->entries;
            uint8_t *end = ((uint8_t*)m) + m->header.length;
            while (p + sizeof(struct madt_lapic) <= end) {
                struct madt_lapic *lap = (struct madt_lapic*)p;
                if (lap->type == 0 && (lap->flags & 1)) {
                    if (count < BOOTINFO_MAX_CPUS) {
                        bootinfo->cpus[count].processor_id = lap->processor_id;
                        bootinfo->cpus[count].apic_id = lap->apic_id;
                        bootinfo->cpus[count].flags = lap->flags;
                        count++;
                    }
                }
                p += lap->length ? lap->length : 2;
            }
            bootinfo->cpu_count = count ? count : 1;
        }
    }
}
