#pragma once
#include "types.hpp"

#define BITMASK(i) ( ( (qword)1<<(qword)(i) ) - 1 )
#define BIT(i) ( (qword)1 << ( (qword)(i)  ) )


namespace UOS{
	template< typename T > struct remove_reference      {typedef T type;};
	template< typename T > struct remove_reference<T&>  {typedef T type;};
	template< typename T > struct remove_reference<T&&> {typedef T type;};
	
	template< typename T >
	typename remove_reference<T>::type&& move( T&& t ){
		return static_cast<typename remove_reference<T>::type&&>(t);
	}
	
	template<typename T>
	void swap(T& a,T& b){
		T t(move(a));
		a=move(b);
		b=move(t);
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
	bool equal(const T& a,const T& b){
		return a==b;
	}
	template<typename T>
	bool not_equal(const T& a,const T& b){
		return a!=b;
	}
	
	template<typename T>
	bool less(const T& a,const T& b){
		return a<b;
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
	
	template<typename T>
	T align_down(T value,size_t align){
		return value & (T)~(align-1);
	}
	
	template<typename T>
	T align_up(T value,size_t align){
		return align_down((T)(value + align - 1),align);
	}
	
};