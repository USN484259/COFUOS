#include "types.hpp"
#include "kdb.hpp"
#include "cpu/include/hal.hpp"
#include "intrinsics.hpp"
#include "sysinfo.hpp"
#include "memory/include/pm.hpp"
#include "memory/include/vm.hpp"
#include "dev/include/acpi.hpp"
#include "util.hpp"
#include "lang.hpp"

using namespace UOS;

constexpr size_t kdb_stub::buffer_size;

/*erferences
	https://elixir.bootlin.com/linux/latest/source/arch/x86/include/asm/kgdb.h
	https://elixir.bootlin.com/linux/latest/source/kernel/debug/gdbstub.c
	https://docs.freebsd.org/info/gdb/gdb.info.Protocol.html
*/

static const char* hexchar = "0123456789ABCDEF";

static const char* err_invarg = "E16";

static const char* feature_list = "PacketSize=400";

inline byte hex_to_bin(byte ch){
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	ch &= ~0x20;
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 0x0A;
	return 0xFF;
}

inline bool serial_peek(word port){
	byte stat = in_byte(port + 5);
	return stat & 1;
}

inline byte serial_get(word port){
	while(!serial_peek(port))
		mm_pause();
	return in_byte(port);
}

inline void serial_put(word port,byte data){
	do{
		byte stat = in_byte(port + 5);
		if (stat & 0x20)
			break;
		mm_pause();
	}while(true);
	out_byte(port,data);
}

inline byte put_hex_char(word port,byte ch){
	serial_put(port,hexchar[ch >> 4]);
	byte chksum = hexchar[ch >> 4];
	serial_put(port,hexchar[ch & 0x0F]);
	chksum += hexchar[ch & 0x0F];
	return chksum;
}

inline byte put_escaped_char(word port,char ch){
	if (ch > 0x7D)
		ch = '?';
	byte chksum = 0;
	switch (ch){
		case 0x23:
		case 0x24:
		case 0x2A:
		case 0x7D:
			serial_put(port,0x7D);
			chksum = 0x7D;
			ch ^= 0x20;
			break;
	}
	serial_put(port,ch);
	chksum += ch;
	return chksum;
}

template<typename T>
inline size_t reg_encode(byte* dst,T val){
	memcpy(dst,&val,sizeof(T));
	return sizeof(T);
	/*
	for (unsigned i = 0;i < sizeof(T);++i){
		dst[2*i] = hexchar[(val >> 4) & 0x0F];
		dst[2*i + 1] = hexchar[val & 0x0F];
		val >>= 8;
	}
	return 2*sizeof(T);
	*/
}

inline size_t hex_to_num(const byte* sor,size_t lim,qword& val){
	val = 0;
	size_t i = 0;
	lim = min<size_t>(lim,2*sizeof(qword));
	while(i < lim){
		byte bi = hex_to_bin(sor[i]);
		if (bi > 0x0F)
			break;
		val = (val << 4) | bi;
		++i;
	}
	return i;
}

kdb_stub::kdb_stub(word pt) : port(pt){
	out_byte(port + 1, 0);
	out_byte(port + 3, 0x80);
	out_byte(port + 0, 3);
	out_byte(port + 1, 0);
	out_byte(port + 3, 0x03);
	out_byte(port + 2, 0xC7);

	//__writedr(7,0x700);

	features.set(decltype(features)::GDB);

	//handshake with GDB


	int_trap<3>();
}

void kdb_stub::recv(void) {
	while (true) {
		while (serial_get(port) != '$');

		byte chksum = 0;
		byte ch;
		size_t index = 0;

		do{
			ch = serial_get(port);
			if (ch == '#')
				break;
			if (index >= buffer_size)
				break;
			if (ch == 0x7D){
				ch = serial_get(port);
				buffer[index++] = ch ^ 0x20;
				chksum += 0x7D + ch;
			}
			else{
				buffer[index++] = ch;
				chksum += ch;
			}
		}while(true);

		if (ch == '#'){
			byte hi = hex_to_bin(serial_get(port));
			byte lo = hex_to_bin(serial_get(port));
			if (hi <= 0x0F && lo <= 0x0F && ((hi << 4) | lo) == chksum){
				serial_put(port,'+');
				length = index;
				return;
			}
		}
		serial_put(port,'-');
	}
}


void kdb_stub::send_bin(void) {

	do{
		byte chksum = 0;
		serial_put(port,'$');
		for (size_t index = 0;index < length;++index){
			chksum += put_hex_char(port,buffer[index]);
		}
		serial_put(port,'#');
		put_hex_char(port,chksum);

		byte ch = serial_get(port);
		switch(ch){
			case '$':
				serial_put(port,'-');
			case '+':
				return;
		}
	}while(true);
}

