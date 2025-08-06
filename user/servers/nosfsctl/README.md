# nosfsctl

`nosfsctl` is a tiny example utility showing how userland tools interact with the
NOSFS filesystem agent via IPC.  Rather than linking directly against filesystem
internals, tools send requests to the agent and receive replies.  This keeps the
filesystem hot‑swappable and enforces capability boundaries.

The sample code issues a `NOSFS_MSG_LIST` request to enumerate the root
directory.  Real implementations would perform agent discovery and handle
permissions, multi‑tenant routing, and error handling.
