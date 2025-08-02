# Planned Network Servers

This document outlines the network services for NitrOS.  A small loopback
network stack lives in `Net/` and allows local user-mode servers to exchange
packets without real hardware.  Each service communicates over a dedicated
logical port so they no longer interfere with one another.  The VNC, SSH, and
FTP servers remain simple demonstrations until higher level protocols are
implemented.

## VNC Server

- **Purpose**: Expose the system display over a Virtual Network Computing (VNC) protocol so that a remote client can view and control the NitrOS console.
- **Status**: Sends a greeting on port 1 of the loopback stack and responds to `ping` with `pong`.
- **Future work**: Requires keyboard/mouse events over the network and a frame buffer driver.

## SSH Server with SCP Support

- **Purpose**: Offer a secure remote shell over the network with optional file copy using the SCP protocol.
- **Status**: Provides a line-based echo shell on port 2 of the loopback stack. Commands such as `exit` close the session. Encryption and real networking are not yet present.
- **Future work**: Implement key exchange, authentication, encryption, and integration with the shell server.

## FTP Server

- **Purpose**: Provide file transfer capabilities for legacy clients.
- **Status**: Responds on port 3 of the loopback stack. After a greeting, it processes `LIST`, `RETR`, `STOR`, and `QUIT` commands using the NitrFS server for storage. Commands are terminated with CRLF and trimmed before processing.
- **Example**:
  ```text
  LIST
      (files listed)
  STOR demo.txt hello world
  RETR demo.txt
  QUIT
  ```
- **Limitations**: Only the loopback device is supported and transfers are in-memory; there is no authentication or real TCP/IP stack yet.
- **Future work**: Build on the NitrFS filesystem once a TCP/IP stack is available and hardware drivers are implemented.

Each of these services is started as a kernel thread during system initialization. They use the loopback network stack (ports 1–3) for testing but remain placeholders until true network drivers and protocols are added.
