# NitrOS Serial Console

This short guide explains how to capture kernel logs over the COM1 serial port.

## Overview

The kernel initializes a basic driver for the first serial port at boot.
All log messages printed to the VGA console are also sent to COM1.
The port runs at **38400** baud using 8 data bits, no parity, and one stop bit (8N1).

## Using QEMU

Run QEMU with the `-serial` option to attach the virtual COM1 to a host device.
The simplest approach is to print serial output directly to your terminal:

```sh
qemu-system-x86_64 -drive format=raw,file=disk.img \
  -bios /usr/share/OVMF/OVMF_CODE.fd \
  -serial stdio -display sdl
```

All boot messages will appear in the terminal window. You can also log to a
file instead by using `-serial file:boot.log`.

Note: Omitting the graphical window with `-display none` disables the VGA
output and PS/2 keyboard. The system will appear to hang after "[Stage 5]
Scheduler start" because the shell only prints to VGA.

## Real Hardware

When running on a physical machine connect a null-modem cable to the first
serial port and configure your terminal program for **38400 8N1**. Any boot
messages will be streamed over this connection, allowing early debugging even
before the framebuffer is initialized.

---

See the main [README](../README.md) for build and run instructions.
