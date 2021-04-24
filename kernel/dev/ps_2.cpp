#include "ps_2.hpp"
#include "apic.hpp"
#include "acpi.hpp"
#include "rtc.hpp"
#include "lock_guard.hpp"
#include "intrinsics.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"

using namespace UOS;

/* references :
https://wiki.osdev.org/%228042%22_PS/2_Controller
https://wiki.osdev.org/Keyboard
https://wiki.osdev.org/Mouse_Input
*/

inline void command(byte cmd){
	dword cnt = 0;
	while(true){
		byte stat = in_byte(0x64);
		if (0 == (stat & 2))
			break;
		if (++cnt > spin_timeout)
			bugcheck("spin timeout %x",cnt);
		mm_pause();
	}
	out_byte(0x64,cmd);
}

inline byte read(void){
	dword cnt = 0;
	while(true){
		byte stat = in_byte(0x64);
		if (stat & 1)
			break;
		if (++cnt > spin_timeout)
			bugcheck("spin timeout %x",cnt);
		mm_pause();
	}
	byte data = in_byte(0x60);
	return data;
}

inline void write(byte data){
	dword cnt = 0;
	while (true){
		byte stat = in_byte(0x64);
		if (0 == (stat & 2))
			break;
		if (++cnt > spin_timeout)
			bugcheck("spin timeout %x",cnt);
		mm_pause();
	}
	out_byte(0x60,data);
}

extern "C"
byte keycode[0x100];

inline byte translate(byte code,byte stat){
	byte base = (stat == 0xC0 || stat == 0xE0) ? 0x80 : 0;
	if (code & 0x80)
		base = 0;
	auto key = keycode[base + code];
	if (stat == 0xC0 || stat == 0xF0)
		key |= 0x80;
	return key;
}

PS_2::safe_queue::safe_queue(void) : buffer((byte*)operator new(QUEUE_SIZE)), head(0),tail(0) {}
PS_2::safe_queue::~safe_queue(void){
	operator delete(buffer,(size_t)QUEUE_SIZE);
}

byte PS_2::safe_queue::get(void){
	interrupt_guard<spin_lock> guard(objlock);
	if (head == tail)
		bugcheck("PS_2::safe_queue get empty queue @ %p",this);
	byte val = buffer[head];
	head = (head + 1) % QUEUE_SIZE;
	return val;
}

void PS_2::safe_queue::put(byte val){
	interrupt_guard<spin_lock> guard(objlock);
	auto new_tail = (tail + 1) % QUEUE_SIZE;
	if (new_tail != head){
		buffer[tail] = val;
		tail = new_tail;
	}
	else
		dbgprint("WARNING PS_2::safe_queue overflow");
	
	guard.drop();
	notify();
}

void PS_2::safe_queue::clear(void){
	interrupt_guard<spin_lock> guard(objlock);
	head = tail;
}

REASON PS_2::safe_queue::wait(qword us,wait_callback func){
	assert(func == nullptr);
	interrupt_guard<spin_lock> guard(objlock);
	if (head != tail){
		return PASSED;
	}
	guard.drop();
	return imp_wait(us);
}

