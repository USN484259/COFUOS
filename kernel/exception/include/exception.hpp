#pragma once
#include "types.hpp"
#include "process/include/context.hpp"
#include "sync/include/spin_lock.hpp"

#include "stack.hpp"

namespace UOS{

	class exception{
	public:
		exception(void);
		exception(const exception&) = delete;
		bool dispatch(byte id,qword errcode,context* context);
	};
	extern exception eh;

}