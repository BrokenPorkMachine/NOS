\$1

## Appendix: N2 vs XNU (macOS Kernel)

Here’s how the N2 kernel of NitrOS is fundamentally different from Apple’s XNU (macOS kernel):

---

### 1. **True Modularity and Agent Architecture**

* **N2:**

  * Modular “agent” architecture: kernel, modules, filesystem, and bootloader are all self-describing, hot-reloadable, and versioned agents.
  * NOSM modules are signed, manifest-based, introspectable, and designed for easy live upgrades.
  * Every part of the system exposes a machine-readable manifest and can be queried at runtime.
* **XNU:**

  * Hybrid monolithic/microkernel, but only some drivers and kexts are loadable.
  * Kexts are less self-describing, not easily introspectable or upgradable at runtime, and not all kernel functionality is modular.

---

### 2. **Security & Integrity**

* **N2:**

  * Security-first by default: Every agent/module is signed, versioned, and sandboxed.
  * Manifest-driven privilege and capability declaration for all code loaded into kernel space.
  * Agents can only do what they declare; everything is least-privilege and auditable.
* **XNU:**

  * Gatekeeper, kext signing, SIP, and sandboxing exist, but some are optional, retrofitted, or inconsistent across the system.

---

### 3. **Hot Swapping and Live Upgrade**

* **N2:**

  * Any agent or module—including kernel services—can be hot-upgraded, rolled back, or swapped at runtime with zero downtime.
  * Robust lifecycle management for agents: load, register, upgrade, unload with automatic cleanup.
* **XNU:**

  * No true hot-patching of core kernel code; kexts can be unloaded/loaded, but not always cleanly or without reboot.

---

### 4. **Discovery & Introspection**

* **N2:**

  * Agent registry and introspection API: userland and other modules can discover, query, and manage all agents, modules, and filesystems at runtime.
  * Consistent manifest and metadata for all components.
* **XNU:**

  * No built-in runtime discovery of all kernel services, and userland access is limited.

---

### 5. **Filesystem & Storage**

* **N2:**

  * NitrFS is fully transactional, versioned, atomic, and journaled by design.
  * Snapshots, deduplication, and rollback are built-in.
  * Filesystem agent is modular and hot-replaceable, not hardwired.
* **XNU:**

  * APFS is robust, but some operations aren’t atomic at the OS level, and filesystem drivers are not designed to be hot-swapped.

---

### 6. **Language Neutrality and Future Proofing**

* **N2:**

  * NOSM modules can be written in C, Rust, WebAssembly, or other languages.
  * Kernel and modules communicate via versioned, introspectable ABIs.
  * Design supports “kernel containers” for future language or security upgrades.
* **XNU:**

  * Primarily C and C++, kernel extensions are C-based.
  * No WASM, no explicit ABI versioning for modules.

---

### 7. **Manifest-Based Everything**

* **N2:**

  * Every agent/module has a manifest (JSON/CBOR/struct) stating its name, version, dependencies, capabilities, permissions, and signature.
  * Boot, runtime, and security policies are driven by machine-readable manifests—no opaque blobs.
* **XNU:**

  * Plist files are used for kexts, but manifest-driven runtime and security is not enforced kernel-wide.

---

### 8. **Unified System Design**

* **N2:**

  * O2 bootloader, N2 kernel, NOSM modules, and NitrFS all use a single, unified, extensible system ABI.
  * All system components communicate and integrate using a discoverable, hot-upgradable model.
* **XNU:**

  * Mix of Mach APIs, BSD syscalls, and legacy interfaces.

---

#### **Summary Table**

| Feature             | **N2 (NitrOS)**                                  | **XNU (macOS)**          |
| ------------------- | ------------------------------------------------ | ------------------------ |
| Modularity          | Agents & manifest modules (NOSM), hot-reloadable | Kexts, some monolithic   |
| Security            | Signed, sandboxed, manifest-based permissions    | Kext signing, SIP        |
| Hot Swap            | Yes, for any agent/module                        | No, only kexts, limited  |
| Filesystem          | Transactional, atomic, hot-swappable (NitrFS)    | APFS, not hot-swap       |
| Language neutrality | C, Rust, WASM, more                              | C, C++ only              |
| Discovery           | Registry/introspection for all agents/modules    | None                     |
| System ABI          | Unified, extensible for all system layers        | Hybrid Mach/BSD/legacy   |
| Upgrade Model       | Live, atomic, rollback/forward                   | Requires reboot for core |

---

## **Agent Types**

* **Boot Agents:**
  UEFI-based bootloader and pre-N2 loaders.
* **N2 Kernel Agent:**
  The N2 microkernel/hybrid kernel, including module loaders, security managers, and core schedulers.
* **Module Agents (NOSM):**
  Dynamically loadable N2 modules (“NOSM” files), implementing drivers, filesystems, services, and more.
* **Userland Agents:**
  Privileged system daemons and user processes that interact with N2 and modules via a secure API.
* **Filesystem Agents (NitrFS):**
  Both in-N2 and user-facing utilities for transactional, versioned, and secure storage.
* **Module Agents (NOSM):**
  Dynamically loadable N2 modules (“NOSM” files), implementing drivers, filesystems, services, and more.
* **Userland Agents:**
  Privileged system daemons and user processes that interact with the N2 and modules via a secure API.
