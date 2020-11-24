#include "types.hpp"
#include "kdb.hpp"
#include "..\cpu\include\hal.hpp"
#include "..\cpu\include\pio.hpp"
#include "..\cpu\include\IF_guard.hpp"
#include "sysinfo.hpp"
#include "lock_guard.hpp"
#include "..\memory\include\pm.hpp"
#include "..\memory\include\vm.hpp"
#include "util.hpp"

using namespace UOS;

//static const char* hexchar = "0123456789ABCDEF";


bool UOS::kdb_enable = false;


#pragma warning(push)
#pragma warning(disable: 4365)
void UOS::kdb_init(word port) {
	io_write<byte>(port + 1, 0);
	io_write<byte>(port + 3, 0x80);
	io_write<byte>(port + 0, 2);
	io_write<byte>(port + 1, 0);
	io_write<byte>(port + 3, 0x03);
	io_write<byte>(port + 2, 0xC7);
	io_write<byte>(port + 4, 0x0B);	//?????

	//__writedr(7,0x700);
	kdb_enable = true;
	__debugbreak();
}
#pragma warning(pop)



size_t UOS::kdb_recv(word port, byte* dst, size_t lim) {

	while (true) {
		while (serial_get(port) != '$');

		byte chk_req = serial_get(port);
		byte chk_rel = '$';
		word len = serial_get(port);
		chk_rel += (byte)len;
		byte cur = serial_get(port);
		if (cur >= 4)	//shall less than 1KB
			continue;
		chk_rel += cur;
		len |= (word)cur << 8;

		for (word off = 0; off < len; off++) {
			cur = serial_get(port);
			if (off<lim)
				dst[off] = cur;
			chk_rel += cur;
		}
		if (chk_rel == chk_req)
			return min((size_t)len,lim);

	}
}


void UOS::kdb_send(word port, const byte* sor, size_t len) {

	byte checksum = '$';
	for (word off = 0; off < len; off++) {
		checksum += sor[off];
	}
	checksum += (byte)len;
	checksum += (byte)(len >> 8);

	serial_put(port, '$');
	serial_put(port, checksum);
	serial_put(port, (byte)len);
	serial_put(port, (byte)(len >> 8));
	while (len--) {
		serial_put(port, *sor++);
	}
}


void UOS::kdb_break(byte id,exception_context* context){

	DR_match();

	byte buf[0x400];
	size_t len=0;
	
	buf[len++]='B';
	buf[len++] = id;
	*(qword*)(buf+len)=context->rip;
	len+=8;
	*(qword*)(buf+len)=context->errcode;
	len+=8;
	kdb_send(sysinfo->ports[0],buf,len);

	len = kdb_recv(sysinfo->ports[0], buf, 0x400);	
	switch(buf[0]){
		case 'S':
			context->rflags |= 0x10100;		//TF RF
		case 'G':
			return;
		case 'Q':
			kdb_enable=false;
			buf[0] = 'C';
			len = 1;
			break;
		case 'B':	//breakpoint
		{
			IF_guard ig;
			DR_STATE dr;
			qword mask;
			DR_get(&dr);
			switch (buf[1]) {
			case '+':
				if (len < 10) {
					dbgprint("bad packet");
					break;
				}

				mask = 0x0C;
				for (unsigned i = 1; i <= 3; i++, mask <<= 2) {
					if (dr.dr7 & mask)
						continue;
					dr.dr[i] = *(qword*)(buf + 2);
					dr.dr7 |= mask;
					DR_set(&dr);
					break;
				}
				if (mask > 0xFF) {
					dbgprint("breakpoint slot full");
				}

				break;
			case '-':
				if (len < 3) {
					dbgprint("bad packet");
					break;
				}
				mask = (qword)0x03 << (2 * buf[2]);

				if (dr.dr7 & mask) {
					dr.dr7 &= ~mask;
					DR_set(&dr);
				}
				else
					dbgprint("no matching breakpoint");
				break;
			case '=':
				dr.dr0 = *(qword*)(buf + 2);
				dr.dr7 |= 0x03;
				DR_set(&dr);
				return;
			default:
				dbgprint("bad packet");
			}
			buf[0] = 'C';
			len = 1;
			break;
		}
		case 'I':	//info
			switch (buf[1]) {
			case 'R':
				memcpy(buf + 2, context, sizeof(exception_context));
				len = 2 + sizeof(exception_context);
				break;
			case 'D':	//dr
				DR_get(buf+2);
				len = 2 + sizeof(qword) * 6;
				break;
			default:
				dbgprint("bad packet");
				buf[0] = 'C';
				len = 1;
			}
			break;

		case 'T':	//stack
			buf[0] = 'C';
			len = 1;
			do{
				if (len < 2) {
					dbgprint("bad packet");
					break;
				}
				if (!VM::spy(buf + 2, context->rsp, buf[1] * (size_t)8)) {
					dbgprint("memory gap");
					buf[0] = 'C';
					break;
				}
				len = (size_t)2 + 8 * buf[1];
			}while(false);
			break;
		case 'V':	//vm
			buf[0] = 'C';
			len = 1;
			do{
				if (len < 11) {
					dbgprint("bad packet");
					break;
				}
				if ((len = *(word*)(buf + 9)) > 0x7C0) {
					dbgprint("too large");
					break;
				}
				if (!VM::spy(buf + 11, *(qword*)(buf + 1), len)) {
					dbgprint("memory gap");
					break;
				}
				buf[0] = 'M';
				len += 11;
			}while(false);
			break;
		case 'M':	//pm
			buf[0] = 'C';
			len = 1;
			do{
				if (len < 11) {
					dbgprint("bad packet");
					break;
				}
				if ((len = *(word*)(buf + 9)) > 0x7C0) {
					dbgprint("too large");
					break;
				}
				if (!PM::spy(buf + 11, *(qword*)(buf + 1), len)) {
					dbgprint("bad memory");
					break;
				}
				buf[0] = 'M';
				len += 11;
			}while(false);
			break;
		default:
			dbgprint("unknown command");
			buf[0] = 'C';
			len = 1;
	}

	kdb_send(sysinfo->ports[0], buf, len);

	
}

#pragma warning(push)
#pragma warning(disable: 4706)
void UOS::dbgprint(const char* str){
	if (!kdb_enable)
		return;
	byte buf[0x200] = { 'P' };
	size_t len = 1;
	
	//lock_guard<volatile MP> lck(*mp);
	//interrupt_guard ig;

	while (len < 0x200) {
		if (buf[len++] = (byte)*str++)
			;
		else
			break;
	}

	kdb_send(sysinfo->ports[0], buf, len);

}
#pragma warning(pop)
