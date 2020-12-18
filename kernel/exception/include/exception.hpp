#pragma once
#include "types.hpp"
#include "exception_context.hpp"
#include "../../sync/include/spin_lock.hpp"
#include "bugcheck.hpp"
#define THROW(x) BugCheck(corrupted,x)
#include "stack.hpp"
#undef THROW


namespace UOS{

	class exception{
	public:
		typedef bool (*CALLBACK)(exception_context*,void*);
	private:
		struct handler{
			CALLBACK callback;
			void* data;
		};
		spin_lock lock;
		simple_stack<handler> table[20];

	public:
		exception(void) = default;
		exception(const exception&) = delete;
		bool dispatch(byte id,exception_context* context);
		void push(byte id,CALLBACK callback,void* data = nullptr);
		CALLBACK pop(byte id);
	};
	extern exception eh;

}