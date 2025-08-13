#include <string.h>
#include <stdint.h>
#include "tss.h"
#include "gdt.h"
#include "drivers/IO/serial.h"
#include "arch_x86_64/gdt_tss.h"

static struct tss64 tss;

void gdt_tss_init(void *stack_top) {
    memset(&tss, 0, sizeof(tss));
    tss.rsp0 = (uint64_t)stack_top;

    gdt_install_with_tss(&tss, sizeof(tss) - 1);

    uint16_t tr;
    __asm__ volatile("str %0" : "=r"(tr));
    serial_printf("[tss] TR=0x%04x RSP0=0x%llx\n", tr, (unsigned long long)tss.rsp0);
}
