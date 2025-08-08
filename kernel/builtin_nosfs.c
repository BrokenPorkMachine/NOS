#include <stddef.h>
#include <stdint.h>

/*
 * Minimal built-in NOSFS image placeholder so the kernel
 * can attempt to load a filesystem agent without emitting
 * a missing/invalid warning at boot.
 */
const uint8_t nosfs_image[] = {0};
size_t nosfs_size = sizeof(nosfs_image);

