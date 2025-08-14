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

## Architecture

NOSFS is designed around a set of modular subsystems that balance performance,
security and scalability.  The following sections sketch the core components
that will be fleshed out during implementation.

### Storage Engine
- Copy‑on‑write update path for all data and metadata.
- Dynamic block allocation (4 KB–1 MB) with space maps and free‑space
  coalescing.
- Pluggable compression and encryption at the file or directory level.

### Metadata Layer
- Hybrid B⁺‑tree/LSM indexing for fast lookups and append‑heavy workloads.
- Versioned entries to support snapshots and clones with minimal overhead.
- Shardable namespace to distribute directory subtrees across nodes.

### Journaling & Consistency
- Two‑tier journal: an in‑memory log for low‑latency commits and a persistent
  log for crash recovery.
- Journal records include Merkle hashes to enable integrity verification and
  atomic group commits.

### Security Subsystem
- Transparent AES‑256‑GCM encryption with per‑object keys and rotation.
- Pluggable authentication and authorization modules (ACLs, roles,
  capabilities).
- Cryptographically signed journals and metadata blocks.

### Caching & Prefetching
- Multi‑level adaptive caches separating hot and cold data.
- Asynchronous read‑ahead and write coalescing to maximise throughput.

### Distributed Operation
- Namespace sharding across storage nodes with replication and failover.
- Lightweight RPC layer with TLS and optional RDMA data paths.

### Observability & Management
- Unified metrics and tracing (OpenTelemetry compatible).
- CLI/REST management interfaces for snapshot, clone and recovery orchestration.

### Implementation Roadmap
1. Foundation: block allocator, basic metadata store and journal.
2. Reliability: full CoW path, crash recovery and background scrubbing.
3. Security: encryption, ACL framework and signed metadata.
4. Scalability: namespace sharding and replication.
5. Advanced features: snapshots, compression and extended attributes.
