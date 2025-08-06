# Planned Network Servers

This document outlines the network services for NitrOS.  A small network stack
in `Net/` allows user-mode servers to exchange packets even without real
hardware.  Each service communicates over a dedicated logical port so they no
longer interfere with one another.  The stack tracks a basic IPv4 address and
can now answer ARP requests, paving the way for real hardware networking. A
brief overview of the stack and how current servers make use of it follows.

## Network Stack

- **Purpose**: Provide a minimal, message-oriented networking layer for
  user-space services.
- **Status**: Supports a loopback device with port multiplexing, a fixed IPv4
  address and rudimentary ARP replies.
- **Future work**: Introduce UDP/TCP modules and hardware NIC drivers so that
  the existing servers can communicate beyond the emulator.

## VNC Server

- **Purpose**: Expose the system display over a Virtual Network Computing (VNC) protocol so that a remote client can view and control the NitrOS console.
- **Status**: Sends a greeting on port 1 of the loopback stack and responds to `ping` with `pong`.
- **Future work**: Requires keyboard/mouse events over the network and a frame
  buffer driver so remote users can interact with the login server and shell.

## SSH Server with SCP Support

- **Purpose**: Offer a secure remote shell over the network with optional file copy using the SCP protocol.
- **Status**: Provides a line-based echo shell on port 2 of the loopback stack.
  Commands such as `exit` close the session. Encryption and real networking are
  not yet present.
- **Future work**: Implement key exchange, authentication, encryption, and
  integration with the login server so remote users can launch the shell after
  successful authentication.

## FTP Server

- **Purpose**: Provide file transfer capabilities for legacy clients.
- **Status**: Responds on port 3 of the loopback stack. After a greeting, it processes `LIST`, `RETR`, `STOR`, and `QUIT` commands using the NOSFS server for storage. Commands are terminated with CRLF and trimmed before processing.
- **Example**:
  ```text
  LIST
      (files listed)
  STOR demo.txt hello world
  RETR demo.txt
  QUIT
  ```
- **Limitations**: Only the loopback device is supported and transfers are in-memory; there is no authentication or real TCP/IP stack yet.
- **Future work**: Build on the NOSFS filesystem once a TCP/IP stack is available and hardware drivers are implemented.

## Login Server

- **Purpose**: Authenticate local users and expose the current IP address for
  remote services such as SSH and VNC.
- **Status**: Displays the network address at boot and launches the shell after
  checking credentials against an in-memory table.
- **Future work**: Accept credentials from the SSH server via IPC and manage
  multiple concurrent sessions.

## NOSFS Integration

- **Purpose**: Provide backing storage for networked services. The FTP server
  already stores data in NOSFS, and SCP will reuse the same interface.
- **Status**: RAM-backed filesystem with helpers for listing, deleting and
  verifying file contents. An export/import facility allows the filesystem to be
  snapshotted as a block image.
- **Future work**: Map NOSFS images onto real block devices and add access
  controls for network clients.

Each of these services is started as a kernel thread during system initialization. They use the loopback network stack (ports 1–3) for testing but remain placeholders until true network drivers and protocols are added.
