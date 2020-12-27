#include "types.hpp"
#include "kdb.hpp"
#include "cpu/include/hal.hpp"
#include "cpu/include/port_io.hpp"
#include "sysinfo.hpp"
#include "memory/include/pm.hpp"
#include "memory/include/vm.hpp"
#include "util.hpp"
#include "lang.hpp"

using namespace UOS;

static const char* hexchar = "0123456789ABCDEF";


bool UOS::kdb_enable = false;


#pragma warning(push)
#pragma warning(disable: 4365)
void UOS::kdb_init(word port) {
	port_write(port + 1, (byte)0);
	port_write(port + 3, (byte)0x80);
	port_write(port + 0, (byte)2);
	port_write(port + 1, (byte)0);
	port_write(port + 3, (byte)0x03);
	port_write(port + 2, (byte)0xC7);
	port_write(port + 4, (byte)0x0B);	//?????

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
	static byte buffer[PAGE_SIZE + 0x10];
	size_t send_len=0;
	
	buffer[send_len++]='B';
	buffer[send_len++] = id;
	*(qword*)(buffer+send_len)=context->rip;
	send_len+=8;
	*(qword*)(buffer+send_len)=context->errcode;
	send_len+=8;
	kdb_send(sysinfo->ports[0],buffer,send_len);

	while(kdb_enable) {
		auto recv_len = kdb_recv(sysinfo->ports[0], buffer, sizeof(buffer));
		auto command = buffer[0];
		buffer[0] = 'C';
		send_len = 1;
		switch(command){
			case 'S':
				context->rflags |= 0x10100;		//TF RF
			case 'G':
				return;
			case 'Q':
				kdb_enable = false;
				break;
			case 'B':	//breakpoint
			{
				DR_STATE dr;
				qword mask;
				DR_get(&dr);
				switch (buffer[1]) {
				case '+':
					if (recv_len < 10) {
						dbgprint("bad packet");
						break;
					}

					mask = 0x0C;
					for (unsigned i = 1; i <= 3; i++, mask <<= 2) {
						if (dr.dr7 & mask)
							continue;
						dr.dr[i] = *(qword*)(buffer + 2);
						dr.dr7 |= mask;
						DR_set(&dr);
						break;
					}
					if (mask > 0xFF) {
						dbgprint("breakpoint slot full");
					}

					break;
				case '-':
					if (recv_len < 3) {
						dbgprint("bad packet");
						break;
					}
					mask = (qword)0x03 << (2 * buffer[2]);

					if (dr.dr7 & mask) {
						dr.dr7 &= ~mask;
						DR_set(&dr);
					}
					else
						dbgprint("no matching breakpoint");
					break;
				case '=':
					dr.dr0 = *(qword*)(buffer + 2);
					dr.dr7 |= 0x03;
					DR_set(&dr);
					return;
				default:
					dbgprint("bad packet");
				}
				break;
			}
			case 'I':	//info
				switch (buffer[1]) {
				case 'R':
					memcpy(buffer + 2, context, sizeof(exception_context));
					buffer[0] = 'I';
					send_len = 2 + sizeof(exception_context);
					break;
				case 'D':	//dr
					DR_get(buffer+2);
					buffer[0] = 'I';
					send_len = 2 + sizeof(qword) * 6;
					break;
				default:
					dbgprint("bad packet");
				}
				break;

			case 'T':	//stack
				if (recv_len < 2) {
					dbgprint("bad packet");
					break;
				}
				{
					auto stk_size = (size_t)buffer[1] * 8;
					if (!vm.peek(buffer + 2, context->rsp, stk_size)) {
						dbgprint("memory gap");
						break;
					}
					buffer[0] = 'T';
					send_len = stk_size + 2;
					break;
				}
			case 'V':	//vm
			case 'M':	//pm
				if (recv_len < 11) {
					dbgprint("bad packet");
					break;
				}
				{
					size_t peek_size = *(word*)(buffer + 9);
					if (peek_size > PAGE_SIZE) {
						dbgprint("too large");
						break;
					}
					auto addr = *(qword*)(buffer + 1);
					auto result = false;
					if (command == 'V')
						result = vm.peek(buffer + 11, addr, peek_size);
					if (command == 'M')
						result = pm.peek(buffer + 11, addr, peek_size);
					if (!result) {
						dbgprint("memory gap");
						break;
					}
					buffer[0] = 'M';
					send_len = peek_size + 11;
					break;
				}
			default:
				dbgprint("unknown command");
		}
		kdb_send(sysinfo->ports[0], buffer, send_len);
	}
	
}

#pragma warning(push)
#pragma warning(disable: 4706)
void UOS::dbgprint(const char* fmt,...){
	if (!kdb_enable)
		return;
	static const size_t BUFFER_SIZE = 0x400;
	static byte buf[BUFFER_SIZE] = { 'P' };
	size_t len = 1;
	va_list args;
	va_start(args,fmt);


	while (len < BUFFER_SIZE) {
		auto c = *fmt++;

		if (c == '%'){
			c = *fmt++;
			switch(~0x20 & c){
				case 'P':	//print pointer (0 padding hex)
					if (BUFFER_SIZE - len >= 16) {
						len += 16;
						auto val = va_arg(args,qword);
						for (auto i = 1;i <= 16;++i){
							buf[len - i] = hexchar[val & 0x0F];
							val >>= 4;
						}
						continue;
					}
					c = '#';	//no enough space
					break;
				case 'X':	//print hex value
				{
					auto val = va_arg(args,qword);
					if (!val){
						c = '0';
						break;
					}
					auto width = 16;
					auto mask = (qword)0x0F << 0x3C;
					while (0 == (mask & val)) {
						mask >>= 4;
						--width;
					}
					if (BUFFER_SIZE - len >= width) {
						len += width;
						for (auto i = 1;i <= width;++i){
							buf[len - i] = hexchar[val & 0x0F];
							val >>= 4;
						}
						continue;
					}
					c = '#';	//no enough space
					break;
				}
				case 'D':	//print decimal dword value
				{
					auto val = va_arg(args,dword);
					if (!val){
						c = '0';
						break;
					}
					auto count = 0;
					while (val && len + count < BUFFER_SIZE) {
						buf[len + count] = '0' + val % 10;
						val /= 10;
						++count;
					}
					if (val){
						c = '#';	//no enough space
						break;
					}
					//reverse order
					auto head = buf + len;
					auto tail = head + count - 1;
					while(head < tail){
						swap(*head,*tail);
						++head,--tail;
					}
					len += count;
					continue;
				}
				case 'S':	//print string
				{
					auto str = va_arg(args,const char*);
					while (len < BUFFER_SIZE && *str){
						buf[len++] = *str++;
					}
					continue;
				}
				case 'C':	//character
					c = va_arg(args,char);
					break;
			}
		}
		if (!c)
			break;
		buf[len++] = c;
	}
	va_end(args);
	kdb_send(sysinfo->ports[0], buf, len);

}
#pragma warning(pop)
