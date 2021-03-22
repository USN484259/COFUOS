#include "timer.hpp"
#include "constant.hpp"
#include "acpi.hpp"
#include "dev/include/apic.hpp"
#include "memory/include/vm.hpp"
#include "lock_guard.hpp"
#include "intrinsics.hpp"
#include "assert.hpp"
#include "vector.hpp"

using namespace UOS;

qword basic_timer::set_timer(unsigned index,byte sel,qword us){
	qword state = *(base + (0x100 + 0x20*index)/sizeof(qword));
	dbgprint("comparator #%d : %x",index,state);
	if ((state & 0x30) != 0x30)
		bugcheck("64-bit periodic mode not supported");
	state &= ~((1 << 14) | (1 << 8) | (1 << 6) | (1 << 1));	//64-bit, no-FSB, edge triggered
	state |= (1 << 3) | (1 << 2);	//periodic, enable
	state |= (sel & 0x1F) << 9;		//channel selection
	*(base + (0x100 + 0x20*index)/sizeof(qword)) = state;
	auto count = us*us2fs/tick_fs;
	*(base + (0x108 + 0x20*index)/sizeof(qword)) = count;	//tick count
	dbgprint("comparator #%d : (%x,%d)",index,state,count);
	return count;
}


basic_timer::basic_timer(void) : base(nullptr){
	auto hpet = acpi.get_hpet();
	if (hpet == nullptr){
		//fallback to 8254
		static constexpr dword frequency = 1193182;
		dword count = frequency*heartbeat_us/(1000*1000);	// frequency/(1000*1000/heartbeat_us)
		if (count >= 0x10000)
			bugcheck("invalid 8254 count %d",count);
		out_byte(0x43,0x34);
		out_byte(0x40,(byte)count);
		out_byte(0x40,(byte)(count >> 8));
		dbgprint("Timer using 8254 with count = %d",count);
	}
	else{
		//disable 8254
		out_byte(0x43,0x32);
		out_byte(0x40,0);
		out_byte(0x40,0);

		auto phy_page = align_down(hpet->address.Address,PAGE_SIZE);
		auto res = vm.assign(HPET_VBASE,phy_page,1);
		if (!res)
			bugcheck("vm.assign failed @ %p",phy_page);
		base = ((qword volatile*)(HPET_VBASE + hpet->address.Address - phy_page));
		qword capability = *base;
		tick_fs = capability >> 32;
		auto comarator_count = ((capability >> 8) & 0x1F) + 1;
		dbgprint("HPET %s-bit with %d counters, tick = %dns",\
			capability & (1 << 13) ? "64" : "32", comarator_count, tick_fs / (1000*1000));
		
		auto main_switch = *(base + 0x10/sizeof(qword));
		main_switch &= ~(qword)(0x03);	//disable legacy replacement
		*(base + 0x10/sizeof(qword)) = main_switch;	//halt the counter
		*(base + 0xF0/sizeof(qword)) = 0;	//reset counter

		//channel 0 as heat beat
		auto count = set_timer(0,2,heartbeat_us);

		for (unsigned i = 0;i < comarator_count;++i){
			qword state = *(base + (0x100 + i*0x20)/sizeof(qword));
			dbgprint("comparator #%d : %x",i,state);
		}

		*(base + 0x10/sizeof(qword)) = main_switch | 1;	//enable counter
		dbgprint("Timer using HPET with count = %d",count);
	}
	conductor = 0;
	running_us = 0;
	beat_counter = 0;
	apic.set(APIC::IRQ_PIT, irq_timer,this);
}

qword basic_timer::wait(qword us, CALLBACK func, void* arg, bool repeat){
	interrupt_guard<spin_lock> guard(lock);
	auto ticket = ++conductor;
	record.insert(ticket);

	auto total_tick = max<qword>(us/heartbeat_us,1);
	auto delta_tick = total_tick;
	auto it = delta_queue.begin();
	while(it != delta_queue.end()){
		if (delta_tick <= it->diff_tick){
			it->diff_tick -= delta_tick;
			delta_queue.insert(it,ticket,delta_tick,func,arg,repeat ? total_tick : 0);
			return ticket;
		}
		delta_tick -= it->diff_tick;
		++it;
	}
	delta_queue.push_back(ticket,delta_tick,func,arg,repeat ? total_tick : 0);
	return ticket;
}

bool basic_timer::cancel(qword ticket){
	interrupt_guard<spin_lock> guard(lock);
	auto it = record.find(ticket);
	if (it == record.end())
		return false;
	record.erase(it);
	return true;
}

void basic_timer::step(unsigned count){
	IF_assert;
	assert(count);
	lock_guard<spin_lock> guard(lock);
	decltype(delta_queue) periodic_list;
	while(!delta_queue.empty()){
		auto& cur = delta_queue.front();
		if (cur.diff_tick > count){
			cur.diff_tick -= count;
			break;
		}
		count -= cur.diff_tick;
		auto rec = record.find(cur.ticket);
		if (rec == record.end()){
			delta_queue.pop_front();
			continue;
		}
		//callback here
		cur.func(cur.ticket,cur.arg);

		if (!cur.interval){
			//one-shot
			delta_queue.pop_front();
			continue;
		}
		//periodic
		auto node = delta_queue.get_node(delta_queue.begin());
		cur.diff_tick = (cur.interval > count) ? (cur.interval - count) : 0;
		periodic_list.put_node(periodic_list.begin(),node);
	}
	while(!periodic_list.empty()){
		auto node = periodic_list.get_node(periodic_list.begin());
		auto& cur = node->payload;
		auto it = delta_queue.begin();
		while(true){
			if (it == delta_queue.end()){
				delta_queue.put_node(it,node);
				break;
			}
			if (cur.diff_tick <= it->diff_tick){
				it->diff_tick -= cur.diff_tick;
				delta_queue.put_node(it,node);
				break;
			}
			cur.diff_tick -= it->diff_tick;
			++it;
		}
	}
}

void basic_timer::on_second(void){
	auto val = beat_counter;
	auto adjust_tick = (heartbeat_hz > beat_counter) ? (heartbeat_hz - beat_counter) : 0;
	beat_counter = 0;
	dbgprint("beat = %d",val);
	running_us += heartbeat_us*adjust_tick;
	if (adjust_tick)
		step(adjust_tick);
}

void basic_timer::on_timer(void){
	IF_assert;
	++beat_counter;
	running_us += heartbeat_us;
	step(1);
}

void basic_timer::irq_timer(byte irq,void* ptr){
	IF_assert;
	assert(irq == APIC::IRQ_PIT);
	auto self = (basic_timer*)ptr;
	self->on_timer();
}