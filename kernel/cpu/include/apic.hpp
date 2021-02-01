#pragma once

#include "types.hpp"
#include "constant.hpp"
#include "sync/include/spin_lock.hpp"

namespace UOS{
    class APIC{
	public:
		typedef void (*CALLBACK)(byte, void*);
		static constexpr size_t IRQ_MIN = 0x20;		//min handled IRQ
		static constexpr size_t IRQ_MAX = IDT_LIM/0x10;		//max handled IRQ
		static constexpr byte IRQ_OFFSET = IRQ_MIN + 4;	//offset for external IRQ
		static constexpr byte IRQ_CONTEXT_TRAP = IRQ_MIN;
		static constexpr byte IRQ_APIC_TIMER = IRQ_MIN + 1;
		static constexpr byte IRQ_SCI = IRQ_MIN + 2;
		static constexpr byte IRQ_KEYBOARD = IRQ_OFFSET + 1;
		static constexpr byte IRQ_RTC = IRQ_OFFSET + 8;
		static constexpr byte IRQ_MOUSE = IRQ_OFFSET + 0x0C;
		static constexpr byte IRQ_SATA = IRQ_OFFSET + 0x0E;
		
		static_assert(IRQ_MAX - IRQ_MIN >= 28,"IRQ range error");
	private:
		struct irq_handler{
			CALLBACK callback;
			void* data;
		};
		spin_lock lock;
		irq_handler table[IRQ_MAX - IRQ_MIN];

    public:
		APIC(void);
		APIC(const APIC&) = delete;
		byte id(void);
		void dispatch(byte off_id);

		CALLBACK get(byte irq) const;
		void set(byte irq,CALLBACK callback,void* data = nullptr);
    };
	extern APIC apic;
}
