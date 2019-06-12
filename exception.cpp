#include "types.hpp"
#include "hal.hpp"
#include "sysinfo.hpp"
#include "context.hpp"
#include "mm.hpp"


#undef assert
#undef assertinv
#undef assertless


using namespace UOS;

static const char* hexchar = "0123456789ABCDEF";

void kdb_init(word port) {
	__outbyte(port + 1, 0);
	__outbyte(port + 3, 0x80);
	__outbyte(port + 0, 2);
	__outbyte(port + 1, 0);
	__outbyte(port + 3, 0x03);
	__outbyte(port + 2, 0xC7);
	__outbyte(port + 4, 0x0B);	//?????


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
	byte buf[0x800];
	size_t len=0;
	
	
	
	buf[len++]='B';
	buf[len++] = id;
	
	kdb_send(sysinfo->ports[0],buf,len);

	bool quit = false;

	while(!quit){
		len = kdb_recv(sysinfo->ports[0], buf, 0x800);
		
		switch (buf[0]) {
		case 'W':	//where
			buf[0] = 'A';
			*(qword*)(buf + 1) = context->rip;
			len = 9;
			break;
		case 'S':	//step
			//TODO set DR0
		case 'G':	//go
			quit = true;
		case 'P':	//pause
			buf[0] = 'C';
			len = 1;
			break;
		case 'Q':	//debugger quit
			//TODO shut stub here

			quit = true;
			buf[0] = 'C';
			len = 1;
			break;
		case 'I':	//info
			switch (buf[1]) {
			case 'R':
				__movsq((qword*)(buf + 2), (const qword*)context, sizeof(CONTEXT) / 8);
				len = 2 + sizeof(CONTEXT);
				break;
			default:
				//bad packet
				continue;
			}
			break;
		case 'B':	//breakpoint
			//TODO edit bp in DR

			continue;
		case 'T':	//stack
			if (!VM::spy(buf + 2, context->rsp, buf[1] * 8)) {
				buf[0] = 'A';
				__movsb(buf, (const byte*)"Pmemory gap", 12);
				len = 12;
			}
			else {
				len = 2 + 8 * buf[1];
			}

			break;
		case 'V':	//vm
			if ((len = *(word*)(buf + 9)) > 0x7C0) {
				__movsb(buf, (const byte*)"Ptoo large", 11);
				len = 11;
				break;
			}
			if (!VM::spy(buf + 11, *(qword*)(buf + 1), len)) {
				__movsb(buf, (const byte*)"Pmemory gap", 12);
				len = 12;
			}
			else {
				buf[0] = 'M';
				len += 11;
			}
			break;
		case 'M':	//pm
			if ((len = *(word*)(buf + 9)) > 0x7C0) {
				__movsb(buf, (const byte*)"Ptoo large", 11);
				len = 11;
				break;
			}
			if (!PM::spy(buf + 11, *(qword*)(buf + 1), len)) {
				__movsb(buf, (const byte*)"Pbad memory", 12);
				len = 12;
			}
			else {
				buf[0] = 'M';
				len += 11;
			}
			break;
		default:
			__movsb(buf, (const byte*)"Punknown command", 17);
			len = 17;
		}

		kdb_send(sysinfo->ports[0], buf, len);

	}
	
}




extern "C"
void dispatch_exception(byte id,qword errcode,CONTEXT* context){

//special errcode indicates BugCheck call

	kdb_break(id,errcode,context);


	//TODO handle exceptions here

}



