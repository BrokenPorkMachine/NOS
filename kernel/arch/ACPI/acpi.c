#include "acpi.h"
#include "../../drivers/IO/serial.h"
#include "../../../user/libc/libc.h"
#include "../CPU/lapic.h"
#include <stddef.h>
#include <stdint.h>

/* ---------------- ACPI core structs ---------------- */

#define ACPI_SIGNATURE_EQ(hdr, s) (memcmp((hdr)->signature, (s), 4) == 0)

#define ACPI_DSDT32_OFFSET    40
#define ACPI_DSDT64_OFFSET   140
#define ACPI_FADT_MIN_LEN     44
#define ACPI_FADT_2_MIN_LEN  148
#define ACPI_MAX_TABLES       64   /* Upper limit to avoid loops on corrupted tables */

struct rsdp {
    char     signature[8];        /* "RSD PTR " */
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

/* MADT (APIC) structs */
struct madt {
    struct sdt_header header;
    uint32_t lapic_addr;
    uint32_t flags;
    uint8_t  entries[];
} __attribute__((packed));

/* Entry type 0: Processor Local APIC */
struct madt_lapic {
    uint8_t  type;       /* 0 */
    uint8_t  length;     /* >= 8 */
    uint8_t  processor_id;
    uint8_t  apic_id;
    uint32_t flags;      /* bit0 enabled */
} __attribute__((packed));

/* Entry type 5: Local APIC Address Override */
struct madt_lapic_addr_override {
    uint8_t  type;       /* 5 */
    uint8_t  length;     /* 12 */
    uint16_t reserved;
    uint64_t lapic_addr64;
} __attribute__((packed));

/* Entry type 9: Processor Local x2APIC */
struct madt_x2apic {
    uint8_t  type;       /* 9 */
    uint8_t  length;     /* 16 */
    uint16_t reserved;
    uint32_t x2apic_id;
    uint32_t flags;      /* bit0 enabled */
    uint32_t acpi_uid;   /* ACPI Processor UID */
} __attribute__((packed));

/* ---------------- Globals ---------------- */

static const struct sdt_header *g_dsdt = NULL;
const void *acpi_get_dsdt(void) { return g_dsdt; }

/* ---------------- Helpers ---------------- */

static uint8_t sum8(const uint8_t *p, size_t len) {
    uint8_t v = 0; for (size_t i = 0; i < len; ++i) v += p[i]; return v;
}

static void print_sig(const char *s) {
    char buf[5] = {0};
    for (int i = 0; i < 4; ++i)
        buf[i] = (s[i] >= 32 && s[i] < 127) ? s[i] : '.';
    serial_puts(buf);
}

static int sdt_valid(const struct sdt_header *hdr) {
    if (!hdr) return 0;
    if (hdr->length < sizeof(*hdr) || hdr->length > 1u << 20) return 0; /* 1 MiB sanity */
    if (sum8((const uint8_t*)hdr, hdr->length) != 0) return 0;
    return 1;
}

/* ---------------- ACPI init ---------------- */

void acpi_init(bootinfo_t *bootinfo) {
    serial_puts("[acpi] init\n");

    if (!bootinfo || !bootinfo->acpi_rsdp) {
        serial_puts("[acpi] no RSDP pointer in bootinfo\n");
        return;
    }

    struct rsdp *rsdp = (struct rsdp*)(uintptr_t)bootinfo->acpi_rsdp;

    /* RSDP validation */
    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        serial_puts("[acpi] bad RSDP signature\n");
        return;
    }
    if (sum8((const uint8_t*)rsdp, 20) != 0) {
        serial_puts("[acpi] RSDP base checksum fail\n");
        return;
    }
    if (rsdp->revision >= 2 && rsdp->length >= sizeof(struct rsdp)) {
        if (sum8((const uint8_t*)rsdp, rsdp->length) != 0) {
            serial_puts("[acpi] RSDP extended checksum fail\n");
            return;
        }
    }
    serial_puts("[acpi] RSDP OK\n");

    /* Pick XSDT if available, else RSDT */
    struct sdt_header *sdt = NULL;
    int entry_size = 0;
    if (rsdp->revision >= 2 && rsdp->xsdt_addr) {
        sdt = (struct sdt_header*)(uintptr_t)rsdp->xsdt_addr;
        entry_size = 8;
    } else if (rsdp->rsdt_addr) {
        sdt = (struct sdt_header*)(uintptr_t)rsdp->rsdt_addr;
        entry_size = 4;
    }
    if (!sdt) {
        serial_puts("[acpi] no RSDT/XSDT\n");
        return;
    }
    if (!sdt_valid(sdt)) {
        serial_puts("[acpi] RSDT/XSDT invalid checksum/len\n");
        return;
    }

    int entries = (int)((sdt->length - sizeof(*sdt)) / (unsigned)entry_size);
    if (entries <= 0 || entries > ACPI_MAX_TABLES) {
        serial_puts("[acpi] RSDT/XSDT entry count out of range\n");
        return;
    }

