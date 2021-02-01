#pragma once
#include "types.hpp"
#include "process/include/context.hpp"
#include "exception.hpp"
#include "optional.hpp"

namespace UOS{
	class kdb_stub{
		static constexpr size_t buffer_size = 0x400;
		const word port;
		byte vector;
		qword errcode;
		context* conx = nullptr;
		size_t length = 0;
		byte buffer[buffer_size];

		void send_bin(void);
		void send(const char*);
		void recv(void);

		void on_break(void);

		void cmd_status(void);
		bool cmd_query(void);
		void cmd_read_regs(void);
		void cmd_write_regs(void);
		void cmd_read_vm(void);
		void cmd_write_vm(void);
		void cmd_breakpoint(void);
		bool cmd_run(void);

	public:
		kdb_stub(word);
		void signal(byte,qword,context*);
		
		void print(const char*,...);
	};
	extern optional<kdb_stub> debug_stub;
/*
	extern unsigned kdb_state;

	void kdb_init(word);
	size_t kdb_recv(word port, byte* dst, size_t lim);
	void kdb_send(word port, const byte* sor, size_t len);
	void kdb_break(byte id,qword errcode,context* context);
	void dbgprint(const char* format,...);
*/
}
