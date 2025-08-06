# N2 API Overview

The N2 kernel exposes a minimal API for agents and user programs. It centres on
runtime registration, discovery and syscall handling.

## Agent registry

* `n2_agent_register(const n2_agent_t *agent)` – register a new agent with the
  kernel registry.
* `n2_agent_unregister(const char *name)` – remove a previously registered
  agent.
* `n2_agent_find_capability(const char *cap)` – locate an agent that advertises
  a particular capability in its manifest.

## Syscalls

* `n2_syscall_register(uint32_t num, syscall_fn_t fn)` – install a syscall handler
  at runtime. The ABI is versioned and extensible.

This document will grow as additional kernel interfaces are finalised.