PS_2::PS_2(void){
	if (acpi.get_version() && 0 == (acpi.get_fadt()->BootArchitectureFlags & 2)){
		// 8042 not present
		bugcheck("8042 not present");
	}
	bool dual_channel = false;
	bool channel_ok[2] = {0};

	interrupt_guard<void> ig;

	command(0xAD);
	command(0xA7);
	in_byte(0x60);	//flush buffer
	
	command(0x20);
	byte data = read();

	if (data & 0x20){
		dual_channel = true;
		data &= 0xBC;
	}
	else{
		data &= 0xBE;
	}
	command(0x60);
	write(data);

	command(0xAA);
	data = read();
	if (data != 0x55){
		bugcheck("invalid response %x",data);
	}
	command(0xAB);
	data = read();
	if (0 == data){
		channel_ok[0] = true;
	}
	if (dual_channel){
		command(0xA9);
		data = read();
		if (0 == data){
			channel_ok[1] = true;
		}
		else{
			dual_channel = false;
		}
	}
	command(0x20);
	data = read();
	data |= 1;
	data &= 0xAF;
	if (dual_channel){
		data |= 2;
		data &= 0xDF;
	}
	command(0x60);
	write(data);

	dbgprint("PS_2 %s channel : {%s,%s}",\
		dual_channel ? "dual" : "uni",\
		channel_ok[0] ? "OK" : "BAD",\
		channel_ok[1] ? "OK" : "BAD");
	
	apic.set(APIC::IRQ_KEYBOARD,on_irq,this);
	apic.set(APIC::IRQ_MOUSE,on_irq,this);

	process* ps = this_core().this_thread()->get_process();
	for (unsigned i = 0;i < 2;++i){
		if (channel_ok[i]){
			switch(i){
				case 0:
					command(0xAE);
					break;
				case 1:
					command(0xA8);
					command(0xD4);
					break;
			}
			write(0xFF);
			qword args[4] = {reinterpret_cast<qword>(this)};
			channel[i].th = ps->spawn(thread_ps2,args);
			assert(channel[i].th);
		}
	}
}

void PS_2::set_handler(callback cb,void* ud){
	userdata = ud;
	if (nullptr != cmpxchg_ptr(&func,cb,(callback)nullptr))
		bugcheck("invalid PS_2::set_handler call from %p",return_address());
}

bool PS_2::on_irq(byte irq,void* ptr){
	IF_assert;
	//assume all IRQs handled on the same core, not locked
	switch(irq){
		case APIC::IRQ_KEYBOARD:
		case APIC::IRQ_MOUSE:
			break;
		default:
			bugcheck("invalid IRQ#%d",irq);
	}
	byte data = read();
	//dbgprint("data %x from IRQ%d",data,irq);
	auto& channel = ((PS_2*)ptr)->channel[irq == APIC::IRQ_MOUSE ? 1 : 0];
	if (channel.th)
		channel.queue.put(data);
	return true;
}

static constexpr qword ps2_wait_timeout = 1000*40;	//40ms
static constexpr qword ps2_idle_timeout = 1000*1000;	//1s

#define DEV_CMD(cmd) \
	do{ \
		{ \
			interrupt_guard<spin_lock> guard(self.lock); \
			if (index) command(0xD4); \
			write(cmd); \
		} \
		if (TIMEOUT == queue.wait(ps2_wait_timeout)) \
			goto restart; \
		data = queue.get(); \
		if (data == 0xFA) \
			break; \
		if (data == 0xFE) \
			continue; \
		goto restart; \
	}while(true)

