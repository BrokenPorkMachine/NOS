# Planned Network Servers

This document outlines the network services for NitrOS. A very small network stack with a loopback device is now present in `Net/`. It can send and receive data within the system but does not talk to real hardware yet.  Each service communicates over a dedicated logical port on the loopback stack so they no longer interfere with one another.  The VNC, SSH, and FTP servers remain simple demonstrations until higher level protocols are implemented.

## VNC Server

- **Purpose**: Expose the system display over a Virtual Network Computing (VNC) protocol so that a remote client can view and control the NitrOS console.
- **Status**: Sends a greeting on port 1 of the loopback stack.
- **Future work**: Requires keyboard/mouse events over the network and a frame buffer driver.

## SSH Server with SCP Support

- **Purpose**: Offer a secure remote shell over the network with optional file copy using the SCP protocol.
- **Status**: Provides an echo shell on port 2 of the loopback stack. Encryption and real networking are not yet present.
- **Future work**: Implement key exchange, authentication, encryption, and integration with the shell server.

## FTP Server

- **Purpose**: Provide file transfer capabilities for legacy clients.
- **Status**: Handles `LIST`, `RETR`, and `STOR` over port 3 of the loopback stack using NitrFS. Real file transfer and TCP/IP remain TODO.
- **Future work**: Build on the NitrFS filesystem once a TCP/IP stack is available.

Each of these services is started as a kernel thread during system initialization. They use the loopback network stack (ports 1–3) for testing but remain placeholders until true network drivers and protocols are added.
