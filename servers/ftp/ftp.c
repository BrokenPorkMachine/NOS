#include "ftp.h"
#include "../../IO/serial.h"
#include "../../Task/thread.h"

void ftp_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[ftp] FTP server stub - networking not implemented\n");
    while (1) thread_yield();
}
