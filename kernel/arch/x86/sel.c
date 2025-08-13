#include "sel.h"
#include "printf.h"
#include "panic.h"

void assert_gdt_selector(uint16_t sel, const char* where){
    uint16_t idx = sel >> 3;
    int ti = (sel >> 2) & 1;
    int rpl = sel & 3;
    kprintf("[sel] %s: sel=0x%04x idx=%u TI=%d RPL=%d\n", where, sel, idx, ti, rpl);
    if (ti) panic("%s: LDT selector used without LDT (sel=0x%04x)", where, sel);
}
