#pragma once

#define IA32_APIC_BASE 0x1B


#define HIGHADDR(x) ( 0xFFFF800000000000 | (qword)(x) )
#define PAGE_SIZE ((qword)0x1000)
#define PAGE_MASK ((qword)0x0FFF)

#define IDT_BASE HIGHADDR(0x0400)
#define SYSINFO_BASE HIGHADDR(0x0A00)
#define MPAREA_BASE HIGHADDR(0x1000)
#define PDPT_LOW_PBASE (0x4000)
#define PDPT_HIGH_PBASE (0x5000)	//PDPT area for sys area
#define PDT0_PBASE (0x6000)
#define PT0_PBASE (0x7000)
//#define PMMWMP_BASE HIGHADDR(0x00200000)	//PBASE at 0x100000 ???
//#define KRNL_STACK_TOP HIGHADDR(0x200000)

#define APIC_PBASE ((qword)0xFEE00000)