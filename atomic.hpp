#pragma once
#include "hal.hpp"


namespace UOS{
	
	template <typename T>
	T cmpxchg(volatile T*,T,T);
	
	template<>
	inline byte cmpxchg<byte>(volatile byte* val,byte xchg,byte cmp){
		return (byte)_InterlockedCompareExchange8((volatile char*)val,(char)xchg,(char)cmp);
	}
	template<>
	inline word cmpxchg<word>(volatile word* val,word xchg,word cmp){
		return (word)_InterlockedCompareExchange16((volatile short*)val,(short)xchg,(short)cmp);
	}
	template<>
	inline dword cmpxchg<dword>(volatile dword* val,dword xchg,dword cmp){
		return (dword)_InterlockedCompareExchange((volatile long*)val,(long)xchg,(long)cmp);
	}
	template<>
	inline qword cmpxchg<qword>(volatile qword* val,qword xchg,qword cmp){
		return (qword)_InterlockedCompareExchange64((volatile __int64*)val,(__int64)xchg,(__int64)cmp);
	}

	template <typename T>
	T xchg(volatile T*,T);

	template<>
	inline byte xchg<byte>(volatile byte* val,byte xchg){
		return (byte)_InterlockedExchange8((volatile char*)val,(char)xchg);
	}
	template<>
	inline word xchg<word>(volatile word* val,word xchg){
		return (word)_InterlockedExchange16((volatile short*)val,(short)xchg);
	}
	template<>
	inline dword xchg<dword>(volatile dword* val,dword xchg){
		return (dword)_InterlockedExchange((volatile long*)val,(long)xchg);
	}
	template<>
	inline qword xchg<qword>(volatile qword* val,qword xchg){
		return (qword)_InterlockedExchange64((volatile __int64*)val,(__int64)xchg);
	}

	template <typename T>
	T lockinc(volatile T*);
	
	template<>
	inline word lockinc<word>(volatile word* val){
		return (word)_InterlockedIncrement16((volatile short*)val);
	}
	template<>
	inline dword lockinc<dword>(volatile dword* val){
		return (dword)_InterlockedIncrement((volatile long*)val);
	}
	template<>
	inline qword lockinc<qword>(volatile qword* val){
		return (qword)_InterlockedIncrement64((volatile __int64*)val);
	}
	
	template <typename T>
	T lockdec(volatile T*);
	
	template<>
	inline word lockdec<word>(volatile word* val){
		return (word)_InterlockedDecrement16((volatile short*)val);
	}
	template<>
	inline dword lockdec<dword>(volatile dword* val){
		return (dword)_InterlockedDecrement((volatile long*)val);
	}
	template<>
	inline qword lockdec<qword>(volatile qword* val){
		return (qword)_InterlockedDecrement64((volatile __int64*)val);
	}
}