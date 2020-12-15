#pragma once

#include "types.hpp"

namespace UOS{
    class APIC{
	public:
		typedef void (*CALLBACK)(byte, qword);
		static constexpr size_t IRQ_MIN = 0x20;		//min handled IRQ
		static constexpr size_t IRQ_MAX = 0x40;		//max handled IRQ
		static constexpr byte IRQ_OFFSET = 0x24;	//offset for external IRQ
		static constexpr byte IRQ_APIC_TIMER = 0x20;
		static constexpr byte IRQ_SCI = 0x21;
		static constexpr byte IRQ_KEYBOARD = IRQ_OFFSET + 1;
		static constexpr byte IRQ_RTC = IRQ_OFFSET + 8;
		static constexpr byte IRQ_SATA = IRQ_OFFSET + 0x0E;
	private:
		struct irq_handler{
			CALLBACK callback;
			qword data;
		};
		irq_handler table[IRQ_MAX - IRQ_MIN];

    public:
		APIC(void);
		APIC(const APIC&) = delete;
		byte id(void);
		void dispatch(byte off_id);

		CALLBACK set(byte irq,CALLBACK callback,qword data);
    };
	extern APIC apic;
}
