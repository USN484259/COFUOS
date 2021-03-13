#include "rtc.hpp"
#include "intrinsics.hpp"
#include "assert.hpp"
#include "exception/include/kdb.hpp"
#include "dev/include/apic.hpp"
#include "acpi.hpp"
#include "sync/include/lock_guard.hpp"
#include "timer.hpp"

using namespace UOS;

RTC::RTC(void){
	interrupt_guard<void> guard;
	if (acpi.get_version() && acpi.get_fadt()->BootArchitectureFlags & 0x20){
		//RTC not present
		bugcheck("RTC not present");
	}
	apic.set(APIC::IRQ_RTC,on_irq,this);
	out_byte(0x70,0x8A);  //RTC Status Register A & disable NMI
	byte val = in_byte(0x71);
	val &= 0xF0;	//disable divider output
	out_byte(0x70,0x8A);
	out_byte(0x71,val);

	out_byte(0x70,0x8B);  //RTC Status Register B
	mode = in_byte(0x71);
	mode = (mode & 0x1E) | 0x10;    //disable alarm & periodic, enable update_end
	out_byte(0x70,0x8B);
	out_byte(0x71,mode);
	out_byte(0x70,0x0C);  //enable NMI
	in_byte(0x71);	//throw away RTC Status Register C
	dbgprint("RTC mode = 0x%x",(qword)mode);
	century_index = acpi.get_fadt()->Century;
}

void RTC::on_irq(byte id,void* data){
	IF_assert;
	assert(id == APIC::IRQ_RTC);
	((RTC*)data)->update();
}

void RTC::convert(byte& val){
	if (mode & 0x04)    //binary
		return;
	//BCD
	val = (val >> 4)*10 + (val & 0x0F);
}

void RTC::update(void){
	out_byte(0x70,0x0C);
	byte reason = in_byte(0x71);
	if (reason & 0x10){ 	//update ended interrupt
		// out_byte(0x70,0x0A);
		// byte stat = in_byte(0x71);
		// if (stat & 0x80)
		// 	dbgprint("RTC warning: non-stable state");
	
		out_byte(0x70,0); //seconds
		byte second = in_byte(0x71);
		convert(second);
		if (reset || (++time & 0x03) != (second & 0x03)){    //not sync, reload

			out_byte(0x70,2); //minute
			byte minute = in_byte(0x71);
			convert(minute);

			out_byte(0x70,4); //hour
			byte hour = in_byte(0x71);
			if (mode & 0x02){   //24h
				;
			}
			else{   //12h
				if (hour & 0x80){   //p.m.
					hour = (hour & 0x7F) + 0x12;    //TRICK: convert could handle 'overflowed' BCD
					if (hour == 0x24)   //special case
						hour = 0;
				}
			}
			convert(hour);

			out_byte(0x70,7); //day of month
			byte date = in_byte(0x71);
			convert(date);

			out_byte(0x70,8); //month
			byte month = in_byte(0x71);
			convert(month);

			out_byte(0x70,9); //year
			byte year = in_byte(0x71);
			convert(year);

			byte century = 20;
			if (century_index){
				out_byte(0x70,century_index);
				century = in_byte(0x71);
				convert(century);
			}
			// out_byte(0x70,0x0A);
			// stat = in_byte(0x71);
			// if (stat & 0x80)
			// 	dbgprint("RTC warning: non-stable state");

			//validate
			dbgprint("RTC date & time: %d%d.%d.%d %d:%d:%d",century,year,month,date,hour,minute,second);
			if (century < 20 || year > 99 || month == 0 || month > 12 \
				|| date == 0 || date > 31 || hour > 23 || minute > 59 || second > 59)
			{
				dbgprint("RTC warning: invalid date & time");
				return;
			}

			constexpr dword month_table[12] = {
				0,   31,  59,  90,
				120, 151, 181, 212,
				243, 273, 304, 334
			};

			//calculate POSIX time
			dword cur_year = century * 100 + year;
			dword leap_count = (cur_year - 1969)/4;
			//year 2100 non-leap, not implemented
			dword days_of_year = month_table[month - 1] + date - 1;
			if (0 == cur_year%4 && month > 2){
				++leap_count;
			}
			auto total_days = (cur_year - 1970)*365 + leap_count + days_of_year;
			time = (qword)total_days*86400 + hour*3600 + minute*60 + second;
			reset = false;
			dbgprint("RTC update: POSIX time = %d",time);
		}
		timer.on_second();
	}
}