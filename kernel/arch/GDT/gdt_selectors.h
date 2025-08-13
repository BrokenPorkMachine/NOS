#pragma once

/* Must match your GDT layout. From your logs:
   CS=0x0008, DS=0x0010, TR(TSS)=0x0028 */
#define KERNEL_CS  0x0008
#define KERNEL_DS  0x0010
#define KERNEL_TSS 0x0028
