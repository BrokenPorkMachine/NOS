# Planned Network Servers

This document outlines the network services for NitrOS. A very small network stack with a loopback device is now present in `Net/`. It can send and receive data within the system but does not talk to real hardware yet. The VNC, SSH, and FTP servers use this loopback interface for simple echo demonstrations until higher level protocols are implemented.

## VNC Server

- **Purpose**: Expose the system display over a Virtual Network Computing (VNC) protocol so that a remote client can view and control the NitrOS console.
- **Status**: Now sends a "not implemented" notice over the loopback network stack.
- **Future work**: Requires keyboard/mouse events over the network and a frame buffer driver.

## SSH Server with SCP Support

- **Purpose**: Offer a secure remote shell over the network with optional file copy using the SCP protocol.
- **Status**: Provides an echo shell using the loopback stack. Encryption and real networking are not yet present.
- **Future work**: Implement key exchange, authentication, encryption, and integration with the shell server.

## FTP Server

- **Purpose**: Provide file transfer capabilities for legacy clients.
- **Status**: Replies to simple commands using the loopback stack. Real file transfer and TCP/IP remain TODO.
- **Future work**: Build on the NitrFS filesystem once a TCP/IP stack is available.

Each of these services is started as a kernel thread during system initialization. They use the loopback network stack for testing but remain placeholders until true network drivers and protocols are added.
