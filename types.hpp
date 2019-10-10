#pragma once
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned long long qword;

#pragma warning(disable: 4626 4514 4711)

namespace UOS{
	
	typedef void (*fun)(void);
	//typedef void (*exception_handler)(void* addr,void* stk,qword type,qword argu);

	template<typename A,typename B>
	struct pair{
		A first;
		B second;
	};

	enum status{
		success,
		bad_assert,
		null_ptr,
		bad_alloc,
		out_of_range,
		unhandled_exception

	};

}