#include "ps_2.hpp"
#include "cpu/include/apic.hpp"
#include "cpu/include/port_io.hpp"
#include "acpi.hpp"
#include "exception/include/kdb.hpp"

using namespace UOS;

PS_2::PS_2(void){	//see https://wiki.osdev.org/%228042%22_PS/2_Controller
	if (acpi.get_version() && 0 == (acpi.get_fadt().BootArchitectureFlags & 2)){
		// 8042 not present
		BugCheck(hardware_fault,this);
	}
	bool dual_channel = false;
	byte data;
	command(0xAD);
	command(0xA7);
	port_read(0x60,data);	//flush buffer
	
	command(0x20);
	data = read();

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
		BugCheck(hardware_fault,data);
	}
	command(0xAB);
	data = read();
	if (0 == data){
		channels[0].state = NONE;
	}
	if (dual_channel){
		command(0xA9);
		data = read();
		if (0 == data){
			channels[1].state = NONE;
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
		channels[0].state == NONE ? "OK" : "BAD",\
		channels[1].state == NONE ? "OK" : "BAD");
	
	interrupt_guard<void> guard;
	apic.set(APIC::IRQ_KEYBOARD,on_irq,this);
	apic.set(APIC::IRQ_MOUSE,on_irq,this);

	command(0xAE);
	write(0xFF);
	if (dual_channel){
		command(0xA8);
		command(0xD4);
		write(0xFF);
	}
}

void PS_2::command(byte cmd){
	dword cnt = 0;
	while(true){
		byte stat;
		port_read(0x64,stat);
		if (0 == (stat & 2))
			break;
		if (++cnt > spin_timeout)
			BugCheck(deadlock,cnt);
		_mm_pause();
	}
	port_write(0x64,cmd);
}

byte PS_2::read(void){
	dword cnt = 0;
	while(true){
		byte stat;
		port_read(0x64,stat);
		if (stat & 1)
			break;
		if (++cnt > spin_timeout)
			BugCheck(deadlock,cnt);
		_mm_pause();
	}
	byte data;
	port_read(0x60,data);
	return data;
}

void PS_2::write(byte data){
	dword cnt = 0;
	while (true){
		byte stat;
		port_read(0x64,stat);
		if (0 == (stat & 2))
			break;
		if (++cnt > spin_timeout)
			BugCheck(deadlock,cnt);
		_mm_pause();
	}
	port_write(0x60,data);
}

void PS_2::on_irq(byte irq,void* ptr){
	IF_assert;
	auto& self = *(PS_2*)ptr;
	byte data = self.read();
	if (irq == APIC::IRQ_KEYBOARD || irq == APIC::IRQ_MOUSE)
		;
	else{
		BugCheck(hardware_fault,irq);
	}
	auto& channel = self.channels[irq == APIC::IRQ_MOUSE ? 1 : 0];
	//TODO more robust state machine
	switch (channel.state){
	case NONE:
		if (data == 0xAA){
			channel.state = INIT;
		}
		break;
	case ID_ACK:
		if (data == 0xFA){
			channel.state = ID;
			break;
		}
		if (--channel.data[0] == 0)
			channel.state = NONE;
		break;
	case ID:
		switch (data){
		case 0:
			if (channel.data[0]){
				channel.state = MODE_M;
				channel.data[0] = 200;
				break;
			}
		case 3:
		case 4:
			channel.state = MOUSE_I;
			channel.queue.clear();
			channel.data[0] = data;
			break;
		case 0xAB:
			channel.state = ID_K;
			break;
		case 0xFA:
			break;
		default:
			channel.state = NONE;
		}
		break;
	case ID_K:
		if (data == 0x83){
			channel.state = KEYBD_I;
			channel.queue.clear();
			channel.data[0] = 0;
		}
		else
			channel.state = NONE;
		break;
	case ID_M:
		if (data == 0xFA){
			switch (channel.data[0]){
			case 200:
				channel.state = MODE_M;
				channel.data[0] = 100;
				break;
			case 100:
				channel.state = MODE_M;
				channel.data[0] = 80;
				break;
			case 80:
				channel.state = MODE_M;
				channel.data[0] = 0;
				break;
			case 0:
				channel.state = ID;
				break;
			default:
				channel.state = NONE;
			}
		}
		else{
			channel.state = NONE;
		}
		break;
	case MOUSE:
	case KEYBD:
		channel.queue.put(data);
		break;
	}
}

void PS_2::set(PS_2::CALLBACK cb,void* data){
	callback = cb;
	userdata = data;
}

void PS_2::step(size_t cur_time){
	for (auto i = 0;i < 2;++i){
		auto& channel = channels[i];
		switch (channel.state){
		case INIT:
			channel.state = ID_ACK;
			channel.data[0] = 4;
			if (i)
				command(0xD4);
			write(0xF5);
			if (i)
				command(0xD4);
			write(0xF2);
			break;
		case MODE_M:
			switch (channel.data[0])
			{
			case 200:
			case 100:
			case 80:
				if (i)
					command(0xD4);
				write(0xF3);
				if (i)
					command(0xD4);
				write(channel.data[0]);
				channel.state = ID_M;
				break;
			case 0:
				if (i)
					command(0xD4);
				write(0xF2);
				channel.state = ID_M;
				break;
			default:
				channel.state = NONE;
			}
			break;
		case KEYBD_I:
			if (i)
				command(0xD4);
			write(0xF4);
			dbgprint("PS_2 Keyboard ready");
			channel.state = KEYBD;
			break;
		case MOUSE_I:
			if (i)
				command(0xD4);
			write(0xF3);
			if (i)
				command(0xD4);
			write(40);
			if (i)
				command(0xD4);
			write(0xF4);
			dbgprint("PS_2 Mouse %d ready",channel.data[0]);
			channel.state = MOUSE;
			break;
		case KEYBD:
			step_keybd(channel);
			break;
		case MOUSE:
			step_mouse(channel);
			break;
		}
		if (channel.state == KEYBD || channel.state == MOUSE){
			if (timestamp != cur_time){
				if (i)
					command(0xD4);
				write(0xEE);
			}
		}
	}
	timestamp = cur_time;
}

void PS_2::step_keybd(PS_2::CHANNEL& channel){
	if (channel.queue.empty())
		return;
	byte& stat = channel.data[0];
	do{
		byte data;
		if (!channel.queue.get(data))
			return;
		if (data == 0xEE)
			continue;
		switch(stat){
		case 0:
			if (data == 0xE0 || data == 0xF0){
				stat = data;
				continue;
			}
			break;
		case 0xE0:
			if (data == 0xF0){
				stat = 0xC0;
				continue;
			}
			break;
		case 0xC0:
		case 0xF0:
			break;
		default:
			stat = 0;
			continue;
		}
		auto keycode = translate(data,stat);
		if (keycode && callback)
			callback(keycode,(stat == 0xC0 || stat == 0xF0),userdata);
		stat = 0;
	}while(true);
}

void PS_2::step_mouse(PS_2::CHANNEL& channel){
	//TODO
}



byte PS_2::translate(byte code,byte stat){
	auto ext = (stat == 0xC0 || stat == 0xE0);
	if (code == 0x83 && !ext)
		return code;	//treat F7 as 'ext'
	if (code == 0x12 && ext)
		return 0;	//simplify PrtScr, so PrtScr -> (0x7C | 0x80)
	if (code < 0x80)	//ext -> bit7 set
		return code | (ext ? 0x80 : 0);
	return 0;
}