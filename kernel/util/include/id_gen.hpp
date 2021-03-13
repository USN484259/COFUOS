#pragma once
#include "types.h"
#include "intrinsics.hpp"

namespace UOS{
	template<typename T>
	struct id_gen{
		volatile T count = 0;
		inline T operator()(void){
			do{
				auto cur = count;
				if (cur == cmpxchg(&count,cur + 1,cur)){
					return cur;
				}
				mm_pause();
			}while(true);
		}
	};
}