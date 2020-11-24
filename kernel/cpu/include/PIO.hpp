#pragma once
#include "hal.hpp"


namespace UOS{
	
	template<typename T>
	void io_read(word port,T& val);

	template<>
	inline void io_read<byte>(word port,byte& val){
		val=__inbyte(port);
	}
	template<>
	inline void io_read<word>(word port,word& val){
		val=__inword(port);
	}
	template<>
	inline void io_read<dword>(word port,dword& val){
		val=__indword(port);
	}
	
	template<typename T>
	void io_write(word port,T val);

	template<>
	inline void io_write<byte>(word port,byte val){
		__outbyte(port,val);
	}
	template<>
	inline void io_write<word>(word port,word val){
		__outword(port,val);
	}
	template<>
	inline void io_write<dword>(word port,dword val){
		__outdword(port,val);
	}
}