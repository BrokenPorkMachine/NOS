# NitrOS Serial Console

This short guide explains how to capture kernel logs over the COM1 serial port.

## Overview

All kernel messages are emitted exclusively over the first serial port (COM1),
leaving the `/dev/console` device for userland such as the login agent.
Nothing is mirrored to the framebuffer by the kernel. The port runs at
**38400** baud using 8 data bits, no parity, and one stop bit (8N1).

## Using QEMU

Run QEMU with the `-serial` option to attach the virtual COM1 to a host device.
The simplest approach is to print serial output directly to your terminal:

```sh
qemu-system-x86_64 -drive format=raw,file=disk.img \
  -bios /usr/share/OVMF/OVMF_CODE.fd \
  -device i8042 -device qemu-xhci -device usb-kbd \
  -serial stdio -display none -vnc :0
```

All boot messages will appear in the terminal window. You can also log to a
file instead by using `-serial file:boot.log`.

The `-device i8042` option explicitly creates a PS/2 keyboard controller so
that input works once the login server starts, while `-device qemu-xhci -device
usb-kbd` adds a USB keyboard. The example above hides the graphical window
while exposing a VNC display on port 5900 for keyboard input.
If you prefer a local window replace `-display none -vnc :0` with `-display sdl`.

Note: Omitting both the graphical window and a VNC display disables the PS/2
keyboard. The system will appear to hang after "[Stage 5] Scheduler start"
because no keyboard input can reach the login server.

## Real Hardware

When running on a physical machine connect a null-modem cable to the first
serial port and configure your terminal program for **38400 8N1**. Any boot
messages will be streamed over this connection, allowing early debugging even
before the framebuffer is initialized.

---

See the main [README](../README.md) for build and run instructions.
