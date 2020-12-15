#pragma once

//#define ENABLE_DEBUGGER



#define IA32_APIC_BASE 0x1B

#define HIGHADDR(x) ( (qword)0xFFFF800000000000 | (qword)(x) )
#define LOWADDR(x) ( (qword)0x00007FFFFFFFFFFF & (qword)(x) )

#define IS_HIGHADDR(x) ( ((qword)(x) & HIGHADDR(0)) == HIGHADDR(0) )

#define PAGE_SIZE ((qword)0x1000)
#define PAGE_MASK ((qword)0x0FFF)

#define IDT_BASE HIGHADDR(0x1000)
#define SYSINFO_BASE HIGHADDR(0x0A00)

#define PDPT0_PBASE (0x5000)
#define PDPT8_PBASE (0x6000)
#define PDT0_PBASE (0x7000)
#define PT0_PBASE (0x8000)
#define PT_KRNL_PBASE (0x9000)
#define PT_MAP_PBASE (0xA000)
#define DIRECT_MAP_TOP (0xB000)
#define BOOT_AREA_TOP (0x10000)

//#define APIC_PBASE ((qword)0xFEE00000)
#define LOCAL_APIC_VBASE HIGHADDR(0xD000)
#define IO_APIC_VBASE HIGHADDR(0xC000)

#define KRNL_STK_TOP HIGHADDR(0x001FF000)
#define MAP_TABLE_BASE HIGHADDR(PT_MAP_PBASE)
#define MAP_VIEW_BASE HIGHADDR(0x00200000)

#define PMMSCAN_BASE HIGHADDR(0x3000)
#define PMMBMP_BASE HIGHADDR(0x00400000)
#define PMMBMP_PT_PBASE (DIRECT_MAP_TOP)

#define PAGE_XD ((qword)1 << 63)
#define PAGE_GLOBAL ((qword)0x100)
#define PAGE_CD ((qword)0x10)
#define PAGE_WT ((qword)0x08)
#define PAGE_USER ((qword)0x04)
#define PAGE_WRITE ((qword)0x02)
#define PAGE_PRESENT ((qword)0x01)
