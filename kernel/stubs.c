#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef struct ipc_queue ipc_queue_t;

void usb_init(void) {}
void usb_kbd_init(void) {}
void video_init(const void *fb) { (void)fb; }
void tty_init(void) {}
void ps2_init(void) {}
void block_init(void) {}
int  sata_init(void) { return 0; }
void net_init(void) {}

int block_read(uint32_t lba, uint8_t *buf, size_t count) { (void)lba; (void)buf; (void)count; return -1; }
int block_write(uint32_t lba, const uint8_t *buf, size_t count) { (void)lba; (void)buf; (void)count; return -1; }

int fs_read_all(const char *path, void **out, size_t *out_sz) { (void)path; if(out)*out=NULL; if(out_sz)*out_sz=0; return -1; }

void nosfs_server(ipc_queue_t *q, uint32_t self_id) { (void)q; (void)self_id; }
void nosm_entry(void) {}
