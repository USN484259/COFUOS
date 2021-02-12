#pragma once
#include "types.hpp"
#include "fasthash.h"

namespace UOS{
	template<typename T>
	struct hash{
		qword operator()(const T& val){
			return fasthash64(&val,sizeof(T),0);
		}
	};
}