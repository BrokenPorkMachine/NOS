import subprocess
import shutil

import pytest


def run_qemu():
    subprocess.run(["make"], check=True)
    try:
        result = subprocess.run(
            [
                "qemu-system-x86_64",
                "-cpu",
                "max",
                "-bios",
                "/usr/share/ovmf/OVMF.fd",
                "-drive",
                "file=disk.img,format=raw",
                "-drive",
                "file=fs.img,format=raw",
                "-m",
                "512M",
                "-netdev",
                "user,id=n0",
                "-device",
                "e1000,netdev=n0",
                "-device",
                "i8042",
                "-device",
                "qemu-xhci",
                "-device",
                "usb-kbd",
                "-serial",
                "stdio",
                "-display",
                "none",
                "-no-reboot",
                "-no-shutdown",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=10,
            text=True,
        )
        out = result.stdout
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode()
    return out


@pytest.mark.skipif(shutil.which("qemu-system-x86_64") is None, reason="qemu-system-x86_64 not installed")
def test_boot_sequence():
    out = run_qemu()
    sequence = ["[nboot]", "[O2]", "[N2]", "[regx]", "[init]", "[login]"]
    last = -1
    for marker in sequence:
        idx = out.find(marker)
        assert idx != -1 and idx > last, f"{marker} missing or out of order"
        last = idx


if __name__ == "__main__":
    run_qemu()
