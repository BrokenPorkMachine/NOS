#include "acpi.h"
#include "../../drivers/IO/serial.h"
#include "../../../user/libc/libc.h"
#include "../CPU/lapic.h"

// ---- ACPI Table Structures (per spec) ----

#define ACPI_SIGNATURE(s) (!memcmp(hdr->signature, s, 4))
#define ACPI_DSDT32_OFFSET   40
#define ACPI_DSDT64_OFFSET  140
#define ACPI_FADT_MIN_LEN    44
#define ACPI_FADT_2_MIN_LEN 148
#define ACPI_MAX_TABLES     64    // Upper limit to avoid loops on corrupted tables

struct rsdp {
    char     signature[8];        // "RSD PTR "
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

// Global pointer to DSDT (fixed ACPI table)
static const struct sdt_header *g_dsdt = NULL;
const void *acpi_get_dsdt(void) { return g_dsdt; }

static uint8_t sum(const uint8_t *p, size_t len) {
    uint8_t v = 0; for (size_t i = 0; i < len; ++i) v += p[i]; return v;
}

// Safe 4-char ACPI signature printer (shows '.' for non-printable)
static void print_sig(const char *s) {
    char buf[5] = {0};
    for (int i = 0; i < 4; ++i)
        buf[i] = (s[i] >= 32 && s[i] < 127) ? s[i] : '.';
    serial_puts(buf);
}

// MADT (APIC) and LAPIC entry per ACPI spec
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

// ---- Main ACPI Initialization ----

void acpi_init(bootinfo_t *bootinfo) {
    serial_puts("[acpi] init\n");

    // -------- Locate RSDP --------
    if (!bootinfo || !bootinfo->acpi_rsdp) {
        serial_puts("[acpi] no RSDP\n");
        return;
    }
    struct rsdp *rsdp = (struct rsdp*)(uintptr_t)bootinfo->acpi_rsdp;

    // Signature and checksum validation
    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        serial_puts("[acpi] bad RSDP signature\n");
        return;
    }
    if (sum((const uint8_t*)rsdp, 20) != 0) {
        serial_puts("[acpi] RSDP base checksum fail\n");
        return;
    }
    if (rsdp->revision >= 2 && rsdp->length >= sizeof(struct rsdp)) {
        if (sum((const uint8_t*)rsdp, rsdp->length) != 0) {
            serial_puts("[acpi] RSDP extended checksum fail\n");
            return;
        }
    }
    serial_puts("[acpi] RSDP OK\n");

    // -------- Locate RSDT or XSDT --------
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
    if (sum((const uint8_t*)sdt, sdt->length) != 0) {
        serial_puts("[acpi] RSDT/XSDT checksum fail\n");
        return;
    }

    int entries = (sdt->length - sizeof(*sdt)) / entry_size;
    if (entries <= 0 || entries > ACPI_MAX_TABLES) {
        serial_puts("[acpi] RSDT/XSDT table entry count out of range\n");
        return;
    }

    g_dsdt = NULL;
    for (int i = 0; i < entries; ++i) {
        uint64_t addr = 0;
        if (entry_size == 8)
            addr = ((uint64_t*)((uintptr_t)sdt + sizeof(*sdt)))[i];
        else
            addr = ((uint32_t*)((uintptr_t)sdt + sizeof(*sdt)))[i];

        if (!addr) continue;
        struct sdt_header *hdr = (struct sdt_header*)(uintptr_t)addr;
        // Basic validity checks
        if (!hdr || hdr->length < sizeof(struct sdt_header) || hdr->length > 0x10000)
            continue;
        if (sum((const uint8_t*)hdr, hdr->length) != 0) {
            serial_puts("[acpi] table checksum fail: ");
            print_sig(hdr->signature); serial_puts("\n");
            continue;
        }

        serial_puts("[acpi] table ");
        print_sig(hdr->signature);
        serial_puts("\n");

        // -------- FADT/FACP: Find and validate DSDT --------
        if (ACPI_SIGNATURE("FACP")) {
            serial_puts("[acpi] FADT found\n");
            if (hdr->length >= ACPI_FADT_MIN_LEN) {
                uint32_t dsdt32 = *(uint32_t*)((uint8_t*)hdr + ACPI_DSDT32_OFFSET);
                uint64_t dsdt = dsdt32;
                if (hdr->length >= ACPI_FADT_2_MIN_LEN) {
                    uint64_t dsdt64 = *(uint64_t*)((uint8_t*)hdr + ACPI_DSDT64_OFFSET);
                    if (dsdt64) dsdt = dsdt64;
                }
                struct sdt_header *d = (struct sdt_header*)(uintptr_t)dsdt;
                if (d && d->length >= sizeof(struct sdt_header) && d->length < 0x10000 &&
                    sum((const uint8_t*)d, d->length) == 0 && memcmp(d->signature, "DSDT", 4) == 0) {
                    g_dsdt = d;
                    serial_puts("[acpi] DSDT loaded\n");
                } else {
                    serial_puts("[acpi] DSDT invalid\n");
                }
            }
        }

        // -------- MADT/APIC: Parse LAPIC entries --------
        else if (ACPI_SIGNATURE("APIC")) {
            serial_puts("[acpi] MADT found\n");
            struct madt *m = (struct madt *)hdr;
            lapic_init(m->lapic_addr);
            uint32_t count = 0;
            uint8_t *p = m->entries;
            uint8_t *end = ((uint8_t*)m) + m->header.length;
            while (p + sizeof(struct madt_lapic) <= end) {
                struct madt_lapic *lap = (struct madt_lapic*)p;
                if (lap->length < 2) break; // Prevent infinite loop
                if (lap->type == 0 && (lap->flags & 1)) { // Processor Local APIC, enabled
                    if (count < BOOTINFO_MAX_CPUS) {
                        bootinfo->cpus[count].processor_id = lap->processor_id;
                        bootinfo->cpus[count].apic_id = lap->apic_id;
                        bootinfo->cpus[count].flags = lap->flags;
                        count++;
                    }
                }
                p += lap->length;
            }
            bootinfo->cpu_count = count ? count : 1;
        }
    }
}
