#include "types.hpp"
#include "hal.hpp"
#include "sysinfo.hpp"
#include "context.hpp"
#include "lock.hpp"
#include "mm.hpp"
#include "mp.hpp"


#undef assert
#undef assertinv
#undef assertless


using namespace UOS;

static const char* hexchar = "0123456789ABCDEF";


bool kdb_enable = false;


void kdb_init(word port) {
	__outbyte(port + 1, 0);
	__outbyte(port + 3, 0x80);
	__outbyte(port + 0, 2);
	__outbyte(port + 1, 0);
	__outbyte(port + 3, 0x03);
	__outbyte(port + 2, 0xC7);
	__outbyte(port + 4, 0x0B);	//?????

	kdb_enable = true;
	__debugbreak();
}


size_t kdb_recv(word port, byte* dst, size_t lim) {

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
			dst[off] = cur;
			chk_rel += cur;
		}
		if (chk_rel == chk_req)
			return len;

	}
}


void kdb_send(word port, const byte* sor, size_t len) {

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




void kdb_break(byte id,qword errcode,CONTEXT* context){

	DR_match();

	byte buf[0x400];
	size_t len=0;
	
	buf[len++]='B';
	buf[len++] = id;
	
	kdb_send(sysinfo->ports[0],buf,len);

	bool quit = false;

	while(!quit){
		len = kdb_recv(sysinfo->ports[0], buf, 0x400-1);
		if (!len)
			continue;

		buf[len] = 0;
		switch (buf[0]) {
		case 'W':	//where
			buf[0] = 'A';
			*(qword*)(buf + 1) = context->rip;
			len = 9;
			break;
		case 'S':	//step
			context->rflags |= 0x10100;		//TF RF

		case 'G':	//go
			quit = true;
		case 'P':	//pause
			buf[0] = 'C';
			len = 1;
			break;
		case 'Q':	//debugger quit
			kdb_enable=false;

			quit = true;
			buf[0] = 'C';
			len = 1;
			break;
		case 'B':	//breakpoint
			switch (buf[1]) {
			case '+':
			{
				if (len < 10) {
					dbgprint("bad packet");
					continue;
				}
				interrupt_guard ig;
				DR_STATE dr;
				DR_get(&dr);

				qword mask = 0x0C;
				unsigned i;
				for (i = 1; i <= 3; i++, mask <<= 2) {
					if (dr.dr7 & mask)
						continue;
					dr.dr[i] = *(qword*)(buf + 2);
					dr.dr7 |= mask;
					DR_set(&dr);
					break;
				}
				if (i > 3) {
					dbgprint("breakpoint slot full");
				}

				break;
			}
			case '-':
			{
				if (len < 3) {
					dbgprint("bad packet");
					break;
				}
				interrupt_guard ig;
				DR_STATE dr;
				qword mask = 0x03 << (2*buf[2]);
				DR_get(&dr);

				if (dr.dr7 & mask) {
					dr.dr7 &= ~mask;
					DR_set(&dr);
				}
				else
					dbgprint("no matching breakpoint");

				break;
			}
			default:
				dbgprint("bad packet");
				continue;
			}
			buf[0] = 'I';
			buf[1] = 'D';
		case 'I':	//info
			switch (buf[1]) {
			case 'R':
				memcpy(buf + 2, context, sizeof(CONTEXT));
				len = 2 + sizeof(CONTEXT);
				break;
			case 'D':	//dr
				DR_get(buf+2);
				len = 2 + sizeof(qword) * 6;
				break;
			default:
				dbgprint("bad packet");
				continue;
			}
			break;

		case 'T':	//stack
			if (len < 10) {
				dbgprint("bad packet");
				continue;
			}
			if (!VM::spy(buf + 2, context->rsp, buf[1] * 8)) {
				dbgprint("memory gap");
				continue;
			}
			else {
				len = 2 + 8 * buf[1];
			}

			break;
		case 'V':	//vm
			if (len < 11) {
				dbgprint("bad packet");
				continue;
			}
			if ((len = *(word*)(buf + 9)) > 0x7C0) {
				dbgprint("too large");
				continue;
			}
			if (!VM::spy(buf + 11, *(qword*)(buf + 1), len)) {
				dbgprint("memory gap");
				continue;
			}
			else {
				buf[0] = 'M';
				len += 11;
			}
			break;
		case 'M':	//pm
			if (len < 11) {
				dbgprint("bad packet");
				continue;
			}
			if ((len = *(word*)(buf + 9)) > 0x7C0) {
				dbgprint("too large");
				continue;
			}
			if (!PM::spy(buf + 11, *(qword*)(buf + 1), len)) {
				dbgprint("bad memory");
				continue;
			}
			else {
				buf[0] = 'M';
				len += 11;
			}
			break;
		default:
			dbgprint("unknown command");
			continue;
		}

		kdb_send(sysinfo->ports[0], buf, len);

	}
	
}




extern "C"
void dispatch_exception(byte id,qword errcode,CONTEXT* context){

//special errcode indicates BugCheck call
	lock_guard<MP> lck(*mp);
	mp->sync(MP::CMD::pause);

	//clear TF
	context->rflags &= ~0x100;
	if (kdb_enable)
		kdb_break(id,errcode,context);

	mp->sync(MP::CMD::resume);
	//TODO handle exceptions here

}


void dbgprint(const char* str){
	byte buf[0x200] = { 'P' };
	size_t len = 1;
	
	lock_guard<MP> lck(*mp);
	interrupt_guard ig;

	while (len < 0x200) {
		if (buf[len++] = *str++)
			;
		else
			break;
	}

	kdb_send(sysinfo->ports[0], buf, len);

}