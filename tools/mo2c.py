#!/usr/bin/env python3
import json, os, struct, sys, shutil

LC_SEGMENT_64 = 0x19
LC_MACHO2INFO = 0x80000035
HEADER_SIZE = 32
INFO_CMD_SIZE = 32


def prompt_manifest():
    name = input('Agent name: ').strip()
    atype = input('Agent type: ').strip()
    version = input('Version: ').strip()
    entry = input('Entry symbol: ').strip()
    privs = input('Required privileges (comma separated): ').split(',')
    privs = [p.strip() for p in privs if p.strip()]
    return {
        'name': name,
        'type': atype,
        'version': version,
        'entry': entry,
        'required_privileges': privs,
        'resources': []
    }


def create_directory(outdir, binary, manifest):
    os.makedirs(outdir, exist_ok=True)
    shutil.copy2(binary, os.path.join(outdir, 'binary'))
    with open(os.path.join(outdir, 'manifest.json'), 'w') as f:
        json.dump(manifest, f, indent=2)
    print('Created directory', outdir)


def embed_manifest(infile, outfile, manifest):
    with open(infile, 'rb') as f:
        data = bytearray(f.read())
    if len(data) < HEADER_SIZE:
        print('Input too small')
        sys.exit(1)
    magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved = struct.unpack_from('<IiiIIIII', data, 0)
    loadcmds = bytearray(data[HEADER_SIZE:HEADER_SIZE+sizeofcmds])
    body = data[HEADER_SIZE+sizeofcmds:]

    # adjust existing load commands offsets
    offset = 0
    while offset < len(loadcmds):
        cmd, cmdsize = struct.unpack_from('<II', loadcmds, offset)
        if cmd == LC_SEGMENT_64:
            fileoff = struct.unpack_from('<Q', loadcmds, offset + 40)[0]
            if fileoff != 0:
                struct.pack_into('<Q', loadcmds, offset + 40, fileoff + INFO_CMD_SIZE)
            nsects = struct.unpack_from('<I', loadcmds, offset + 64)[0]
            sect_off = offset + 72
            for _ in range(nsects):
                sec_off = struct.unpack_from('<I', loadcmds, sect_off + 48)[0]
                if sec_off != 0:
                    struct.pack_into('<I', loadcmds, sect_off + 48, sec_off + INFO_CMD_SIZE)
                sect_off += 80
        offset += cmdsize

    manifest_bytes = json.dumps(manifest, indent=2).encode('utf-8')
    new_lc = struct.pack('<IIQQQ', LC_MACHO2INFO, INFO_CMD_SIZE,
                         0, len(manifest_bytes), 0)

    new_header = struct.pack('<IiiIIIII', magic, cputype, cpusubtype,
                             filetype, ncmds + 1, sizeofcmds + INFO_CMD_SIZE,
                             flags, reserved)

    out = bytearray()
    out += new_header
    out += loadcmds
    newcmd_off = len(out)
    out += new_lc
    out += body
    manifest_off = len(out)
    out += manifest_bytes
    struct.pack_into('<Q', out, newcmd_off + 8, manifest_off)

    with open(outfile, 'wb') as f:
        f.write(out)
    print('Wrote', outfile)


def main():
    if len(sys.argv) != 3:
        print('usage: mo2c.py <input-macho> <output|outdir.mo2>')
        return 1
    infile, outpath = sys.argv[1:]
    manifest = prompt_manifest()
    if outpath.endswith('.mo2'):
        create_directory(outpath, infile, manifest)
    else:
        embed_manifest(infile, outpath, manifest)
    return 0

if __name__ == '__main__':
    sys.exit(main())
