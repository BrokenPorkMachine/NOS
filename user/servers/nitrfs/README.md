# NitrFS - Secure In-Memory Filesystem

NitrFS is the reference filesystem server for NitrOS. It aims to provide a
small but secure storage implementation for early user space. The current
implementation is a simple RAM-based filesystem that demonstrates the
message-passing design. The API now includes helpers to list and delete files so
that higher level servers can manage storage without directly accessing the file
table. In addition, NitrFS can export and import its contents as a block image,
laying the groundwork for persistent storage.

## Design Goals

* **Memory safety**: written in C with bounds checks and fixed-size tables.
* **Integrity**: each file stores a CRC32 checksum. `nitrfs_verify()` can be
  used to validate data against the stored checksum.
* **Capabilities**: basic read/write permissions on a per-file basis.
* **Isolation**: intended to run as a user-mode server communicating over IPC.

Future versions may support persistent block devices and a hierarchical
on-disk layout, but the initial focus is a minimal, auditable core.

## Server Usage

The `server.c` file implements a minimal message-driven server built on top of
the generic IPC queue. Each request is delivered as an `ipc_message_t` and the
server replies on the same queue. This design is extremely small and intended
only as a demonstration of how future user-mode services might operate.

### Message Types

The filesystem server understands the following request types:

| Type              | Description                          |
| ----------------- | ------------------------------------ |
| `NITRFS_MSG_CREATE` | Create a new file with a given size and permissions |
| `NITRFS_MSG_WRITE`  | Write data to an open file (arg1=handle, arg2=offset, msg.len=bytes, msg.data=payload) |
| `NITRFS_MSG_READ`   | Read data from an open file  (arg1=handle, arg2=offset, msg.len=bytes) |
| `NITRFS_MSG_DELETE` | Delete a file by handle              |
| `NITRFS_MSG_RENAME` | Rename an existing file              |
| `NITRFS_MSG_LIST`   | List all file names                  |
| `NITRFS_MSG_CRC`    | Compute CRC32 of a file              |
| `NITRFS_MSG_VERIFY` | Verify stored CRC against data       |
