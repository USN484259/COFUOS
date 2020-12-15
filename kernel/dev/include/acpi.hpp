#pragma once
#include "types.hpp"
#include "sysinfo.hpp"

#include "bugcheck.hpp"
#define THROW(x) BugCheck(corrupted,x)
#include "vector.hpp"
#undef THROW

namespace UOS{
	struct MADT{
		struct processor{
			byte uid;
			byte apic_id;
		};
		struct nmi{
			byte uid;
			byte pin;
			byte mode;
		};
		struct redirect{
			dword gsi;
			byte irq;	// 2 -> NMI
			byte mode;
		};
		qword local_apic_pbase = 0xFEE00000;
		dword io_apic_pbase = 0xFEC00000;
		dword gsi_base = 0;
		bool pic_present = true;
		vector<processor> processors;
		vector<nmi> nmi_pins;
		vector<redirect> redirects;

		MADT(void const* vbase);
	};
	struct FADT{	//see: https://wiki.osdev.org/FADT
		struct GenericAddressStructure {
			byte AddressSpace;
			byte BitWidth;
			byte BitOffset;
			byte AccessSize;
			qword Address;
		};

		dword FirmwareCtrl;
		dword Dsdt;
		byte  Reserved;
		byte  PreferredPowerManagementProfile;
		word  SCI_Interrupt;
		dword SMI_CommandPort;
		byte  AcpiEnable;
		byte  AcpiDisable;
		byte  S4BIOS_REQ;
		byte  PSTATE_Control;
		dword PM1aEventBlock;
		dword PM1bEventBlock;
		dword PM1aControlBlock;
		dword PM1bControlBlock;
		dword PM2ControlBlock;
		dword PMTimerBlock;
		dword GPE0Block;
		dword GPE1Block;
		byte  PM1EventLength;
		byte  PM1ControlLength;
		byte  PM2ControlLength;
		byte  PMTimerLength;
		byte  GPE0Length;
		byte  GPE1Length;
		byte  GPE1Base;
		byte  CStateControl;
		word  WorstC2Latency;
		word  WorstC3Latency;
		word  FlushSize;
		word  FlushStride;
		byte  DutyOffset;
		byte  DutyWidth;
		byte  DayAlarm;
		byte  MonthAlarm;
		byte  Century;
		// reserved in ACPI 1.0; used since ACPI 2.0+
		word BootArchitectureFlags;
		byte  Reserved2;
		dword Flags;
 
		// 12 byte structure; see below for details
		GenericAddressStructure ResetReg;
 
		byte  ResetValue;
		byte  Reserved3[3];
 
		// 64bit pointers - Available on ACPI 2.0+
		qword                X_FirmwareControl;
		qword                X_Dsdt;
 
		GenericAddressStructure X_PM1aEventBlock;
		GenericAddressStructure X_PM1bEventBlock;
		GenericAddressStructure X_PM1aControlBlock;
		GenericAddressStructure X_PM1bControlBlock;
		GenericAddressStructure X_PM2ControlBlock;
		GenericAddressStructure X_PMTimerBlock;
		GenericAddressStructure X_GPE0Block;
		GenericAddressStructure X_GPE1Block;
	};

	class ACPI{
		MADT* madt = nullptr;
		FADT* fadt = nullptr;
		static bool validate(const void* addr,size_t limit);

		void parse_table(qword pbase);
	public:
		ACPI(void);
		ACPI(const ACPI&) = delete;

		const MADT& get_madt(void) const;
		const FADT& get_fadt(void) const;
	};
	extern ACPI acpi;
}