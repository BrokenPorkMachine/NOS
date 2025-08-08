# NOSFS Filesystem

NOSFS is the transactional, agent-driven filesystem for NitrOS. It replaces
the legacy NitrFS design and showcases how storage integrates with the new
agent platform.

## Overview

- **Kernel integration**
  - Filesystem agents are loaded dynamically through the NOSM loader.
  - The kernel discovers the active filesystem via a capability query
    (`filesystem`).
  - VFS syscalls are routed through the agent entry point, enabling hot-swap
    without a reboot.
  - `n2_hot_reload_filesystem` unregisters the old agent before loading a
    replacement to preserve journal consistency.
- **Agent registry**
  - Agents register via `n2_agent_register()` with a manifest pointer.
  - The registry exposes enumeration and capability search used by the kernel
    and userland tools such as `nosfsctl`.
- **Module packaging**
  - NOSFS is distributed as a signed NOSM module with `nosfs.manifest.json`.
  - The on-disk superblock (`nosfs_superblock_t`) embeds a pointer to the
    manifest and version fields.
  - All metadata operations are journaled to support snapshots and rollbacks.
  - APIs in `include/nosfs.h` support quotas, ACLs, xattrs, and async flush.
- **Userland tools**
  - `nosfsctl` demonstrates IPC-based management without linking against
    filesystem internals.
  - Tools discover agents at runtime and can trigger snapshot or rollback
    operations safely.
