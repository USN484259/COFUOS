#include "uos.h"
#include "util.hpp"
#include "stdarg.h"

using namespace UOS;

static const char* hexchar = "0123456789ABCDEF";

static void* image_base;
static const char* environment;

typedef void (*procedure)(void);

extern "C"{
	procedure __CTOR_LIST__;
	procedure __DTOR_LIST__;
	int main(int argc,char** argv);
}

extern "C"
int isspace(int ch){
	switch(ch){
		case 0x9:
		case 0xA:
		case 0xB:
		case 0xC:
		case 0xD:
		case 0x20:
			return 1;
	}
	return 0;
}

extern "C"
size_t strlen(const char* str){
	size_t count = 0;
	while(*str++)
		++count;
	return count;
}

inline byte hex2bin(char ch){
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	ch &= ~0x20;
	if (ch >='A' && ch <= 'F')
		return ch - 'A';
	return 0xFF;
}

extern "C"
unsigned long strtoul(const char* str,char** end,int base){
	auto result = strtoull(str,end,base);
	constexpr auto limit = (unsigned long long)1 << (8*sizeof(unsigned long));
	return (result >= limit) ? limit : result;
}

extern "C"
unsigned long long strtoull(const char* str,char** end,int base){
	if (str == nullptr)
		return 0;
	unsigned long long val = 0;
	while (*str && isspace(*str))
		++str;
	while(*str){
		if (base == 0){
			if (*str =='0')
				base = ((~0x20 & *(str+1)) == 'X') ? 0x10 : 8;
			else
				base = 10;
		}
		byte digit = hex2bin(*str);
		if (digit >= base)
			break;
		val = val*base + digit;
		++str;
	}
	if (end)
		*end = (char*)str;
	return val;
}

byte parse_size(const char*& format,char& ch){
	byte operand_size = sizeof(int);
	switch(ch & (~0x20)){
		case 'H':
			operand_size = sizeof(short);
			break;
		case 'L':
			operand_size = sizeof(long);
			break;
		default:
			return operand_size;
	}
	char next = *format++;
	switch(next & (~0x20)){
		case 'H':
			operand_size = sizeof(char);
			break;
		case 'L':
			operand_size = sizeof(long long);
			break;
		case 'O':
		case 'D':
		case 'I':
		case 'X':
		case 'U':
			ch = next;
			return operand_size;
		default:
			return 0;
	}
	if ((~0x20) & (ch ^ next))
		return 0;
	ch = *format++;
	switch(ch & (~0x20)){
		case 'O':
		case 'D':
		case 'I':
		case 'X':
		case 'U':
			return operand_size;
		default:
			return 0;
	}
}

template<typename F>
bool imp_printf(const char* format,va_list args,F func){
	if (format == nullptr)
		return false;
	while(true){
		auto ch = *format++;
		if (ch == 0)
			break;
		if (ch == '%'){
			ch = *format++;
			if (ch == 0)
				break;
			switch(ch & (~0x20)){
				case 'C':	//single character
					ch = va_arg(args,int);
					break;
				case 'S':	//string
				{
					auto str = va_arg(args,const char*);
					if (str == nullptr)
						str = "(null)";
					while(*str){
						if (!func(*str++))
							return true;
					}
					continue;
				}
				case 'P':	//pointer
				{
					qword ptr = (qword)va_arg(args,void*);
					unsigned shift = 8*sizeof(qword);
					do{
						shift -= 4;
						byte val = (ptr >> shift) & 0x0F;
						if (!func(hexchar[val]))
							return true;
					}while(shift);
					continue;
				}
				case 'H':
				case 'L':
				case 'D':
				case 'I':
				case 'O':
				case 'X':
				case 'U':
				{
					byte oparand_size = parse_size(format,ch);
					bool is_signed = ((ch & (~0x20)) == 'D' || (ch & (~0x20)) == 'I');
					if (oparand_size == 0)
						return false;
					union {
						long long s;
						unsigned long long u;	
					};
					switch(oparand_size){
						case 1:
						case 2:
							if (is_signed)
								s = va_arg(args,int);
							else
								u = va_arg(args,unsigned int);
							break;
						case 4:
							if (is_signed)
								s = va_arg(args,int32_t);
							else
								u = va_arg(args,uint32_t);
							break;
						case 8:
							if (is_signed)
								s = va_arg(args,int64_t);
							else
								u = va_arg(args,uint64_t);
							break;
						default:
							return false;
					}
					qword val;
					if (is_signed){
						if (s < 0){
							if (!func('-'))
								return true;
							val = (-s);
						}
						else
							val = s;
					}
					else{
						val = u;
					}
					if (val == 0){
						ch = '0';
						break;
					}
					byte base = 10;
					if ((ch & (~0x20)) == 'X')
						base = 16;
					if ((ch & (~0x20)) == 'O')
						base = 8;
					char buffer[0x20];
					byte index = 0;
					while(val){
						buffer[index] = hexchar[val % base];
						if (ch == 'x' && (buffer[index] & 0x40))
							buffer[index] |= 0x20;
						++index;
						val /= base;
					}
					do{
						if (!func(buffer[--index]))
							return true;
					}while(index);
					continue;
				}
				default:
					return false;
			}
		}
		if(!func(ch))
			return true;
	}
	return true;
}


