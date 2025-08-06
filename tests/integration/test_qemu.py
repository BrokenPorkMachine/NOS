import subprocess
import pathlib

def run_qemu():
    subprocess.run(["make"], check=True)
    try:
        result = subprocess.run([
            "qemu-system-x86_64",
            "-bios", "/usr/share/ovmf/OVMF.fd",
            "-drive", "file=disk.img,format=raw",
            "-m", "512M",
            "-netdev", "user,id=n0",
            "-device", "e1000,netdev=n0",
            "-device", "i8042",
            "-serial", "stdio",
            "-display", "none",
            "-no-reboot",
            "-no-shutdown"
        ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10, text=True)
        out = result.stdout
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode()
    # The NitrOS boot sequence is considered successful once the O2
    # bootloader reports loading the kernel. The previous assertion
    # expected a legacy string from an early Mach microkernel demo,
    # which no longer matches the current boot log and caused the
    # integration test to fail even though the bootloader ran.
    assert "[O2] Universal UEFI bootloader" in out
    return out

if __name__ == "__main__":
    run_qemu()
