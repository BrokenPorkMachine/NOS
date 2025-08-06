# NitrOS Development Roadmap

This roadmap outlines the core phases and milestones for building NitrOS—the next-generation modular, secure, and agent-driven operating system.

---

## **1. O2 Boot Agent**

* **Goal:**
  Build a secure, extensible UEFI bootloader for NitrOS, responsible for:

  * Loading the N2 kernel and NOSM modules.
  * Validating signatures and integrity.
  * Collecting system info (memory map, ACPI, graphics, modules).
  * Passing a manifest-rich `bootinfo_t` structure to the kernel.
* **Milestones:**

  * [ ] Minimal UEFI boot (load & jump to a test kernel)
  * [ ] Module and kernel loading from disk
  * [ ] Manifest/bootinfo building
  * [ ] Secure validation (signatures, hashes)
  * [ ] Recovery/fallback support
  * [ ] Logging/auditing

---

## **2. N2 Kernel**

* **Goal:**
  Develop the N2 microkernel/hybrid kernel:

  * Agent/manifest-based, hot-reloadable design.
  * Registry for modules (“agents”), syscalls, and services.
  * Secure agent sandboxing and lifecycle management.
  * Versioned, extensible syscall ABI for userland and modules.
  * Support for dynamic loading/unloading of NOSM modules.
* **Milestones:**

  * [ ] Boot and main agent registry
  * [ ] Syscall table and core API
  * [ ] Module loader (NOSM)
  * [ ] Sandbox and manifest enforcement
  * [ ] Hot upgrade/unload logic
  * [ ] Agent discovery API

---

## **3. NOSM Module System**

* **Goal:**
  Create the NitrOS Module format and loader:

  * Self-describing, signed `.nosm` binaries with manifests.
  * Multi-language (C, Rust, WASM) support.
  * Secure privilege, capability, and dependency declaration.
  * Hot-plug/hot-reload support.
* **Milestones:**

  * [ ] NOSM format spec and header
  * [ ] Minimal test module
  * [ ] Loader and signature verification in N2
  * [ ] Agent registration/unregistration logic
  * [ ] Example drivers/services as NOSM agents
  * [ ] nosmctl userland tool for management

---

## **4. NOSFS Filesystem Agent**

* **Goal:**
  Build the next-gen transactional, versioned filesystem for NitrOS:

  * Fully atomic operations and crash-proof journaling.
  * Metadata: xattrs, ACLs, snapshots, rollback, integrity.
  * Pluggable as a NOSM agent; discoverable and upgradable.
* **Milestones:**

  * [ ] NOSFS format and spec
  * [ ] Minimal block device & in-memory FS
  * [ ] Transactional core logic
  * [ ] Userland utility (nosfsctl)
  * [ ] Snapshots and rollback support
  * [ ] Integration with agent discovery and agent registry

---

## **5. NitroShell (nsh)**

* **Goal:**
  Provide an agent-based user interface for managing NOSFS and agents.
* **Milestones:**

  * [ ] Basic command set and IPC with NOSFS
  * [ ] Package and update management via nosmctl
  * [ ] Remote sessions over SSH or VNC

---

## **Long-term Integration**

* [ ] Cross-platform build system & CI
* [ ] Virtual machine and hardware testing
* [ ] Documentation, manifests, and user/contributor guides
* [ ] Security auditing and fuzzing
* [ ] Community extension API

---

**NitrOS Roadmap: Building the future of modular, hot-upgradable, self-healing operating systems.**