void kdb_stub::send(const char* str){
	do{
		byte chksum = 0;
		serial_put(port,'$');
		auto ptr = str;
		while(*ptr){
			chksum += put_escaped_char(port,*ptr++);
		}
		serial_put(port,'#');
		put_hex_char(port,chksum);

		byte ch = serial_get(port);
		switch(ch){
			case '$':
				serial_put(port,'-');
			case '+':
				return;
		}

	}while(true);
}

void kdb_stub::on_break(void){
	static const char* breakname[] = { "DE","DB","NMI","BP","OF","BR","UD","NM","DF","??","TS","NP","SS","GP","PF","??","MF","AC","MC","XM" };
	print("%s : %d @ %p",
		vector == 0xFF ? "BugCheck" : (vector < 20 ? breakname[vector] : "??"),
		(dword)vector,conx->rip);
	switch(vector){
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
			print("errcode = 0x%x", errcode);
	}
	cmd_status();
}

void kdb_stub::cmd_status(void){
	char str[4] = { "S0\0" };

	switch(vector){
		case 1:
		case 3:
			str[2] = '5';
			break;
		default:
			str[2] = '7';
	}
	send(str);
}

bool kdb_stub::cmd_query(void){
	if (length >= 11 && \
		11 == match((const char*)buffer,"qSupported:",11)){
		send(feature_list);
		return true;
	}
	return false;
}

void kdb_stub::cmd_read_regs(void){
	if (length != 1){
		send(err_invarg);
		return;
	}
	length = 0;
	length += reg_encode(buffer + length,conx->rax);
	length += reg_encode(buffer + length,conx->rbx);
	length += reg_encode(buffer + length,conx->rcx);
	length += reg_encode(buffer + length,conx->rdx);
	length += reg_encode(buffer + length,conx->rsi);
	length += reg_encode(buffer + length,conx->rdi);
	length += reg_encode(buffer + length,conx->rbp);
	length += reg_encode(buffer + length,conx->rsp);
	length += reg_encode(buffer + length,conx->r8);
	length += reg_encode(buffer + length,conx->r9);
	length += reg_encode(buffer + length,conx->r10);
	length += reg_encode(buffer + length,conx->r11);
	length += reg_encode(buffer + length,conx->r12);
	length += reg_encode(buffer + length,conx->r13);
	length += reg_encode(buffer + length,conx->r14);
	length += reg_encode(buffer + length,conx->r15);
	length += reg_encode(buffer + length,conx->rip);
	length += reg_encode(buffer + length,(dword)conx->rflags);
	length += reg_encode(buffer + length,(dword)conx->cs);
	length += reg_encode(buffer + length,(dword)conx->ss);
	send_bin();
}

void kdb_stub::cmd_write_regs(void){
// TODO
	send("E01");
}

void kdb_stub::cmd_read_vm(void){
	do{
		if (!features.get(decltype(features)::MEM))
			break;
		if (length < 2)
			break;
		qword va;
		auto index = hex_to_num(buffer + 1,length - 1,va);
		if (0 == index)
			break;
		index += 1;
		if (index + 1 >= length || buffer[index] != ',')
			break;
		qword count;
		index = hex_to_num(buffer + index + 1,length - index - 1,count);
		if (index == 0)
			break;
		
		length = vm.peek(buffer,va,min((size_t)count,buffer_size));
		if (!length)
			break;
		send_bin();
		return;
	}while(false);
	send(err_invarg);
}

void kdb_stub::cmd_write_vm(void){
// TODO
	send("E01");
}

struct dr_state{
	union {
		struct {
			qword dr0;
			qword dr1;
			qword dr2;
			qword dr3;
		};
		qword dr[4];
	};
	//qword dr6;
	qword dr7;

	inline void load(void){
		__asm__ volatile (
			"mov %0,dr0\n\
			 mov %1,dr1\n\
			 mov %2,dr2\n\
			 mov %3,dr3\n\
			 mov %4,dr7"
			: "=r" (dr0), "=r" (dr1), "=r" (dr2), "=r" (dr3), "=r" (dr7)
		);
	}
	
	inline void store(bool only_7 = false){
		if (only_7 == false){
			__asm__ volatile (
				"mov dr0,%0\n\
				 mov dr1,%1\n\
				 mov dr2,%2\n\
				 mov dr3,%3"
				:
				: "r" (dr0), "r" (dr1), "r" (dr2), "r" (dr3)
			);
		}
		__asm__ volatile (
			"mov dr7,%0"
			:
			: "r" (dr7)
		);
	}
};

