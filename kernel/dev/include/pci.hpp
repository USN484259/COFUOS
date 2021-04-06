#pragma once
#include "types.h"
#include "assert.hpp"
#include "vector.hpp"

namespace UOS{
	constexpr word pci_function(byte bus,byte dev,byte fun){
		return ((word)bus << 8) | (byte)(dev << 3) | (fun & 7);
	}
	class PCI{
	public:
		struct device_info{
			word func;
			word interrupt;
			dword vender;
			dword classify;
		};
	private:
		vector<device_info> devices;

		void check_bus(byte bus);
		void check_function(word func);
	public:
		PCI(void);
		static dword read(word func,byte off);
		static void write(word func,byte off,dword val);
		const device_info* find(byte class_id,byte subclass,const device_info* prev = nullptr) const;
	};
	extern PCI pci;
}