void gdt_flush_with_tr(const void *gdtr, unsigned short tss_sel) {}

void serial_printf(const char *fmt, ...) { (void)fmt; }
