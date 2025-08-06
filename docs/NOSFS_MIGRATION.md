# NOSFS Migration Plan

This document outlines the steps required to migrate NitrOS from the legacy
NitrFS filesystem to the agent‑driven NOSFS design.

## Overview

* **Kernel**
  * Load filesystem agents dynamically through the NOSM loader.
  * Discover active filesystem via capability query (`filesystem`).
  * Route VFS syscalls through the agent entry point to allow hot‑swap.
  * Ensure `n2_hot_reload_filesystem` unregisters the old agent before
    loading a replacement to preserve journal consistency.

* **Agent Registry**
  * Agents register via `n2_agent_register()` with a manifest pointer.
  * Registry exposes enumeration and capability search used by the kernel
    and userland tools such as `nosfsctl`.

* **Filesystem (NOSFS)**
  * Packaged as a signed NOSM module with `nosfs.manifest.json`.
  * Manifest advertises version, ABI, capabilities, and permissions.
  * On‑disk superblock (`nosfs_superblock_t`) includes version fields and a
    pointer to the embedded manifest for verification.
  * All metadata operations are journaled to support snapshots and rollbacks.
  * APIs in `include/nosfs.h` support quotas, ACLs, xattrs, and async flush.

* **Userland Tools**
  * `nosfsctl` demonstrates IPC based management without linking against
    filesystem internals.
  * Tools discover agents at runtime and can trigger snapshot/rollback
    operations safely.

## Migration Steps

1. Rename all NitrFS sources and references to NOSFS.
2. Introduce `nosfs.manifest.json` and embed manifest pointer in module.
3. Update kernel boot flow to locate and bind the NOSFS agent dynamically.
4. Add `nosfs_superblock_t` header and versioning to the on‑disk format.
5. Provide userland examples (`nosfsctl`) and documentation for the new
   API and hot‑swap semantics.
