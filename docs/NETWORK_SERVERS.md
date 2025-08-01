# Planned Network Servers

This document outlines proposed network services for NitrOS. The current code base does not include a full TCP/IP stack, so these services are provided as stub servers only.

## VNC Server

- **Purpose**: Expose the system display over a Virtual Network Computing (VNC) protocol so that a remote client can view and control the NitrOS console.
- **Status**: Placeholder implementation that simply logs a message at startup.
- **Future work**: Requires keyboard/mouse events over the network and a frame buffer driver.

## SSH Server with SCP Support

- **Purpose**: Offer a secure remote shell over the network with optional file copy using the SCP protocol.
- **Status**: Currently implemented as a stub. Networking and cryptographic components are not yet present.
- **Future work**: Implement key exchange, authentication, encryption, and integration with the shell server.

## FTP Server

- **Purpose**: Provide file transfer capabilities for legacy clients.
- **Status**: Placeholder only. It prints a message and yields the CPU.
- **Future work**: Build on the NitrFS filesystem once a TCP/IP stack is available.

Each of these services is started as a kernel thread during system initialization. They do not yet handle any network traffic but serve as a starting point for future development.
