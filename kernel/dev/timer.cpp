#include "timer.hpp"
#include "constant.hpp"
#include "acpi.hpp"
#include "dev/include/apic.hpp"
#include "memory/include/vm.hpp"
#include "sync/include/lock_guard.hpp"
#include "intrinsics.hpp"
#include "assert.hpp"
#include "vector.hpp"

using namespace UOS;


basic_timer::basic_timer(void) : base(nullptr){
	dbgprint("Timer using HPET");
	auto& hpet = acpi.get_hpet();
	auto phy_page = align_down(hpet.address.Address,PAGE_SIZE);
	auto res = vm.assign(HPET_VBASE,phy_page,1);
	if (!res)
		bugcheck("vm.assign failed @ %p",phy_page);
	base = ((qword volatile*)(HPET_VBASE + hpet.address.Address - phy_page));
	qword capability = *base;
	tick_fs = capability >> 32;
	auto comarator_count = ((capability >> 8) & 0x1F) + 1;
	dbgprint("HPET %s-bit with %d counters, tick = %dns",\
		capability & (1 << 13) ? "64" : "32", comarator_count, tick_fs / (1000*1000));
	
	if (!hpet.legacy_replacement)
		bugcheck("legacy replacement not supported");
	
	auto main_switch = *(base + 0x10/sizeof(qword));
	main_switch &= ~(qword)(0x01);
	*(base + 0x10/sizeof(qword)) = main_switch;	//halt the counter
	*(base + 0xF0/sizeof(qword)) = 0;	//reset counter

	qword state = *(base + 0x100/sizeof(qword));
	dbgprint("comparator #0 : %x",state);
	if ((state & 0x30) != 0x30)
		bugcheck("64-bit periodic mode not supported");
	state &= ~((1 << 14) | (1 << 8) | (1 << 6) | (1 << 1));	//64-bit, no-FSB, edge triggered
	state |= (1 << 3) | (1 << 2);	//periodic, enable
	*(base + 0x100/sizeof(qword)) = state;
	//*(base + 0x108/sizeof(qword)) = 0;	//reset comparator
	auto count = heartbeat_rate_us*us2fs/tick_fs;
	*(base + 0x108/sizeof(qword)) = count;	//tick count
	//apic.allocate(APIC::IRQ_PIT,false);
	channel_index = 0;
	apic.set(APIC::IRQ_PIT, irq_timer,this);


#if false
	channel_index = 0xFF;
	for (unsigned i = 0;i < comarator_count;++i){
		qword state = *(base + (0x100 + i*0x20)/sizeof(qword));
		dbgprint("comparator #%d : %x",i,state);
		if (channel_index >= comarator_count && (state & 0x30) == 0x30){	//64-bit & periodic
			//use this channel, allocate IRQ slot for it
			for (unsigned irq = 20;irq < 24;++irq){
				if (state & (0x100000000 << irq) && apic.available(irq)){
					//use this IRQ line
					state &= ~((1 << 14) | (1 << 8) | (1 << 1));	//64-bit, no-FSB, edge triggered
					state |= (1 << 6) | (1 << 3) | (1 << 2);	//periodic, enable
					state |= (irq << 9);	//set IRQ line
					*(base + (0x100 + i*0x20)/sizeof(qword)) = state;	//write back state
					*(base + (0x100 + i*0x20 + 8)/sizeof(qword)) = 0;	//reset value
					auto count = heartbeat_rate_us*(1000*1000*1000)/tick_fs;
					*(base + (0x100 + i*0x20 + 8)/sizeof(qword)) = count;	//tick count
					dbgprint("IRQ line %d, periodic %d ticks",irq,count);
					state = *(base + (0x100 + i*0x20)/sizeof(qword));	//read back state
					res = apic.allocate(irq,false);
					if (!res || ((state >> 9) & 0x1F) != irq)		//check IRQ line
						bugcheck(hardware_fault,irq);
					channel_index = i;
					apic.set(APIC::IRQ_OFFSET + irq, irq_timer,this);

					break;
				}
			}
			if (channel_index < comarator_count)
				continue;
		}
		state &= ~(1 << 2);		//disable interrupt
		*(base + (0x100 + i*0x20)/sizeof(qword)) = state;
	}
	if (channel_index >= comarator_count)
		bugcheck(not_implemented,this);
#endif
	for (unsigned i = 0;i < comarator_count;++i){
		qword state = *(base + (0x100 + i*0x20)/sizeof(qword));
		dbgprint("comparator #%d : %x",i,state);
	}

	conductor = 0;

	*(base + 0x10/sizeof(qword)) = main_switch | 3;	//enable counter in legacy-replacement mode
}

qword basic_timer::wait(qword us, CALLBACK func, void* arg, bool repeat){
	interrupt_guard<spin_lock> guard(lock);
	auto ticket = ++conductor;
	record.insert(ticket);

	auto total_tick = max<qword>(us/heartbeat_rate_us,1);
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

void basic_timer::on_timer(void){
	IF_assert;

	struct callback_context {
		CALLBACK func;
		qword ticket;
		void* arg;
		callback_context(CALLBACK f,qword t,void* a) : func(f), ticket(t), arg(a) {}
	};
	vector<callback_context> call_chain;

	{
		lock_guard<spin_lock> guard(lock);
		if (delta_queue.empty())
			return;
		{
			auto& cur = delta_queue.front();
			if (cur.diff_tick){
				--cur.diff_tick;
				return;
			}
		}
		do{
			auto& cur = delta_queue.front();
			if (cur.diff_tick)
				break;
			auto rec = record.find(cur.ticket);
			do{
				if (rec != record.end()){
					call_chain.push_back(cur.func,cur.ticket,cur.arg);
					if (cur.interval){	//periodic
						//move this node back
						auto node = delta_queue.get_node(delta_queue.begin());
						auto it = delta_queue.begin();
						cur.diff_tick = cur.interval;
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
						break;
					}
					else{	//one-shot
						record.erase(rec);
					}
				}
				delta_queue.pop_front();
			}while(false);

		}while(!delta_queue.empty());
	}
	for (auto& it : call_chain){
		it.func(it.ticket,it.arg);
	}
}

void basic_timer::irq_timer(byte,void* ptr){
	auto self = (basic_timer*)ptr;
	//auto stat = *(self->base + (0x20/sizeof(qword)));
	//auto mask = (qword)1 << self->channel_index;
	
	//check IRQ source
	//if (0 == (stat & mask))
	//	bugcheck(hardware_fault,stat);

	self->on_timer();

	//allow further interrupts
	//*(self->base + (0x20/sizeof(qword))) = mask;
}