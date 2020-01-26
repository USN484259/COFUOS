#include "hash.hpp"

using namespace UOS;
//fasthash algorithm by ZilongTan
//https://github.com/ZilongTan/fast-hash
/* The MIT License

   Copyright (C) 2012 Zilong Tan (eric.zltan@gmail.com)


   Permission is hereby granted, free of charge, to any person

   obtaining a copy of this software and associated documentation

   files (the "Software"), to deal in the Software without

   restriction, including without limitation the rights to use, copy,

   modify, merge, publish, distribute, sublicense, and/or sell copies

   of the Software, and to permit persons to whom the Software is

   furnished to do so, subject to the following conditions:



   The above copyright notice and this permission notice shall be

   included in all copies or substantial portions of the Software.



   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,

   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF

   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND

   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS

   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN

   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN

   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE

   SOFTWARE.

*/

qword inline fasthash_mix(qword h){
		h ^= h>>23;
		h *= 0x2127599bf4325c37ULL;
		h ^= h>>47;
		return h;
}

qword UOS::fasthash(const void* ptr,size_t len){
	static const qword seed = rdseed();
	const qword magic = 0x880355f21e6d1965ULL;
	
	const byte* sor = (const byte*)ptr;
	qword result = seed ^ (len*magic);
	
	while(len>=8){
		result ^= fasthash_mix(*(qword*)sor);
		result *= magic;
		sor+=8 , len-=8;
	}
	
	if (len){
		qword h = 0;
		memcpy(&h,sor,len);
		result ^= fasthash_mix(h);
		result *= magic;
	}
	return fasthash_mix(result);
}


/*
#define MAKE_HASH(type) template<> struct hash<type>{ qword operator()(type val) const{ return fasthash(&val,sizeof(type)); } };
MAKE_HASH(unsigned char)
MAKE_HASH(signed char)
MAKE_HASH(short)
MAKE_HASH(unsigned short)
MAKE_HASH(int)
MAKE_HASH(unsigned int)
MAKE_HASH(long)
MAKE_HASH(unsigned long)
MAKE_HASH(long long)
MAKE_HASH(unsigned long long)
//MAKE_HASH(size_t)
MAKE_HASH(float)
MAKE_HASH(double)
MAKE_HASH(long double)
*/