* **Filesystem Agents (NitrFS):**
  Both in-N2 and user-facing utilities for transactional, versioned, and secure storage.

---

## **Core Agents & Responsibilities**

### **1. O2 Boot Agent**

* **Role:** Securely loads the NitrOS N2 and all required NOSM modules at boot.
* **Responsibilities:**

  * Validate and authenticate all loaded binaries (N2, modules, boot config).
  * Pass a complete manifest (“bootinfo”) to the N2 (memory map, modules, config, ACPI, etc).
  * Support for fallback and recovery boot.
  * Provide clear, auditable logs: `[O2] ...`

---

### **2. N2 Kernel Agent**

* **Role:** The core execution environment and service coordinator of NitrOS.
* **Responsibilities:**

  * Enforce memory protection, isolation, and resource management.
  * Provide the system call API for userland and module agents.
  * Load, sandbox, and manage NOSM N2 modules.
  * Expose registry for agent discovery, introspection, and status.
  * Support hot reloading and live upgrades for N2 modules and agents.

---

### **3. NOSM Module Agents**

* **Role:** Dynamically extend the N2 with new drivers, filesystems, network stacks, or security logic.
* **Responsibilities:**

  * Must be self-describing, signed, and versioned (`.nosm` format).
  * Register/unregister themselves with the N2, exposing their services and interfaces.
  * Declare their capabilities, required privileges, dependencies, and API surface in the embedded manifest.
  * Clean up all resources/state on unload to guarantee reliability and security.
  * Support multi-language development (C, Rust, optionally WebAssembly and more).

---

### **4. NitrFS Agent**

* **Role:** Provides next-gen transactional, versioned, and secure filesystem services to N2 and userland.
* **Responsibilities:**

  * All operations are atomic, verifiable, and journaled.
  * File/metadata operations support robust ACLs, timestamps, and xattrs.
  * Supports snapshot, rollback, and real-time integrity verification.
  * Userland interacts via the `nitrfsctl` utility and syscalls for management, mounting, snapshotting, and recovery.

---

### **5. Userland Agents**

* **Role:** High-level system daemons and tools (`nosmctl`, `nitrfsctl`, etc), as well as user applications.
* **Responsibilities:**

  * Interface with the N2 via well-defined syscalls and IPC.
  * Discover available agents and capabilities dynamically.
  * Support live module loading/unloading and system introspection.
  * Provide UI/CLI/REST access to system status and agent management.
  * Help enforce system-wide policy (security, updates, auditing).

---

## **Design Principles for All Agents**

* **Security First:**
  All agents/modules are signed, versioned, sandboxed, and capability-scoped by default.
* **Manifest-Based:**
  Every agent (N2, module, userland, FS) must provide a machine-readable manifest for introspection and dependency resolution.
* **Hot Reloadable:**
  Agents can be upgraded, swapped, or rolled back live with no downtime.
* **Introspectable:**
  System state, agent registry, and capabilities are always discoverable at runtime.
* **Language Neutral:**
  Agent interfaces support multiple languages and ABIs for rapid evolution.

---

## **Agent Lifecycle**

1. **Discovery:**
   On boot, UEFI agent discovers all available modules, verifies, and passes to N2.
2. **Initialization:**
   Kernel agent loads and starts core agents (drivers, filesystems, security, etc).
3. **Registration:**
   Each NOSM and Userland Agent registers with the N2 registry and declares its interfaces.
4. **Operation:**
   Agents cooperate, communicate, and extend the system as needed.
5. **Upgrade/Unload:**
   Agents can be upgraded or removed cleanly at runtime, with full resource reclamation.

---

## **Sample: NOSM Module Manifest (JSON)**

```json
{
  "name": "NitrFS",
  "version": "1.2.0",
  "author": "NitrOS Core Team",
  "description": "Transactional, secure, versioned filesystem",
  "abi": "NitrOS-1.0",
  "entrypoint": "nitrfs_init",
  "dependencies": ["core", "crypto"],
  "capabilities": ["filesystem", "snapshot", "rollback"],
  "permissions": ["read_disk", "write_disk"],
  "signature": "BASE64-ENCODED-SIGNATURE"
}
```

---

## **Agent Registry & Query Example**

Agents are registered and discoverable via the kernel agent registry:

```bash
$ nosmctl list
Name         Version    Status      Capabilities
-------------------------------------------------------
NitrFS       1.2.0      Active      filesystem,snapshot
VirtNet      0.9.1      Active      network,virtual
AudioDrv     0.1.3      Loaded      audio
...
```

---

## **Inspiration & Comparison**

* **NOSM** modules = “kexts” (macOS) + “kmods” (Linux) + “WebAssembly for N2”
* **NitrFS** is to NitrOS as APFS is to macOS, but more open and introspectable
* **Agents** are like daemons/services, but are first-class, discoverable, and manageable

---

## **Contributing New Agents**

To write a new agent:

* Start with the provided manifest template and agent API.
* Implement all required registration, cleanup, and capabilities.
* Sign your module and test using `nosmctl`.
* Document all exported interfaces for other agents to discover.

---

For more details, see [docs/NOSM.md](docs/NOSM.md), [docs/NitrFS.md](docs/NitrFS.md), and [N2 API documentation](docs/api.md).

---

**NitrOS AGENTS: The future of modular, self-healing, secure operating systems.**
