#include "uos.h"
#include "util.hpp"

using namespace UOS;

static void* image_base;
static const char* environment;

typedef void (*procedure)(void);

extern "C"{
	procedure __CTOR_LIST__;
	procedure __DTOR_LIST__;
	int main(int argc,char** argv);
}

extern "C"
size_t strlen(const char* str){
	size_t count = 0;
	while(*str++)
		++count;
	return count;
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