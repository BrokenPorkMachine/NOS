#include <string.h>
#include <stdint.h>
#include "tss.h"
#include "gdt.h"
#include "drivers/IO/serial.h"

static struct tss64 tss;
static uint8_t tss_stack[4096];

void tss_install(void) {
    memset(&tss, 0, sizeof(tss));
    tss.rsp0 = (uint64_t)(tss_stack + sizeof(tss_stack));

    gdt_install_with_tss(&tss, sizeof(tss) - 1);

    uint16_t tr;
    __asm__ volatile("str %0" : "=r"(tr));
    serial_printf("[tss] TR=0x%04x RSP0=0x%llx\n", tr, (unsigned long long)tss.rsp0);
}
