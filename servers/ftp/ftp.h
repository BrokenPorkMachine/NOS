#ifndef FTP_SERVER_H
#define FTP_SERVER_H
#include "../../IPC/ipc.h"
void ftp_server(ipc_queue_t *q, uint32_t self_id);
#endif // FTP_SERVER_H
