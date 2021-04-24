#pragma once
#include "types.h"
#include "constant.hpp"
#include "pci.hpp"
#include "sync/include/semaphore.hpp"
#include "sync/include/event.hpp"

namespace UOS{
	class IDE{
		enum MODE : byte {NONE = 0,READ = 0x20,WRITE = 0x40};

		word port_base[4];
		word dma_base = 0;
		byte irq_vector[2];

		const PCI::device_info* pci_ref;
		// struct {
		// 	dword addr;
		// 	word size;
		// 	MODE mode;
		// 	byte status;
		// } request = {0};

		semaphore sync;
		event ev;

		static bool on_irq(byte irq,void* ptr);

	public:
		IDE(void);
		void reset(void);
		bool read(qword lba,dword pa,word size);
		bool write(qword lba,dword pa,word size);
	};
	extern IDE ide;
}