#pragma once
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned long long qword;


namespace UOS{
	
	typedef void (*fun)(void);
	
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
		out_of_range

	};

}