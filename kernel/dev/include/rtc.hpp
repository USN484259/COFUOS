#pragma once
#include "types.hpp"

namespace UOS{
    class RTC{
    public:
        typedef void (*CALLBACK)(qword,word,void*);
    private:
        qword time = 0;
        word ms_count = 0;
        byte mode;
        bool reset = true;

        CALLBACK handler = nullptr;
        void* handler_data = nullptr;

        static void on_irq(byte id,void* data);
        void convert(byte&);
        void update(void);
    public:
        RTC(void);
        qword get_time(void) const;
        word get_ms(void) const;
        void set_handler(CALLBACK,void* = nullptr);
        void reload(void);
    };
    //extern RTC rtc;
}