#pragma once
#include "types.hpp"
#include "process/include/context.hpp"
#include "sync/include/spin_lock.hpp"

#include "stack.hpp"

namespace UOS{

	class exception{
	public:
		typedef bool (*CALLBACK)(byte,qword,context*,void*);
	private:
		struct handler{
			CALLBACK callback;
			void* data;
		};
		spin_lock lock;
		simple_stack<handler> table[20];

	public:
		exception(void);
		exception(const exception&) = delete;
		bool dispatch(byte id,qword errcode,context* context);
		void push(byte id,CALLBACK callback,void* data = nullptr);
		CALLBACK pop(byte id);
	};
	extern exception eh;

}