    g_dsdt = NULL;

    /* Track LAPIC base; MADT may override the address */
    uintptr_t lapic_base = 0;
    uint32_t  cpu_count = 0;

    for (int i = 0; i < entries; ++i) {
        uint64_t addr;
        if (entry_size == 8)
            addr = ((uint64_t*)((uintptr_t)sdt + sizeof(*sdt)))[i];
        else
            addr = (uint64_t)(((uint32_t*)((uintptr_t)sdt + sizeof(*sdt)))[i]);

        if (!addr) continue;

        struct sdt_header *hdr = (struct sdt_header*)(uintptr_t)addr;
        if (!sdt_valid(hdr)) {
            serial_puts("[acpi] table checksum/len fail: ");
            print_sig(hdr ? hdr->signature : "????");
            serial_puts("\n");
            continue;
        }

        serial_puts("[acpi] table ");
        print_sig(hdr->signature);
        serial_puts("\n");

        /* FADT/FACP → DSDT discovery */
        if (ACPI_SIGNATURE_EQ(hdr, "FACP")) {
            if (hdr->length >= ACPI_FADT_MIN_LEN) {
                uint32_t dsdt32 = *(uint32_t*)((uint8_t*)hdr + ACPI_DSDT32_OFFSET);
                uint64_t dsdt   = (uint64_t)dsdt32;
                if (hdr->length >= ACPI_FADT_2_MIN_LEN) {
                    uint64_t dsdt64 = *(uint64_t*)((uint8_t*)hdr + ACPI_DSDT64_OFFSET);
                    if (dsdt64) dsdt = dsdt64;
                }
                struct sdt_header *d = (struct sdt_header*)(uintptr_t)dsdt;
                if (sdt_valid(d) && ACPI_SIGNATURE_EQ(d, "DSDT")) {
                    g_dsdt = d;
                    serial_puts("[acpi] DSDT loaded\n");
                } else {
                    serial_puts("[acpi] DSDT invalid\n");
                }
            }
        }
        /* MADT/APIC → LAPIC base and CPU list */
        else if (ACPI_SIGNATURE_EQ(hdr, "APIC")) {
            struct madt *m = (struct madt *)hdr;
            lapic_base = (uintptr_t)(uint32_t)m->lapic_addr; /* may be overridden by type 5 */

            uint8_t *p   = m->entries;
            uint8_t *end = ((uint8_t*)m) + m->header.length;

            while (p + 2 <= end) {
                uint8_t type = p[0];
                uint8_t len  = p[1];
                if (len < 2) break; /* corrupt; avoid infinite loop */
                if (p + len > end) break;

                switch (type) {
                    case 0: { /* Processor Local APIC */
                        if (len >= sizeof(struct madt_lapic)) {
                            const struct madt_lapic *lap = (const struct madt_lapic*)p;
                            if (lap->flags & 1u) {
                                if (cpu_count < BOOTINFO_MAX_CPUS) {
                                    bootinfo->cpus[cpu_count].processor_id = lap->processor_id;
                                    bootinfo->cpus[cpu_count].apic_id      = lap->apic_id;
                                    bootinfo->cpus[cpu_count].flags        = lap->flags;
                                    cpu_count++;
                                }
                            }
                        }
                        break;
                    }
                    case 5: { /* Local APIC Address Override */
                        if (len >= sizeof(struct madt_lapic_addr_override)) {
                            const struct madt_lapic_addr_override *ov = (const struct madt_lapic_addr_override*)p;
                            lapic_base = (uintptr_t)ov->lapic_addr64;
                            serial_puts("[acpi] LAPIC addr override\n");
                        }
                        break;
                    }
                    case 9: { /* Processor Local x2APIC */
                        if (len >= sizeof(struct madt_x2apic)) {
                            const struct madt_x2apic *x2 = (const struct madt_x2apic*)p;
                            if (x2->flags & 1u) {
                                if (cpu_count < BOOTINFO_MAX_CPUS) {
                                    /* In x2APIC, APIC IDs are 32-bit; store low 8 if your bootinfo uses 8-bit IDs,
                                       otherwise extend bootinfo to 32-bit IDs. */
                                    bootinfo->cpus[cpu_count].processor_id = (uint8_t)x2->acpi_uid;
                                    bootinfo->cpus[cpu_count].apic_id      = (uint8_t)(x2->x2apic_id & 0xFF);
                                    bootinfo->cpus[cpu_count].flags        = x2->flags;
                                    cpu_count++;
                                }
                            }
                        }
                        break;
                    }
                    default:
                        /* skip other types */
                        break;
                }
                p += len;
            }

            /* Initialize LAPIC now that we know its base (identity-mapped) */
            if (lapic_base) {
                lapic_init(lapic_base);
            }
        }
    }

    /* Finalize CPU count in bootinfo */
    bootinfo->cpu_count = cpu_count ? cpu_count : 1u;
}
