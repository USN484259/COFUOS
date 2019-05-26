#pragma once
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned long long qword;


namespace UOS{
	template<typename A,typename B>
	struct pair{
		A first;
		B second;
	};


}