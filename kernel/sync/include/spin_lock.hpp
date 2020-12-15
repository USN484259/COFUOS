#pragma once
#include "types.hpp"

namespace UOS{

    class spin_lock {
        volatile dword state;
    public:
        spin_lock(void);
        void lock(void);
        void unlock(void);
        bool try_lock(void);
        bool is_locked(void) const;
    };

}