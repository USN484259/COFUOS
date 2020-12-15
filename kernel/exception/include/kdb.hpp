#pragma once
#include "types.hpp"
#include "exception_context.hpp"
#include "exception.hpp"

namespace UOS{
	extern bool kdb_enable;

	void kdb_init(word);
	size_t kdb_recv(word port, byte* dst, size_t lim);
	void kdb_send(word port, const byte* sor, size_t len);
	void kdb_break(byte id,exception_context* context);
	void dbgprint(const char* format,...);

}
