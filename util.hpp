#pragma once

#define HIGHADDR(x) ( 0xFFFF800000000000 | (x) )


namespace UOS{
	
	template<typename T>
	void swap(T& a,T& b){
		T t=a;
		b=a;
		a=t;
	}
	
	template<typename T>
	T& min(T& a,T& b){
		return (a<b) ? a : b;
	}
	
	template<typename T>
	const T& min(const T& a,const T& b){
		return (a<b) ? a : b;
	}
	
	template<typename T>
	T& max(T& a,T& b){
		return (a>b) ? a : b;
	}
	
	template<typename T>
	const T& max(const T& a,const T& b){
		return (a>b) ? a : b;
	}
	
	
	template<typename T>
	size_t match(T a,T b,size_t cnt){
		size_t i=0;
		for (;i<cnt;i++){
			if (*a==*b)
				;
			else
				break;
			++a;
			++b;
		}
		return i;
	}
	
	extern "C" void* memset(void*,int,size_t);

};