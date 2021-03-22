#pragma once
#include "types.h"
#include "process/include/waitable.hpp"
#include "process/include/core_state.hpp"
#include "event.hpp"

namespace UOS{
	class pipe : public stream{
	public:
		enum MODE : byte {
			owner_write = 1,
			atomic_write = 0x10
		};
	private:
		const process* const owner;
		const byte mode;
		byte iostate = 0;
		bool named = false;
		const dword limit;
		byte* const buffer;
		dword head = 0;
		dword tail = 0;

		bool is_owner(void) const;
		bool is_full(void) const;
		bool is_empty(void) const;
	public:
		pipe(dword size,byte mode);
		~pipe(void);
		OBJTYPE type(void) const override{
			return PIPE;
		}
		IOSTATE state(void) const override{
			return (IOSTATE)iostate;
		}
		inline dword capacity(void) const{
			return limit;
		}
		bool relax(void) override;
		void manage(void*) override;
		bool check(void) override;
		REASON wait(qword us = 0,wait_callback = nullptr) override;
		dword read(void* dst,dword length);
		dword write(void const* sor,dword length);
	};
}