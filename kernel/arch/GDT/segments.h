#pragma once

/* GDT selector values for each privilege ring */
#define GDT_SEL_KERNEL_CODE 0x08
#define GDT_SEL_KERNEL_DATA 0x10
#define GDT_SEL_RING1_CODE  0x18
#define GDT_SEL_RING1_DATA  0x20
#define GDT_SEL_RING2_CODE  0x28
#define GDT_SEL_RING2_DATA  0x30
#define GDT_SEL_USER_CODE   0x38
#define GDT_SEL_USER_DATA   0x40

/* User-mode selectors with Ring 3 privilege level set */
#define GDT_SEL_USER_CODE_R3 (GDT_SEL_USER_CODE | 3)
#define GDT_SEL_USER_DATA_R3 (GDT_SEL_USER_DATA | 3)

