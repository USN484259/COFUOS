#pragma once
#include "types.h"

namespace UOS{
	bool user_exception(qword& rip,qword& rsp,dword errcode);
	[[ noreturn ]]
	void process_loader(qword env,qword img_base,qword img_size,qword header_size);
	[[ noreturn ]]
	void user_entry(qword entry,qword arg,qword stk_top,qword stk_size);
}