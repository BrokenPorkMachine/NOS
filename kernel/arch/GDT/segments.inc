; segments.inc — x86_64 GDT selector definitions & helper constants
; Keep selectors stable across the kernel. Code assumes:
;  - Kernel code is a 64-bit code segment (L=1, D=0, DPL=0)
;  - Data segments are flat/writable (DPL as noted)
;  - TSS is a 64-bit available TSS descriptor (occupies 16 bytes)

; ---------------------------
; GDT selectors (byte values)
; ---------------------------
%define GDT_SEL_KERNEL_CODE  0x08
%define GDT_SEL_KERNEL_DATA  0x10
%define GDT_SEL_RING1_CODE   0x18
%define GDT_SEL_RING1_DATA   0x20
%define GDT_SEL_RING2_CODE   0x28
%define GDT_SEL_RING2_DATA   0x30
%define GDT_SEL_USER_CODE    0x38
%define GDT_SEL_USER_DATA    0x40

; RPL=3 aliases for convenience (same table entry, different CPL on load)
%define GDT_SEL_USER_CODE_R3 (GDT_SEL_USER_CODE | 3)
%define GDT_SEL_USER_DATA_R3 (GDT_SEL_USER_DATA | 3)

; ---------------------------
; Optional system selectors
; ---------------------------
; 64-bit TSS descriptor (16 bytes) lives at 0x48..0x4F (low 8) and 0x50..0x57 (high 8)
; If you install a TSS in the GDT, use this selector with LTR.
%define GDT_SEL_TSS          0x48

; (If you ever add an LDT: it’s another 16-byte system descriptor)
;%define GDT_SEL_LDT          0x58

; ---------------------------
; Indices (entry numbers)
; ---------------------------
%define GDT_IDX(sel)         ((sel) >> 3)
%define GDT_IDX_KERNEL_CODE  GDT_IDX(GDT_SEL_KERNEL_CODE)
%define GDT_IDX_KERNEL_DATA  GDT_IDX(GDT_SEL_KERNEL_DATA)
%define GDT_IDX_RING1_CODE   GDT_IDX(GDT_SEL_RING1_CODE)
%define GDT_IDX_RING1_DATA   GDT_IDX(GDT_SEL_RING1_DATA)
%define GDT_IDX_RING2_CODE   GDT_IDX(GDT_SEL_RING2_CODE)
%define GDT_IDX_RING2_DATA   GDT_IDX(GDT_SEL_RING2_DATA)
%define GDT_IDX_USER_CODE    GDT_IDX(GDT_SEL_USER_CODE)
%define GDT_IDX_USER_DATA    GDT_IDX(GDT_SEL_USER_DATA)
%define GDT_IDX_TSS          GDT_IDX(GDT_SEL_TSS)

; ---------------------------
; Handy aliases for code
; ---------------------------
%define KERNEL_CS            GDT_SEL_KERNEL_CODE
%define KERNEL_DS            GDT_SEL_KERNEL_DATA
%define USER_CS              GDT_SEL_USER_CODE_R3
%define USER_DS              GDT_SEL_USER_DATA_R3

; ---------------------------
; Access/Granularity bits (for building descriptors in C/ASM)
; ---------------------------
; Access byte: P | DPL | S | Type
%define ACC_P                0x80
%define ACC_DPL0             0x00
%define ACC_DPL1             0x20
%define ACC_DPL2             0x40
%define ACC_DPL3             0x60
%define ACC_S_CODEDATA       0x10         ; 1 = code/data, 0 = system
%define ACC_TYPE_CODE        0x08         ; executable
%define ACC_TYPE_DATA        0x00         ; data
%define ACC_TYPE_RW          0x02         ; readable code / writable data

; Canonical access presets (match 0x9A/0x92 patterns)
%define ACC_CODE64_DPL0      (ACC_P | ACC_DPL0 | ACC_S_CODEDATA | ACC_TYPE_CODE | ACC_TYPE_RW) ; 0x9A
%define ACC_DATA_DPL0        (ACC_P | ACC_DPL0 | ACC_S_CODEDATA | ACC_TYPE_DATA | ACC_TYPE_RW) ; 0x92
%define ACC_CODE64_DPL3      (ACC_P | ACC_DPL3 | ACC_S_CODEDATA | ACC_TYPE_CODE | ACC_TYPE_RW) ; 0xFA
%define ACC_DATA_DPL3        (ACC_P | ACC_DPL3 | ACC_S_CODEDATA | ACC_TYPE_DATA | ACC_TYPE_RW) ; 0xF2

; TSS access (system descriptor, S=0)
%define ACC_TSS_AVAIL        (ACC_P | 0x09)   ; available 64-bit TSS
%define ACC_TSS_BUSY         (ACC_P | 0x0B)   ; busy 64-bit TSS

; Granularity byte high nibble: lim[19:16] | AVL | L | D/B | G
%define GRAN_G               0x80             ; 4KiB granularity
%define GRAN_DB              0x40             ; D/B: must be 0 for 64-bit code
%define GRAN_L               0x20             ; 64-bit code segment
%define GRAN_AVL             0x10

; Presets:
;  - 64-bit code: L=1, D/B=0, G=1 (limit ignored in long mode for code)
%define GRAN_CODE64          (GRAN_G | GRAN_L)
;  - data: L=0, D/B ignored in long mode; keep DB=0 to be tidy, G=1
%define GRAN_DATA            (GRAN_G)

; ---------------------------
; Sanity (doc only; enforce in C/ASM where possible)
;  * Kernel CS must point to a 64-bit code descriptor (L=1, D=0, DPL=0)
;  * User CS must be 64-bit code (L=1, D=0, DPL=3)
;  * Data segments writable, flat
;  * TSS selector used with LTR after GDT load
; ---------------------------
