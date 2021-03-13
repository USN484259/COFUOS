#pragma once
#include "types.h"

namespace UOS{
	enum service_code : word {
		os_info = 0x0000,
		get_time = 0x0008,
		enum_process = 0x0010,
		get_message = 0x0020,
		dbg_print = 0x0028,
		display_fill = 0x0030,
		display_draw = 0x0038,
		get_thread = 0x0100,
		thread_id = 0x0104,
		get_handler = 0x0108,
		get_priority = 0x010C,
		exit_thread = 0x0110,
		kill_thread = 0x0114,
		set_handler = 0x0118,
		set_priority = 0x011C,
		create_thread = 0x0120,
		sleep = 0x0130,
		check = 0x0134,
		wait_for = 0x0138,
		get_process = 0x0200,
		process_id = 0x0204,
		process_info = 0x0208,
		get_command = 0x020C,
		exit_process = 0x0210,
		kill_process = 0x0214,
		process_result = 0x0218,
		create_process = 0x0220,
		open_process = 0x0224,
		handle_type = 0x0230,
		close_handle = 0x0238,
		vm_peek = 0x0300,
		vm_protect = 0x0308,
		vm_reserve = 0x0310,
		vm_commit = 0x0314,
		vm_release = 0x031C
	};
}