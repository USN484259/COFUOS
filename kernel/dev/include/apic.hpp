#pragma once

#include "types.h"
#include "constant.hpp"
#include "sync/include/spin_lock.hpp"

extern "C" void dispatch_irq(byte);

namespace UOS{
    class APIC{
	public:
		typedef bool (*CALLBACK)(byte, void*);
		static constexpr size_t IRQ_MIN = 0x20;		//min handled IRQ
		static constexpr size_t IRQ_MAX = IDT_LIM/0x10;		//max handled IRQ
		static constexpr byte IRQ_OFFSET = IRQ_MIN + 4;	//offset for external IRQ
		static constexpr byte IRQ_IPI = IRQ_MIN;
		static constexpr byte IRQ_CONTEXT_TRAP = IRQ_MIN + 1;
		static constexpr byte IRQ_APIC_TIMER = IRQ_MIN + 2;
		static constexpr byte IRQ_SCI = IRQ_MIN + 3;
		static constexpr byte IRQ_PIT = IRQ_OFFSET;
		static constexpr byte IRQ_KEYBOARD = IRQ_OFFSET + 1;
		static constexpr byte IRQ_RTC = IRQ_OFFSET + 8;
		static constexpr byte IRQ_MOUSE = IRQ_OFFSET + 0x0C;
		static constexpr byte IRQ_IDE_PRI = IRQ_OFFSET + 0x0E;
		static constexpr byte IRQ_IDE_SEC = IRQ_OFFSET + 0x0F;

		static_assert(IRQ_MAX - IRQ_MIN >= 28,"IRQ range error");
	private:
		struct irq_handler{
			CALLBACK callback;
			void* data;
		};
		spin_lock lock;
		word io_apic_entries;
		irq_handler table[IRQ_MAX - IRQ_MIN];

		void dispatch(byte off_id);
		friend void ::dispatch_irq(byte);
    public:
		APIC(void);
		APIC(const APIC&) = delete;
		byte id(void);


		//bool available(byte irq_index);
		//bool allocate(byte irq_index,bool level);

		CALLBACK get(byte irq) const;
		void set(byte irq,CALLBACK callback,void* data = nullptr);
    };
	extern APIC apic;
}
