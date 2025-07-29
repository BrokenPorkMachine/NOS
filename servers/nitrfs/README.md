# NitrFS - Secure In-Memory Filesystem

NitrFS is the reference filesystem server for NitrOS. It aims to provide a
small but secure storage implementation for early user space. The current
implementation is a simple RAM-based filesystem that demonstrates the
message-passing design.

## Design Goals

* **Memory safety**: written in C with bounds checks and fixed-size tables.
* **Integrity**: each file stores a CRC32 checksum. `nitrfs_verify()` can be
  used to validate data against the stored checksum.
* **Capabilities**: basic read/write permissions on a per-file basis.
* **Isolation**: intended to run as a user-mode server communicating over IPC.

Future versions may support persistent block devices and a hierarchical
on-disk layout, but the initial focus is a minimal, auditable core.
