#pragma once
#include "types.h"
#include "process/include/context.hpp"

namespace UOS{
	class kdb_stub{
		static constexpr size_t buffer_size = 0x400;
		static constexpr size_t sw_bp_count = 0x10;

		struct bp_slot{
			qword va;
			bool active;
			byte data;
		};	//simple one-shot software breakpoint implementation

		const word port;
		byte vector;
		qword errcode;
		context* conx;
		size_t length;
		bp_slot sw_bp[sw_bp_count];
		byte buffer[buffer_size];

		void send_bin(void);
		void send(const char*);
		void recv(void);

		bool sw_bp_match(qword va);
		bool breakpoint_sw(qword va,char op);

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
		word get_port(void) const;
		void signal(byte,qword,context*);
		
		//void print(const char*,...);
	};
	extern kdb_stub debug_stub;

}
