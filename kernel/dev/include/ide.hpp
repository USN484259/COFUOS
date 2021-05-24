#pragma once
#include "types.h"
#include "constant.hpp"
#include "pci.hpp"
#include "sync/include/semaphore.hpp"
#include "sync/include/event.hpp"

namespace UOS{
	class IDE{
		word port_base[4];
		word dma_base = 0;
		byte irq_vector[2];

		const PCI::device_info* pci_ref;

		semaphore sync;
		event ev;

		static bool on_irq(byte irq,void* ptr);

	public:
		enum MODE {READ,WRITE};

		IDE(void);
		void reset(void);
		bool command(MODE mode,qword lba,dword pa,word size);
	};
	extern IDE ide;
}