# NOSM Modules

NOSM modules are self-describing agents that extend NitrOS. Each module is
packaged with a manifest detailing its name, version, capabilities and
required permissions. Modules can be written in C, Rust or other languages that
conform to the N2 ABI.

## Key characteristics

* **Signed and versioned** – manifests include cryptographic signatures and
  semantic versions so the kernel can verify integrity.
* **Hot-loadable** – modules may be loaded, upgraded or unloaded at runtime
  without rebooting the system.
* **Capability scoped** – manifests declare the capabilities and privileges
  required by the module, enforcing least privilege.
* **Language neutral** – the ABI supports multiple implementation languages
  including C and Rust.

## Lifecycle

1. **Load** – the O2 boot agent or userland loader places the module in memory.
2. **Registration** – the module calls `n2_agent_register()` with a pointer to
   its manifest.
3. **Operation** – services are exposed through the module's entry point and
   discovered via the agent registry.
4. **Upgrade/Unload** – modules can be replaced or removed via
   `n2_agent_unregister()` with all resources cleaned up.

