#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "../../user/agents/ftp/ftp.h"
#include "../../kernel/IPC/ipc.h"
#include "../../user/libc/libc.h"
#include "../../nosm/drivers/Net/netstack.h"

static const char *input = "QUIT\r\n";
static size_t in_pos;
static char output[256];
static size_t out_pos;

int net_socket_open(uint16_t port, net_socket_type_t type) {
    (void)port; (void)type;
    return 1;
}
int net_socket_close(int sock) { (void)sock; return 0; }
int net_socket_send(int sock, const void *data, size_t len) {
    (void)sock;
    if (out_pos + len > sizeof(output)) len = sizeof(output) - out_pos;
    memcpy(output + out_pos, data, len);
    out_pos += len;
    return (int)len;
}
int net_socket_recv(int sock, void *buf, size_t len) {
    (void)sock;
    if (in_pos >= strlen(input))
        return 0;
    size_t n = strlen(input) - in_pos;
    if (n > len) n = len;
    memcpy(buf, input + in_pos, n);
    in_pos += n;
    return (int)n;
}
void thread_yield(void) {}
void serial_puts(const char *s) { (void)s; }
void serial_write(char c) { (void)c; }

int main(void) {
    ftp_server(NULL, 0);
    const char expect[] = "220 NOS FTP\r\n221 Bye\r\n";
    assert(out_pos == strlen(expect));
    assert(memcmp(output, expect, out_pos) == 0);
    return 0;
}
