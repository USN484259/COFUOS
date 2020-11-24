#pragma once
#include "types.hpp"
#include "mutex_base.hpp"

namespace UOS{

    class spin_lock : public UOS::mutex_base{
        volatile dword state;
    public:
        spin_lock(void);
        void lock(void) override;
        void unlock(void) override;
        bool try_lock(void) override;
        bool is_locked(void) const override;
    };

}