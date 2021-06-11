#include "uos.h"
#include "util.hpp"
#include "stdarg.h"
#include "crt_heap.hpp"

using namespace UOS;

static const char* hexchar = "0123456789ABCDEF";

static void* image_base;
static const char* environment;
static qword guard_value;

FILE std_obj[3] = { \
	{STDIN,0,0,0},
	{STDOUT,0,0,0},
	{STDERR,0,0,0},
};

typedef void (*procedure)(void);

extern "C"
procedure __CTOR_LIST__;

extern "C"
procedure __DTOR_LIST__;

extern "C"
int main(int argc,char** argv);

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
int isupper(int ch){
	return (ch >= 'A' && ch <= 'Z');
}
int islower(int ch){
	return (ch >= 'a' && ch <= 'z');
}
extern "C"
int isalpha(int ch){
	ch &= (~0x20);
	return (ch >= 'A' && ch <= 'Z');
}
extern "C"
int isdigit(int ch){
	return (ch >= '0' && ch <= '9');
}
extern "C"
int isxdigit(int ch){
	if (isdigit(ch))
		return 1;
	ch &= (~0x20);
	return (ch >= 'A' && ch <= 'F');
}
extern "C"
int isalnum(int ch){
	return (isalpha(ch) || isdigit(ch));
}
extern "C"
int ispunct(int ch){
	switch(ch){
		case '!':
		case '\"':
		case '#':
		case '$':
		case '%':
		case '&':
		case '\'':
		case '(':
		case ')':
		case '*':
		case '+':
		case ',':
		case '-':
		case '.':
		case '/':
		case ':':
		case ';':
		case '<':
		case '=':
		case '>':
		case '?':
		case '@':
		case '[':
		case '\\':
		case ']':
		case '^':
		case '_':
		case '`':
		case '{':
		case '|':
		case '}':
		case '~':
			return 1;
	}
	return 0;
}
extern "C"
int isprint(int ch){
	return (isalnum(ch) || ispunct(ch) || ch == ' ');
}

extern "C"
size_t strlen(const char* str){
	size_t count = 0;
	while(*str++)
		++count;
	return count;
}

extern "C"
const char* strstr(const char* str,const char* substr){
	auto pos = str;
	auto cmp = substr;
	while(true){
		if (*cmp == 0)
			return pos;
		if (*str == 0)
			break;
		if (*cmp != *str){
			cmp = substr;
			pos = ++str;
			continue;
		}
		++str;
		++cmp;
	}
	return nullptr;
}

