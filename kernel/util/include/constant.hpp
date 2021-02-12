#pragma once

#define MSR_APIC_BASE 0x1B
#define MSR_GS_BASE 0xC0000101

#define SEG_KRNL_CS 0x08
#define SEG_KRNL_SS 0x10
#define SEG_USER_CS 0x28
#define SEG_USER_SS 0x30

#define HIGHADDR(x) ( (qword)0xFFFF800000000000ULL | (qword)(x) )
#define LOWADDR(x) ( (qword)0x00007FFFFFFFFFFFULL & (qword)(x) )

#define IS_HIGHADDR(x) ( ((qword)(x) & HIGHADDR(0)) == HIGHADDR(0) )

#define PAGE_SIZE ((qword)0x1000)
#define PAGE_MASK ((qword)0x0FFF)

#define IDT_BASE HIGHADDR(0x0800)
#define IDT_LIM ((qword)0x400)
#define SYSINFO_BASE HIGHADDR(0x0C00)

#define PDPT0_PBASE ((qword)0x4000)
#define PDPT8_PBASE ((qword)0x5000)
#define PDT0_PBASE ((qword)0x6000)
#define PT0_PBASE ((qword)0x7000)
#define PT_KRNL_PBASE ((qword)0x8000)
#define PT_MAP_PBASE ((qword)0x9000)
#define DIRECT_MAP_TOP ((qword)0xA000)
#define BOOT_AREA_TOP ((qword)0x10000)

#define HPET_VBASE HIGHADDR(0xE000)
#define LOCAL_APIC_VBASE HIGHADDR(0xD000)
#define IO_APIC_VBASE HIGHADDR(0xC000)

#define KRNL_STK_TOP HIGHADDR(0x001FF000)
#define MAP_TABLE_BASE HIGHADDR(PT_MAP_PBASE)
#define MAP_VIEW_BASE HIGHADDR(0x00200000)

#define PMMSCAN_BASE HIGHADDR(0x1000)
#define PMMBMP_BASE HIGHADDR(0x00400000)
#define PMMBMP_PT_PBASE (DIRECT_MAP_TOP)

#define PAGE_XD ((qword)1 << 63)
#define PAGE_GLOBAL ((qword)0x100)
#define PAGE_CD ((qword)0x10)
#define PAGE_WT ((qword)0x08)
#define PAGE_USER ((qword)0x04)
#define PAGE_WRITE ((qword)0x02)
#define PAGE_PRESENT ((qword)0x01)