void kdb_stub::cmd_breakpoint(void){
	do{
		if (length < 3 || buffer[2] != ',')
			break;
		if (buffer[1] != '1'){
			send("");
			return;
		}
		qword addr;
		auto index = hex_to_num(buffer + 3,length - 3,addr);
		if (0 == index)
			break;
		index += 3;
		if (index + 1 >= length || buffer[index] != ',')
			break;

		dr_state dr;
		qword mask = 0x03;
		dr.load();
		byte blank = 0xFF;
		byte match = 0xFF;

		for (unsigned i = 0;i < 4;++i,mask <<= 2){
			if (dr.dr7 & mask){
				if (dr.dr[i] == addr)
					match = i;
			}
			else
				blank = i;
		}
		bool res = false;
		switch(buffer[0]){
			case 'Z':	//set
				if (match >= 4 && blank < 4){
					mask = 0x03 << (2*blank);
					dr.dr[blank] = addr;
					dr.dr7 |= mask;
					dr.store();
					res = true;
				}
				break;
			case 'z':	//clear
				if (match < 4){
					mask = 0x03 << (2*match);
					dr.dr[match] = 0;
					dr.dr7 &= ~mask;
					dr.store(true);
					res = true;
				}
				break;
		}

		if (res)
			send("OK");
		else
			send("E1C");
		return;
	}while(false);
	send(err_invarg);
}

bool kdb_stub::cmd_run(void){
	while(length){
		if (length > 1){
			qword addr;
			if (0 == hex_to_num(buffer + 1,length - 1,addr))
				break;
			conx->rip = addr;
		}
		if (buffer[0] == 's')	//step
			conx->rflags |= 0x10100;	//TF RF
		return true;
	}
	send(err_invarg);
	return false;
}



void kdb_stub::signal(byte id,qword error_code,context* i_context){
	vector = id;
	errcode = error_code;
	conx = i_context;

	// https://elixir.bootlin.com/linux/latest/source/kernel/debug/gdbstub.c#L957

	on_break();

	while(true){
		recv();
		if (length == 0)
			continue;
		switch(buffer[0]){
			case '?':
				cmd_status();
				continue;
			case 'g':
				cmd_read_regs();
				continue;
			case 'G':
				cmd_write_regs();
				continue;
			case 'm':
				cmd_read_vm();
				continue;
			case 'M':
				cmd_write_vm();
				continue;
			case 'Z':
			case 'z':
				cmd_breakpoint();
				continue;
			case 'c':
			case 's':
				if (cmd_run())
					return;
				break;
			case 'k':
				shutdown();
				continue;
			case 'q':
				if (cmd_query())
					continue;
				break;
		}
		send("");
	}
}

void kdb_stub::print(const char* fmt,...){
	if (!features.get(decltype(features)::GDB))
		return;
	
	va_list args;
	auto port = sysinfo->ports[0];
	do{
		serial_put(port,'$');
		serial_put(port,'O');
		byte chksum = 'O';

		char buffer[20];
		auto ptr = fmt;
		va_start(args,fmt);

		while(*ptr){
			char ch = *ptr++;
			if (ch == '%'){
				ch = *ptr++;
				switch (~0x20 & ch){
				case 'P':	//pointer, qword, 0 padding hex
				{
					auto val = va_arg(args,qword);
					for (unsigned i = 1;i <= 16;++i){
						buffer[16 - i] = hexchar[val & 0x0F];
						val >>= 4;
					}
					for (unsigned i = 0;i < 16;++i){
						chksum += put_hex_char(port,buffer[i]);
					}
					continue;
				}
				case 'X':	//qword, 0 stripped hex
				{
					auto val = va_arg(args,qword);
					if (val == 0){
						ch = '0';
						break;
					}
					unsigned index = 0;
					while(val){
						buffer[index++] = hexchar[val & 0x0F];
						val >>= 4;
					}
					while(index--){
						chksum += put_hex_char(port,buffer[index]);
					}
					continue;	
				}
				case 'D':	//dword decimal
				{
					auto val = va_arg(args,dword);
					if (val == 0){
						ch = 0;
						break;
					}
					unsigned index = 0;
					while(val){
						buffer[index++] = '0' + val % 10;
						val /= 10;
					}
					while(index--){
						chksum += put_hex_char(port,buffer[index]);
					}
					continue;
				}
				case 'S':	//string
				{
					auto str = va_arg(args,const char*);
					while(*str)
						chksum += put_hex_char(port,*str++);
					continue;
				}
				case 'C':	//character
					ch = va_arg(args,int);
					break;

				}
			}
			chksum += put_hex_char(port,ch);
		}
		va_end(args);
		serial_put(port,'#');
		put_hex_char(port,chksum);

		switch(serial_get(port)){
			case '$':
				serial_put(port,'-');
			case '+':
				return;
		}
	}while(true);
}
