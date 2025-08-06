#include "grep.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../libc/libc.h"
#include "../../../kernel/IPC/ipc.h"

void grep_main(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    const char *pattern = "foo";
    FILE *f = fopen("src", "r");
    if (!f) { serial_puts("grep: file not found\n"); return; }
    char buf[IPC_MSG_DATA_MAX + 1];
    size_t n = fread(buf, 1, IPC_MSG_DATA_MAX, f);
    buf[n] = '\0';
    if (strstr(buf, pattern))
        serial_puts(buf);
    fclose(f);
}
