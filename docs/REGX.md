# RegX: Unified NitrOS Registry

RegX is a real‑time, manifest‑driven registry used by the NitrOS kernel and all
agents.  It tracks every loaded agent, device, driver, filesystem, userland
service and hardware resource.  Entries are never stale; all operations are
atomic, versioned and auditable.

## Data Model

Each entry is described by a [`regx_manifest_t`](../include/regx.h) and stored in
a [`regx_entry_t`](../include/regx.h).  Devices and buses form a hierarchy via
`parent_id` while agents and services are flat.

Key manifest fields:

- **name** – human readable identifier.
- **type** – agent, driver, filesystem, device or service.
- **version / ABI** – compatibility information for hot‑upgrades.
- **capabilities** – comma separated list used for discovery.
- **permissions / dependencies** – security and ordering information.

Runtime metadata:

- **id** – unique monotonically increasing identifier.
- **state** – active/paused/error flags.
- **generation** – incremented on every update for strong consistency.
- **signature** – pointer to authentication data.

## Kernel API

The kernel exports an API implemented in [`regx.c`](../kernel/regx.c):

```c
int regx_register(const regx_entry_t *entry);
int regx_unregister(uint64_t id);
int regx_update(uint64_t id, const regx_entry_t *delta);
const regx_entry_t *regx_query(uint64_t id);
size_t regx_enumerate(const regx_selector_t *sel,
                      regx_entry_t *out, size_t max);
```

All functions acquire a spinlock so updates and queries are atomic.  Updates only
modify runtime fields and increment the `generation` counter, ensuring that
queried data is consistent and that hot‑unplug/upgrade never leaves dangling
references.  Before insertion the kernel verifies cryptographic signatures and
checks access permissions.

## Kernel and Agent Interaction

1. **Registration** – During startup or load, an agent populates a
   `regx_entry_t` with its manifest and calls `regx_register`.  The kernel
   assigns a unique `id` and publishes the entry.
2. **Discovery** – Agents or user processes query the registry via
   `regx_query` or `regx_enumerate` to locate services by ID, type or
   capability.
3. **Update/Unregister** – Hot upgrades call `regx_update` with new state or
   parent information.  When an agent terminates it calls `regx_unregister`.
   Updates are auditable via the `generation` counter.

Example (agent registering a filesystem):
```c
regx_entry_t fs = {
    .manifest = { .name = "NOSFS", .type = REGX_TYPE_FILESYSTEM,
                  .version = "1.2.0", .abi = "N2-1.0",
                  .capabilities = "filesystem,snapshot" },
    .state = REGX_STATE_ACTIVE
};
regx_register(&fs);
```

Another agent can discover it:
```c
regx_selector_t sel = { .type = REGX_TYPE_FILESYSTEM,
                         .capability = "snapshot" };
regx_entry_t out[4];
size_t n = regx_enumerate(&sel, out, 4);
```


## Userland CLI – `regxctl`

`regxctl` exposes registry information to users and scripts.  The CLI obtains
its data through system calls mirroring the kernel API.

### Commands

- `regxctl list` – enumerate all entries.
- `regxctl query <id>` – show basic info for a specific entry.
- `regxctl manifest <id>` – dump the manifest in a human readable form.
- `regxctl tree` – print the device hierarchy using `parent_id` links.

The [`regxctl.c`](../user/agents/regxctl/regxctl.c) skeleton demonstrates the
basic structure.  Real implementations would handle IPC/syscalls and JSON
rendering of manifests.

## Design Notes

* **Atomicity** – All registry mutations occur under a spinlock and update a
  generation number.  Readers see consistent snapshots and can detect changes.
* **Security** – Every entry carries a signature and declared permissions.  The
  kernel verifies these fields before allowing registration or updates.
* **Extensibility** – Manifests are plain structs that can be extended or
  replaced by CBOR/JSON without altering the runtime API.  `regx_selector_t`
  provides flexible filtering for future capabilities.
* **Auditing** – Because updates increment `generation`, a history of changes can
  be tracked externally to support system auditing and rollback.

RegX turns the entire operating system into a discoverable, self‑describing
collection of agents and resources, enabling live upgrades and deep introspection
from both kernel and userland.
