# AGENTS.md

---

## Project: Mach Microkernel Operating System

**Purpose:**
Document the roles (“agents”) in the system, including both kernel and user-space actors, their responsibilities, and how they interact.
This file is intended as a high-level guide and technical reference for all contributors.

---

## **Agents Overview**

| Agent                | Privilege | Type/Scope  | Example Roles                              |
| -------------------- | --------- | ----------- | ------------------------------------------ |
| Bootloader           | Ring 0    | UEFI app    | Loads kernel, passes boot info, exits      |
| Mach Microkernel     | Ring 0    | Kernel      | Scheduling, memory, IPC, trap/syscall, IRQ |
| User Task/Thread     | Ring 3    | User proc   | Runs app/server code, uses syscalls, IPC   |
| NitrFS Server       | Ring 3    | User server | Secure in-memory filesystem              |
| Device Driver Server | Ring 3    | User server | Handles hardware via IPC (keyboard, disk)  |
| Window Server        | Ring 3    | User server | (Planned) Manages GUI, display, input      |
| IPC Subsystem        | Kernel    | Logic/Abstr | Manages message passing, port rights       |
| Network Server       | Ring 3    | User server | (Planned) TCP/IP stack, drivers, sockets   |
| Login/Session Agent  | Ring 3    | User server | Handles login prompt and user authentication |
| Supervisor/Update    | Ring 3    | User server | (Planned) System updates, integrity        |

---

## **1. Bootloader Agent**

* **Type:** UEFI Application (Ring 0, trusted boot-time only)
* **Responsibilities:**

  * Initialize platform via UEFI
  * Load the kernel from FAT/FAT32/EFI partition into memory
  * Pass optional boot info (memory map, video mode, etc.) to kernel
  * Exit after handing off execution to kernel
* **Interactions:**

  * Reads disk, passes control to kernel at specified entry point

---

## **2. Mach Microkernel Agent**

* **Type:** Kernel (Ring 0, privileged, always resident)
* **Responsibilities:**

  * Set up hardware abstraction (CPU, interrupts, PIT timer, paging/MMU)
  * Manage all system memory (physical and virtual)
  * Preemptive thread scheduling (timer-driven, round-robin or better)
  * Inter-process communication (Mach IPC: messages, ports, rights)
  * Trap/syscall entry and exit (int \$0x80 or syscall gates)
  * Enforce user/kernel separation and isolation
  * Handle device interrupts and mediate hardware access
* **Interactions:**

  * Context switches and schedules user agents
  * Handles system calls and IPC on behalf of user tasks
  * Receives and processes hardware IRQs

---

## **3. User Tasks & Threads (User Agents)**

* **Type:** User mode processes (Ring 3, unprivileged)
* **Responsibilities:**

  * Execute user application or server code
  * Use system calls and IPC to interact with kernel and other agents
  * Serve as Mach “servers”: file system, device drivers, networking, GUI, etc.
* **Examples:**

  * **NitrFS server:** Secure in-memory filesystem via IPC
* **Shell server:** Command interpreter using IPC with built-in file commands (`cd`, `ls`, `dir`, `mkdir`, `mv`)
  * **Device driver server:** Handles input/output to actual hardware, passes events/data to other agents
  * **Demo tasks:** Print to VGA, test syscalls, simple multi-threaded demos
* **Interactions:**

  * Trap into kernel for privileged actions (system calls, memory mapping)
  * Send and receive IPC messages to/from other user agents and kernel
  * Receive context switches and preemption via scheduler

---

## **4. IPC Subsystem (Mach Ports/Messages)**

* **Type:** Logical subsystem (kernel and user)
* **Responsibilities:**

  * Allow agents to send and receive messages (Mach messages)
  * Provide port abstractions and enforce port rights/security
  * Mediate and multiplex message passing between user agents and kernel
* **Interactions:**

  * User agents request port creation, rights, message sending/receiving via syscalls or IPC
  * Kernel delivers and routes messages as per rights and policy

---

## **5. Planned/Future System Agents**

* **Network Stack/Server:** User-space TCP/IP, NIC drivers, socket/port IPC
* **Window/Display Server:** Handles graphics output, user input, windowing
* **Login/Session Agent:** Provides login prompt and manages authentication
* **Supervisor/Update Agent:** System update, patching, and recovery

---

## **Agent Interaction Diagram (Textual)**

```
[Bootloader]
     |
     v
[Mach Microkernel]
  /   |   |   \
[App][FS][Dev][Window]
   \    |     /
     <==IPC==>
```

* **Bootloader:** Loads and starts the kernel, then exits.
* **Kernel:** Schedules and mediates all user agents. Handles IPC.
* **User Agents:** Communicate with each other and kernel via Mach IPC.
* **FS/Device/Window Servers:** Provide services to apps and each other via message passing.

---

## **Agent Interaction Table**

| From       | To              | Mechanism     | Purpose                        |
| ---------- | --------------- | ------------- | ------------------------------ |
| Bootloader | Kernel          | Direct jump   | Boot handoff                   |
| User task  | Kernel          | Syscall/trap  | System calls, IPC, mapping     |
| User task  | FS/Dev server   | Mach IPC      | File/dev/network requests      |
| Kernel     | User task       | Scheduler/IRQ | Preemption, async notification |
| Kernel     | All user agents | Mach IPC/IRQ  | Message delivery, interrupt    |
| Any agent  | Any agent       | Mach IPC      | Message, service, async        |

---

## **Summary: Agent Roles in the OS**

* **Bootloader:** Loads kernel, never returns
* **Kernel:** Scheduler, MMU, IPC, security, system calls, device IRQs
* **User Tasks:** All application logic, servers, drivers, networking, GUI
* **IPC Subsystem:** Messaging/port framework binding the whole OS
* **Servers:** (FS, device, network, display, etc) implemented as user agents
* **NitrFS:** Initial secure in-memory filesystem server

---

## **Development Notes**

* Agents may be further subdivided (e.g., supervisor, security monitor) as project grows.
* Keep this file updated as you add, rename, or split agent roles.

---

**This document is intended to serve as an authoritative reference for all project contributors and as architectural documentation for future review.**