extern "C"
int strcmp(const char* a,const char* b){
	while(*a && *b){
		if (*a != *b)
			break;
		++a,++b;
	}
	return (byte)*a - (byte)*b;
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
unsigned long strtoul(const char* str,const char** end,int base){
	auto result = strtoull(str,end,base);
	constexpr auto limit = (unsigned long)(-1);
	return (result > limit) ? limit : result;
}

extern "C"
unsigned long long strtoull(const char* str,const char** end,int base){
	if (str == nullptr)
		return 0;
	unsigned long long val = 0;
	while (*str && isspace(*str))
		++str;
	
	if (base == 0){
		if (*str =='0')
			base = ((~0x20 & *(str+1)) == 'X') ? 0x10 : 8;
		else
			base = 10;
	}
	if (base == 0x10 && *str == '0' && (~0x20 & *(str+1)) == 'X')
		str += 2;
	
	while(*str){
		byte digit = hex2bin(*str);
		if (digit >= base)
			break;
		val = val*base + digit;
		++str;
	}
	if (end)
		*end = (const char*)str;
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

bool block_write(FILE* stream,const char* ptr,dword length){
	assert(stream);
	do{
		dword size = length;
		if (SUCCESS != stream_write(stream->file,ptr,&size)){
			stream->flags |= FILE::FAIL_BIT;
			return false;
		}
		if (size == 0){
			switch(wait_for(stream->file,0,0)){
				case PASSED:
				case NOTIFY:
					break;
				default:
					stream->flags |= FILE::FAIL_BIT;
					return false;
			}
			auto stat = stream_state(stream->file,&size);
			if (stat > 0xFF){
				stream->flags |= FILE::FAIL_BIT;
				return false;
			}

			if (stat & (OP_FAILURE | MEM_FAILURE)){
				stream->flags |= FILE::FAIL_BIT;
				return false;
			}
			if (stat & (MEDIA_FAILURE | FS_FAILURE)){
				stream->flags |= FILE::BAD_BIT;
				return false;
			}
		}
		length -= size;
		if (length == 0)
			break;
		ptr += size;
	}while(true);
	return true;
}

extern "C"
int fprintf(FILE* stream,const char* format,...){
	if (stream == nullptr || format == nullptr)
		return -1;
	va_list args;
	va_start(args,format);
	unsigned count = 0;

	constexpr dword limit = 0x100;
	char buffer[limit];
	dword index = 0;

	auto res = imp_printf(format,args,[&,stream,limit](char ch) -> bool{
		buffer[index++] = ch;
		if (index < limit)
			return true;
		count += index;
		index = 0;
		return block_write(stream,buffer,limit);
	});
	va_end(args);
	if (index){
		res = res && block_write(stream,buffer,index);
	}
	return res ? (count + index) : (-1);
}

extern "C"
int puts(const char* str){
	if (fputs(str,stdout) < 0)
		return EOF;
	return fputc('\n',stdout);
}

extern "C"
int fputs(const char* str,FILE* stream){
	if (stream == nullptr || str == nullptr)
		return EOF;
	dword len = strlen(str);
	return block_write(stream,str,len) ? len : EOF;
}

extern "C"
int fputc(int ch,FILE* stream){
	if (stream == nullptr)
		return EOF;
	return block_write(stream,(const char*)&ch,1) ? ch : EOF;
}

bool block_read(FILE* stream,char* ptr,dword length){
	assert(stream);
	do{
		dword sz = (stream->tail + sizeof(stream->buffer) - stream->head) % sizeof(stream->buffer);
		sz = min(sz,length);
		if (stream->head + sz <= sizeof(stream->buffer)){
			memcpy(ptr,stream->buffer + stream->head,sz);
		}
		else{
			auto slice = sizeof(stream->buffer) - stream->head;
			memcpy(ptr,stream->buffer + stream->head,slice);
			memcpy(ptr + slice,stream->buffer,sz - slice);
		}
		stream->head = (stream->head + sz) % sizeof(stream->buffer);
		length -= sz;
		if (length == 0)
			break;
		ptr += sz;
		//refill stream ptr
		sz = max<dword>(0x40,length);
		sz = min<dword>(sz,0x100);
		char buffer[sz];
		if (SUCCESS != stream_read(stream->file,buffer,&sz)){
			stream->flags |= FILE::FAIL_BIT;
			return false;
		}
		if (sz == 0){
			switch(wait_for(stream->file,0,0)){
				case PASSED:
				case NOTIFY:
					break;
				default:
					stream->flags |= FILE::FAIL_BIT;
					return false;
			}
			auto stat = stream_state(stream->file,&sz);
			if (stat > 0xFF){
				stream->flags |= FILE::FAIL_BIT;
				return false;
			}
			if (stat & EOF_FAILURE)
				stream->flags |= FILE::EOF_BIT;
			if (stat & (OP_FAILURE | MEM_FAILURE)){
				stream->flags |= FILE::FAIL_BIT;
				return false;
			}
			if (stat & (MEDIA_FAILURE | FS_FAILURE)){
				stream->flags |= FILE::BAD_BIT;
				return false;
			}
		}
		if (stream->tail + sz <= sizeof(stream->buffer)){
			memcpy(stream->buffer + stream->tail,buffer,sz);
		}
		else{
			auto slice = sizeof(stream->buffer - stream->tail);
			memcpy(stream->buffer + stream->tail,buffer,slice);
			memcpy(stream->buffer,buffer + slice,sz - slice);
		}
		stream->tail = (stream->tail + sz) % sizeof(stream->buffer);

	}while(true);
	return true;
}

extern "C"
char* fgets(char* str,int count,FILE* stream){
	if (str == nullptr || count == 0 || stream == nullptr)
		return nullptr;
	if (count == 1){
		*str = 0;
		return str;
	}
	--count;
	int index = 0;
	while(index < count){
		char ch;
		if (!block_read(stream,&ch,1))
			break;
		str[index++] = ch;
		if (ch == '\n')
			break;
	}
	if (index){
		str[index] = 0;
		return str;
	}
	return nullptr;
}

extern "C"
int fgetc(FILE* stream){
	if (stream == nullptr)
		return EOF;
	char ch;
	return block_read(stream,&ch,1) ? ch : EOF;
}

extern "C"
int ungetc(int ch,FILE* stream){
	if (ch == EOF || stream == nullptr)
		return EOF;
	auto new_tail = (stream->tail + 1) % sizeof(stream->buffer);
	if (new_tail == stream->head)
		return EOF;
	stream->buffer[stream->tail] = (char)ch;
	stream->tail = new_tail;
	return ch;
}

extern "C"
[[ noreturn ]]
void abort(void){
	fprintf(stderr,"abort @ %p\n",__builtin_return_address(0));

	exit_process(0x80000000);
}

extern "C"
void __main(void){
	static bool done = false;
	if (done)
		return;
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
	done = true;
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

qword rdtsc(void){
	dword lo,hi;
	__asm__ (
		"rdtsc"
		: "=a" (lo), "=d" (hi)
	);
	return (((qword)hi) << 32) | lo;
}

char** parse_commandline(char* const cmd,dword length,unsigned& argc){
	argc = 1;
	auto ptr = cmd;
	char prev = 0;
	for (unsigned i = 0;i < length;++i,++ptr){
		if (!isspace(*ptr) && isspace(prev)){
			++argc;
		}
		prev = *ptr;
	}
	char** argv = (char**)operator new(sizeof(char*)*argc);
	if (argv == nullptr)
		return nullptr;
	argv[0] = cmd;
	ptr = cmd;
	prev = 0;
	unsigned pos = 1;
	for (unsigned i = 0;i < length;++i,++ptr){
		auto ch = *ptr;
		if (!isspace(ch) && isspace(prev)){
			if (pos >= argc)
				break;
			argv[pos++] = ptr;
		}
		if (isspace(ch) && !isspace(prev)){
			*ptr = 0;
		}
		prev = ch;
	}
	return (pos == argc) ? argv : nullptr;
}

extern "C"
[[ noreturn ]]
void uos_entry(void* entry,void* imgbase,void* env,void* stk_top){
	image_base = imgbase;
	environment = (const char*)env;
	guard_value = get_time() ^ rdtsc();
	__main();
	int return_value = (-1);
	do{
		//prepare arguments
		HANDLE ps = get_process();
		dword commandline_size = 0;
		if (get_command(ps,nullptr,&commandline_size) != TOO_SMALL)
			break;
		commandline_size = align_up(commandline_size + 1,0x10);
		char* commandline = (char*)operator new(commandline_size);
		if (commandline == nullptr)
			break;
		if (get_command(ps,commandline,&commandline_size) != SUCCESS)
			break;
		commandline[commandline_size] = 0;
		close_handle(ps);
		unsigned argc;
		char** argv = parse_commandline(commandline,commandline_size,argc);
		if (argv == nullptr)
			break;
		return_value = main(argc,argv);
	}while(false);
	exit(return_value);
}

constexpr size_t huge_size = PAGE_SIZE/2;

void* operator new(size_t size){
	if (size <= huge_size){
		auto ptr = heap.allocate(size);
		if (ptr)
			return ptr;
	}
	else{
		auto page_count = align_up(size,PAGE_SIZE)/PAGE_SIZE;
		auto va = vm_reserve(0,page_count);
		if (va){
			if (SUCCESS == vm_commit(va,page_count)){
				return va;
			}
			vm_release(va,page_count);
		}
	}
	abort();
}

void* operator new(size_t,void* pos){
	return pos;
}

void operator delete(void* ptr,size_t size){
	if (!ptr)
		return;
	if (size <= huge_size){
		heap.release(ptr,size);
	}
	else{
		assert(0 == ((qword)ptr % PAGE_SIZE));
		auto page_count = align_up(size,PAGE_SIZE)/PAGE_SIZE;
		vm_release(ptr,page_count);
	}
}

void* malloc(size_t size){
	size = 0x10 + align_up(size,8);
	auto ptr = (dword*)operator new(size);
	ptr[0] = guard_value;
	ptr[1] = size;
	ptr[2] = size >> 32;
	ptr[3] = guard_value >> 32;
	return (ptr + 4);
}

void free(void* p){
	auto ptr = (dword*)p - 4;
	if (ptr[0] != guard_value || ptr[3] != (guard_value >> 32))
		abort();
	auto size = ((size_t)ptr[2] << 32) | ptr[1];
	operator delete(ptr,size);
}