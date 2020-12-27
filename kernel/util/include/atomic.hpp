#pragma once
#include "cpu/include/hal.hpp"


namespace UOS{
	
	template <typename T>
	T cmpxchg(T volatile &,T,T);
	
	template<>
	inline byte cmpxchg<byte>(byte volatile & val,byte xchg,byte cmp){
		return (byte)_InterlockedCompareExchange8((char volatile*)&val,(char)xchg,(char)cmp);
	}
	template<>
	inline word cmpxchg<word>(word volatile & val,word xchg,word cmp){
		return (word)_InterlockedCompareExchange16((short volatile *)&val,(short)xchg,(short)cmp);
	}
	template<>
	inline dword cmpxchg<dword>(dword volatile & val,dword xchg,dword cmp){
		return (dword)_InterlockedCompareExchange((long volatile *)&val,(long)xchg,(long)cmp);
	}
	template<>
	inline qword cmpxchg<qword>(qword volatile & val,qword xchg,qword cmp){
		return (qword)_InterlockedCompareExchange64((__int64 volatile *)&val,(__int64)xchg,(__int64)cmp);
	}
	template<typename T>
	inline T* cmpxchg(T* volatile & val,T* xchg,T* cmp){
		return (T*)_InterlockedCompareExchangePointer((void* volatile*)&val,xchg,cmp);
	}

	template <typename T>
	T xchg(T volatile &,T);

	template<>
	inline byte xchg<byte>(byte volatile & val,byte other){
		return (byte)_InterlockedExchange8((char volatile *)&val,(char)other);
	}
	template<>
	inline word xchg<word>(word volatile & val,word other){
		return (word)_InterlockedExchange16((short volatile *)&val,(short)other);
	}
	template<>
	inline dword xchg<dword>(dword volatile & val,dword other){
		return (dword)_InterlockedExchange((long volatile *)&val,(long)other);
	}
	template<>
	inline qword xchg<qword>(qword volatile & val,qword other){
		return (qword)_InterlockedExchange64((__int64 volatile *)&val,(__int64)other);
	}
	template<typename T>
	inline T* xchg(T* volatile & val,T* other){
		return (T*)_InterlockedExchangePointer((void* volatile*)&val,other);
	}

	template <typename T>
	T lock_inc(T volatile &);
	
	template<>
	inline word lock_inc<word>(word volatile & val){
		return (word)_InterlockedIncrement16((short volatile *)&val);
	}
	template<>
	inline dword lock_inc<dword>(dword volatile & val){
		return (dword)_InterlockedIncrement((long volatile *)&val);
	}
	template<>
	inline qword lock_inc<qword>(qword volatile & val){
		return (qword)_InterlockedIncrement64((__int64 volatile *)&val);
	}
	
	template <typename T>
	T lock_dec(T volatile &);
	
	template<>
	inline word lock_dec<word>(word volatile & val){
		return (word)_InterlockedDecrement16((short volatile *)&val);
	}
	template<>
	inline dword lock_dec<dword>(dword volatile & val){
		return (dword)_InterlockedDecrement((long volatile *)&val);
	}
	template<>
	inline qword lock_dec<qword>(qword volatile & val){
		return (qword)_InterlockedDecrement64((__int64 volatile *)&val);
	}
}
