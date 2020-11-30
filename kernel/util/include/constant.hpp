#pragma once

#define ENABLE_DEBUGGER



#define IA32_APIC_BASE 0x1B
#define APIC_PBASE ((qword)0xFEE00000)

#define HIGHADDR(x) ( 0xFFFF800000000000 | (qword)(x) )
#define LOWADDR(x) ( 0x00007FFFFFFFFFFF & (qword)(x) )

#define PAGE_SIZE ((qword)0x1000)
#define PAGE_MASK ((qword)0x0FFF)

#define IDT_BASE HIGHADDR(0x0400)
#define SYSINFO_BASE HIGHADDR(0x0A00)

#define PDPT0_PBASE (0x4000)
#define PDPT8_PBASE (0x5000)	//PDPT area for sys area
#define PDT0_PBASE (0x6000)
#define PT0_PBASE (0x7000)
#define PT_KRNL_PBASE (0x8000)

#define MAP_TABLE_BASE HIGHADDR(0x9000)
#define MAP_VIEW_BASE HIGHADDR(0x00200000)

#define PMMSCAN_BASE HIGHADDR(0x1000)
#define PMMBMP_BASE HIGHADDR(0x00400000)
#define PMMBMP_PT_PBASE (0xA000)



