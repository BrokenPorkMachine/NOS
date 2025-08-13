# Hardware Abstraction Layer (HAL)

The HAL provides a thin wrapper around the RegX registry so hardware devices,
drivers and buses can be described and discovered in a uniform way.

## API

```
void hal_init(void);
void hal_shutdown(void);

uint64_t hal_register(const hal_descriptor_t *desc, uint64_t parent_id);
int      hal_unregister(uint64_t id);
const regx_entry_t *hal_query(uint64_t id);
size_t   hal_enumerate(const regx_selector_t *sel, regx_entry_t *out, size_t max);
```

Each registration maps directly to a RegX entry. `hal_init()` creates a
root bus under which all HAL-managed devices are attached. Calls to
`hal_register()` default to this root when no parent is supplied.

\During system startup the kernel invokes `hal_init()` and registers core
hardware components such as the USB and PS/2 buses, SATA controller, block
device layer and network stack. These registrations appear in RegX and may
be queried by other agents to discover available hardware.

The HAL does not perform device I/O; it purely manages discovery and
lifecycle in cooperation with RegX.
