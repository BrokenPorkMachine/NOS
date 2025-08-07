# Mach-O2 Specification

Mach-O2 extends the 64-bit Mach-O format with a manifest-driven agent
model used by NitrOS.  Each Mach-O2 binary embeds a self-describing
manifest and optional resources so that the kernel can verify and launch
agents without external metadata.

## New Load Command

```
#define LC_MACHO2INFO 0x80000035

struct macho2_info_command {
    uint32_t cmd;        /* LC_MACHO2INFO */
    uint32_t cmdsize;    /* sizeof(struct macho2_info_command) */
    uint64_t manifest_offset; /* file offset of manifest bytes */
    uint64_t manifest_size;   /* size of manifest */
    uint64_t reserved;        /* future expansion */
};
```

`LC_MACHO2INFO` records the location of the agent manifest.  The command
is placed in the normal load-command table and the `mach_header_64`
fields `ncmds` and `sizeofcmds` are updated accordingly.

## Sections

The manifest is stored in segment `__O2INFO` section `__manifest`.
Additional resources may be embedded using other sections within the
same segment (`__O2INFO,__<name>`).  Tools should mark these sections as
`S_REGULAR` and read-only.

## Manifest Format

The manifest is UTF‑8 JSON with the following fields:

```json
{
  "name": "echo",
  "type": "user-agent",
  "version": "1.0",
  "entry": "_start",
  "required_privileges": ["filesystem", "network"],
  "resources": [
    {"name": "logo", "offset": 0x2000, "size": 512}
  ]
}
```

* **name** – human readable identifier
* **type** – `kernel`, `module`, or `user-agent`
* **version** – semantic version string
* **entry** – symbol or address of entry point
* **required_privileges** – array of capabilities requested
* **resources** – optional table of named data blobs stored in other
  sections; offsets are raw file offsets

## Embedding Convention

Single-file agents embed the JSON directly in section
`__O2INFO,__manifest`.  Container agents distribute a directory with the
binary and a separate `manifest.json` file:

```
agent.mo2/
  binary            # Mach-O2 image without embedded manifest
  manifest.json     # standalone manifest
```

## Reserved Identifiers

* Load command value `0x80000035` is reserved for `LC_MACHO2INFO`.
* Segment name `__O2INFO` and section `__manifest` are reserved.
* Future revisions may define additional sections within `__O2INFO` or
additional fields inside `macho2_info_command`.

## Example Loader

The O2 bootloader implementation in `boot/src/O2.c` demonstrates loading
Mach-O images, locating the manifest, and transferring control to the
agent.

