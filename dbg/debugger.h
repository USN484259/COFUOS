#pragma once
#include <string>

#include "pipe.h"
#include "..\..\context.hpp"
#include "symbol.h"


/*
kernel commands
+/-/?
Break address type
Print text

kernel data
Context catagory {regs}		--Reg Cr Dr Sse Ps
Stack count {stkdump}
Mem base count {data}



debugger commands
+/-/?
Info catagory --reg cr dr sse ps
Go[Continue/Step/Pass/Return/Break]
Context name val
Breakpoint [type addr]
Stack count
Vm base len [data]
Pm base len [data]

*/


class Debugger {
	Pipe pipe;
	Symbol symbol;
	qword cur_addr;
	std::string editor;

	void reg_dump(const UOS::CONTEXT& p);

	void packet_dump(const std::string& str);

	bool stub(void);
	bool shell(void);


public:
	Debugger(const char* p, const char* s, const char* e);
	void run(void);
	void pause(void);

};