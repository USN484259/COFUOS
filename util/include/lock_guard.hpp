#pragma once

namespace UOS{
    template<typename M>
    class lock_guard{
        M& mutex;
    public:
        lock_guard(M& m) : mutex(m) {
            m.lock();
        }
        ~lock_guard(void) {
            mutex.unlock();
        }
    };

}