void PS_2::thread_ps2(qword ptr,qword,qword,qword){
	auto& self = *reinterpret_cast<PS_2*>(ptr);
	byte index;
	{
		this_core core;
		for (index = 0;index < 2;++index){
			if (self.channel[index].th == core.this_thread())
				break;
		}
		if (index >= 2)
			bugcheck("PS_2 invalid channel index %d",index);
	}
	safe_queue& queue = self.channel[index].queue;
	unsigned fail_count = 0;
	while(true){
		byte data;
		//wait forever for device hot-plug
		do{
			queue.wait();
			data = queue.get();
			if (data == 0xAA)
				break;
		}while(true);
		//device detected
		//mouse may send 00 after AA, discard that
		if (TIMEOUT != queue.wait(ps2_wait_timeout))
			queue.get();
		//disable scanning first
		DEV_CMD(0xF5);
		//then query identity
		DEV_CMD(0xF2);
		if (TIMEOUT == queue.wait(ps2_wait_timeout))
			goto restart;
		switch(queue.get()){
			case 0:
			{
				//magic sequence to enable scroll wheel
				DEV_CMD(0xF3);
				DEV_CMD(200);
				DEV_CMD(0xF3);
				DEV_CMD(100);
				DEV_CMD(0xF3);
				DEV_CMD(80);
				//query identity again
				DEV_CMD(0xF2);
				if (TIMEOUT == queue.wait(ps2_wait_timeout))
					goto restart;
				byte mode = queue.get();
				if (mode == 0 || mode == 3)
					;
				else
					goto restart;
				//mouse
				//set sample rate to 40
				DEV_CMD(0xF3);
				DEV_CMD(40);
				//enable scanning
				DEV_CMD(0xF4);
				dbgprint("mouse ready %d",mode);
				if (device_mouse(self,index,mode))
					fail_count = 0;
				continue;
			}
			case 0xAB:
				if (TIMEOUT == queue.wait(ps2_wait_timeout))
					goto restart;
				if (queue.get() != 0x83)
					goto restart;
				//keyboard
				//scancode set 2
				DEV_CMD(0xF0);
				DEV_CMD(2);
				//repeat 2Hz, delay 1000ms
				DEV_CMD(0xF3);
				DEV_CMD(0x7F);
				//enable scanning
				DEV_CMD(0xF4);
				dbgprint("keyboard ready");
				if (device_keybd(self,index))
					fail_count = 0;
				continue;
			//default:
			//	goto restart;
		}
	restart:
		queue.clear();
		if (++fail_count < 5){
			//reset device and start over
			if (index)
				command(0xD4);
			write(0xFF);
		}
		else{
			dbgprint("faulty PS2 device %d",index);
		}
	}
}

bool PS_2::device_keybd(PS_2& self,byte index){
	assert(index < 2);
	auto& queue = self.channel[index].queue;
	byte state = 0;
	bool probe = false;
	while(true){
		if (TIMEOUT == queue.wait(ps2_idle_timeout)){
			if (probe)	//device disconnected
				return true;
			probe = true;
			//send echo
			interrupt_guard<spin_lock> guard(self.lock);
			if (index)
				command(0xD4);
			write(0xEE);
			continue;
		}
		probe = false;
		byte data = queue.get();
		switch(data){
			case 0:
			case 0xAA:
			case 0xFC:
			case 0xFD:
			case 0xFE:
			case 0xFF:
				return false;
			case 0xEE:
			case 0xFA:
				continue;
		}
		switch(state){
			case 0:
				if (data == 0xE0 || data == 0xF0){
					state = data;
					continue;
				}
				break;
			case 0xE0:
				if (data == 0xF0){
					state = 0xC0;
					continue;
				}
				break;
			case 0xC0:
			case 0xF0:
				break;
			default:
				state = 0;
				continue;
		}
		auto keycode = translate(data,state);
		if ((keycode & 0x7F) && self.func)
			self.func(&keycode,sizeof(keycode),self.userdata);
		state = 0;
	}
}

bool PS_2::device_mouse(PS_2& self,byte index,byte mode){
	assert(index < 2);
	byte length = mode ? 4 : 3;
	bool probe = false;
	auto& queue = self.channel[index].queue;
	byte packet[4];
	while(true){
		byte it = 0;
		while(it < length){
			if (TIMEOUT == queue.wait(ps2_idle_timeout)){
				if (probe)	//device disconnected
					return true;
				probe = true;
				//request a packet
				interrupt_guard<spin_lock> guard(self.lock);
				if (index)
					command(0xD4);
				write(0xEB);
			}
			byte data = queue.get();
			if (probe && data == 0xFA){
				probe = false;
				continue;
			}
			if (it || (data & 0x08)){
				packet[it++] = data;
			}
		}
		//dbgprint("mouse %x,%x,%x,%x",(qword)packet[0],(qword)packet[1],(qword)packet[2],(qword)(mode ? packet[3] : 0));
		//TODO translate & dispatch mouse event
	}
}
