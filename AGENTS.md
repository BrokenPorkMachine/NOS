# AGENTS.md

## NitrOS Agents Architecture

Welcome to **NitrOS**—the next-generation operating system designed from the ground up for modularity, security, and rapid innovation.
This document outlines the major *agents* (core components and services) that work together to deliver NitrOS’s reliability, extensibility, and performance.

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
