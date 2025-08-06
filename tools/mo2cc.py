#!/usr/bin/env python3
import sys, os, subprocess, struct, hashlib, json, shutil

def align_up(val, align):
    return (val + align - 1) & ~(align - 1)

def fail(msg):
    print("Error:", msg)
    sys.exit(1)

def build_object(src, obj, lang='c'):
    if src.endswith('.S') or src.endswith('.asm'):
        # Assembly
        if shutil.which('nasm'):
            ret = subprocess.run(['nasm', '-f', 'elf64', '-o', obj, src])
        elif shutil.which('as'):
            ret = subprocess.run(['as', src, '-o', obj])
        else:
            fail("No assembler found (nasm/as)")
    elif src.endswith('.c') or src.endswith('.cpp'):
        cc = shutil.which('clang') or shutil.which('gcc')
        if not cc:
            fail("No C compiler found (clang/gcc)")
        cflags = ['-nostdlib', '-ffreestanding', '-fno-pie', '-no-pie', '-c']
        ret = subprocess.run([cc] + cflags + [src, '-o', obj])
    else:
        fail("Unknown source type")
    if ret.returncode != 0:
        fail(f"Compilation failed for {src}")

def extract_text_section(obj, binfile):
    # Use objcopy to extract .text as flat binary
    if not shutil.which('objcopy'):
        fail("objcopy not found")
    ret = subprocess.run(['objcopy', '-O', 'binary', '--only-section=.text', obj, binfile])
    if ret.returncode != 0:
        fail(f"objcopy failed for {obj}")

def find_entry_vaddr(obj, symbol='mo2_entry'):
    # Use nm to find symbol value
    if not shutil.which('nm'):
        fail("nm not found")
    try:
        out = subprocess.check_output(['nm', obj]).decode()
        for line in out.splitlines():
            if f" {symbol}" in line:
                addr = int(line.split()[0], 16)
                return addr
    except Exception:
        pass
    return 0x400000  # fallback

def main():
    if len(sys.argv) != 4:
        print("Usage: mo2cc.py <source.c/S> <manifest.json> <out.mo2>")
        sys.exit(1)

    src, manifestfile, outfile = sys.argv[1:]
    tmp_obj = "tmp_mo2.o"
    tmp_bin = "tmp_mo2.bin"

    build_object(src, tmp_obj)
    extract_text_section(tmp_obj, tmp_bin)

    # Read code
    with open(tmp_bin, 'rb') as f:
        code = f.read()
    # Read manifest
    with open(manifestfile, 'rb') as f:
        manifest = f.read()

    # Find entry vaddr
    entry_vaddr = find_entry_vaddr(tmp_obj)
    # For simplicity, place code at 0x400000, manifest at 0x500000
    code_vaddr = 0x400000
    manifest_vaddr = 0x500000

    hdr_size = 64
    seg_size = 64
    align = 0x1000
    code_off = align_up(hdr_size + seg_size*2, align)
    manifest_off = align_up(code_off + len(code), align)
    hash_off = align_up(manifest_off + len(manifest), 32)

    # Mach-O2 header
    header = struct.pack(
        '<IHHHHIIIQ',
        0x4D4F3200,  # magic
        1,           # version
        0x8664,      # x86_64
        2, 0,        # segments, reserved
        manifest_off, len(manifest),
        hash_off, 32,
        entry_vaddr if entry_vaddr else code_vaddr
    )

    seg_code = struct.pack(
        '<BBH I Q Q Q Q 16s',
        1, 5, 0, align,   # type=code, RX
        code_vaddr, code_off, len(code), len(code),
        b'code'
    )
    seg_manifest = struct.pack(
        '<BBH I Q Q Q Q 16s',
        3, 1, 0, 0x1000,  # type=manifest, R
        manifest_vaddr, manifest_off, len(manifest), len(manifest),
        b'manifest'
    )

    # Build the file
    file = bytearray()
    file += header
    file += seg_code
    file += seg_manifest
    while len(file) < code_off:
        file += b'\x00'
    file += code
    while len(file) < manifest_off:
        file += b'\x00'
    file += manifest
    while len(file) < hash_off:
        file += b'\x00'

    h = hashlib.sha256(file[:hash_off]).digest()
    file += h

    with open(outfile, 'wb') as f:
        f.write(file)

    print(f"Built {outfile}, entry @ 0x{entry_vaddr:x}")

    # Cleanup
    os.remove(tmp_obj)
    os.remove(tmp_bin)

if __name__ == '__main__':
    main()

