#pragma once
#include "types.hpp"

namespace UOS{

    class interrupt_guard{
        const bool state;
    public:
        interrupt_guard(void);
        ~interrupt_guard(void);
    };

    dword inline pci_address(byte bus,byte device,byte function,byte offset);

    dword pci_read(dword address);
    void pci_write(dword address,dword value);
}