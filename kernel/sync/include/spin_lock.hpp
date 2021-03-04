#pragma once
#include "types.hpp"

namespace UOS{

    class spin_lock {
        volatile dword state;
        static constexpr dword x_value = 0x80000000;
    public:
        enum MODE {EXCLUSIVE,SHARED};
        spin_lock(void);
        void lock(MODE = EXCLUSIVE);
        void unlock(void);
        bool try_lock(MODE = EXCLUSIVE);
        inline bool is_locked(void) const{
            return (state);
        }
        inline bool is_exclusive(void) const{
            return (state >= x_value);
        }
    };
    constexpr size_t spin_timeout = 0x04000000;
}