extern "C"
int snprintf(char* buffer,size_t limit,const char* format,...){
	if (buffer == nullptr || limit == 0){
		//should have returned desired buffer size, currently not implemented
		return -1;
	}
	va_list args;
	va_start(args,format);
	unsigned count = 0;
	auto res = imp_printf(format,args,[=,&count](char ch) -> bool{
		if (count + 1 >= limit)
			return false;
		buffer[count++] = ch;
		return true;
	});
	va_end(args);
	if (count < limit)
		buffer[count] = 0;
	return res ? count : (-1);
}

extern "C"
void __main(void){
	//call global constructors
	procedure* head = &__CTOR_LIST__;
	auto tail = head;
	do{
		++tail;
	}while(*tail);
	--tail;
	while(head != tail){
		(*tail--)();
	}
}

extern "C"
[[ noreturn ]]
void exit(int result){
	//call global destructors
	procedure* dtor = &__DTOR_LIST__;
	for(++dtor;*dtor;++dtor)
		(*dtor)();
	exit_process(result);
}

char** parse_commandline(char* const cmd,dword length,unsigned& argc){
	argc = 1;
	auto ptr = cmd;
	for (unsigned i = 0;i < length;++i,++ptr){
		if (*ptr == ' '){
			++argc;
			*ptr = 0;
		}
	}
	char** argv = (char**)operator new(sizeof(char*)*argc);
	if (argv == nullptr)
		return nullptr;
	argv[0] = cmd;
	ptr = cmd;
	unsigned pos = 1;
	for (unsigned i = 0;i < length;++i,++ptr){
		if (*ptr == 0){
			if (pos >= argc)
				break;
			argv[pos++] = ptr + 1;
		}
	}
	return (pos == argc) ? argv : nullptr;
}

extern "C"
[[ noreturn ]]
void uos_entry(void* entry,void* imgbase,void* env,void* stk_top){
	image_base = imgbase;
	environment = (const char*)env;

	int return_value = (-1);
	do{
		//prepare arguments
		HANDLE ps = get_process();
		dword commandline_size = 0;
		if (get_command(ps,nullptr,&commandline_size) != TOO_SMALL)
			break;
		char* commandline = (char*)operator new(commandline_size);
		if (commandline == nullptr)
			break;
		if (get_command(ps,commandline,&commandline_size) != SUCCESS)
			break;
		close_handle(ps);
		unsigned argc;
		char** argv = parse_commandline(commandline,commandline_size,argc);
		if (argv == nullptr)
			break;
		return_value = main(argc,argv);
	}while(false);
	exit(return_value);
}

void* operator new(size_t size){
	//TODO replace with REAL heap
	size = align_up(size,0x1000)/0x1000;
	auto ptr = vm_reserve(nullptr,size);
	if (ptr){
		vm_commit(ptr,size);
	}
	return ptr;
}

void operator delete(void* ptr,size_t size){
	//TODO replace with REAL heap
	size = align_up(size,0x1000)/0x1000;
	vm_release(ptr,size);
}