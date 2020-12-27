#include "rtc.hpp"
#include "cpu/include/port_io.hpp"
#include "assert.hpp"
#include "exception/include/kdb.hpp"
#include "cpu/include/apic.hpp"
#include "acpi.hpp"
#include "sync/include/lock_guard.hpp"

using namespace UOS;

RTC::RTC(void){
	interrupt_guard<void> guard;
	if (acpi.get_version() && acpi.get_fadt().BootArchitectureFlags & 0x20){
		//RTC not present
		BugCheck(hardware_fault,this);
	}
	apic.set(APIC::IRQ_RTC,on_irq,this);
	port_write(0x70,(byte)0x8A);  //RTC Status Register A & disable NMI
	byte val;
	port_read(0x71,val);
	val = (val & 0xF0) | 6; //set divier to 1024Hz
	port_write(0x70,(byte)0x8A);
	port_write(0x71,val);

	port_write(0x70,(byte)0x8B);  //RTC Status Register B
	port_read(0x71,mode);
	mode = (mode & 0x7F) | 0x50;    //periodic & update_end
	port_write(0x70,(byte)0x8B);
	port_write(0x71,mode);
	port_write(0x70,(byte)0x0C);  //enable NMI & throw away RTC Status Register C
	port_read(0x71,val);
	dbgprint("RTC mode = 0x%x",(qword)mode);
}

void RTC::on_irq(byte id,void* data){
	IF_assert;
	if (id == APIC::IRQ_RTC){
		//TODO broadcast to APs
		((RTC*)data)->update();
	}
}

qword RTC::get_time(void) const{
	return time;
}

word RTC::get_ms(void) const{
	return ms_count;
}

void RTC::set_handler(CALLBACK callback,void* data){
	handler = callback;
	handler_data = data;
}

void RTC::reload(void){
	reset = true;
}

void RTC::convert(byte& val){
	if (mode & 0x04)    //binary
		return;
	//BCD
	val = (val >> 4)*10 + (val & 0x0F);
}

void RTC::update(void){
	byte reason = 0;
	port_write(0x70,(byte)0x0C);
	port_read(0x71,reason);
	if (reason & 0x40){ //periodic interrupt
		++ms_count;
	}
	if (reason & 0x10){ //update ended interrupt
		port_write(0x70,(byte)0x0A);
		byte stat;
		port_read(0x71,stat);
		if (stat & 0x80)
			dbgprint("RTC warning: non-stable state");

		port_write(0x70,(byte)0); //seconds
		byte second;
		port_read(0x71,second);
		convert(second);
		if (!reset && (++time & 0x03) == (second & 0x03)){    //time sync with CMOS
			auto cnt = ms_count;
			ms_count = 0;
			dbgprint("RTC step @ %dms: POSIX time = %d",cnt,time);
			if (cnt < 1030){  //ms_count shall <= 1024
				goto finish;
			}
			dbgprint("RTC warning: ms_count overflow");
		}
		port_write(0x70,(byte)2); //minute
		byte minute;
		port_read(0x71,minute);
		convert(minute);

		port_write(0x70,(byte)4); //hour
		byte hour;
		port_read(0x71,hour);
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

		port_write(0x70,(byte)7); //day of month
		byte date;
		port_read(0x71,date);
		convert(date);

		port_write(0x70,(byte)8); //month
		byte month;
		port_read(0x71,month);
		convert(month);

		port_write(0x70,(byte)9); //year
		byte year;
		port_read(0x71,year);
		convert(year);

		byte century = 20;
		auto century_index = acpi.get_fadt().Century;
		if (century_index){
			port_write(0x70,century_index);
			port_read(0x71,century);
			convert(century);
		}
		port_write(0x70,(byte)0x0A);
		port_read(0x71,stat);
		if (stat & 0x80)
			dbgprint("RTC warning: non-stable state");

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
		ms_count = 0;
		reset = false;
		dbgprint("RTC update: POSIX time = %d",time);
	}
finish:
	if (handler)
		handler(time,ms_count,handler_data);
}