#include "cp.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../libc/libc.h"
#include "../../../kernel/IPC/ipc.h"

void cp_main(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    FILE *src = fopen("src", "r");
    if (!src) { serial_puts("cp: src not found\n"); return; }
    FILE *dst = fopen("dst", "w");
    if (!dst) { serial_puts("cp: dst create fail\n"); fclose(src); return; }
    unsigned char buf[IPC_MSG_DATA_MAX];
    size_t n = fread(buf, 1, sizeof(buf), src);
    if (n > 0) fwrite(buf, 1, n, dst);
    fclose(src);
    fclose(dst);
    serial_puts("cp: done\n");
}
