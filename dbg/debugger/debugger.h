#pragma once
#include <string>
#include <atomic>
#include <condition_variable>
#include "pipe.h"
#include "..\sqlite.h"
#include "event.h"
#include "..\..\context.hpp"


/*
kernel commands
At address
Confirm
Break type
Print text

kernel data
Info catagory {regs}		--Reg Cr Dr Sse Ps
sTack count {stkdump}
Mem base count {data}



debugger commands
Where
Info catagory --reg cr dr sse ps
Go
Step
Pause
Context name val
Breakpoint [type addr]
sTack count
Vm base len [data]
pM base len [data]
Quit debug

*/


class Debugger {
	Pipe pipe;
	//Symbol symbol;
	Sqlite sql;
	std::string editor;
	std::string command;
	qword cur_addr;
	std::atomic<bool> quit;
	std::atomic<bool> init;
	Event ui_break;
	Event ui_reply;

	class w{
		Pipe& pipe;
		std::string data;
		std::mutex m;
		std::condition_variable avl;
	public:
		w(Pipe&);
		void put(const std::string&);
		void fin(void);

	}control;

	struct Symbol{
		qword address;
		std::string disasm;
		int length;
		int line;
		std::string file;
		std::string source;
		std::string symbol;
		qword base;
	};


	void reg_dump(const UOS::CONTEXT& p);
	void packet_dump(const std::string& str);


	void pump(void);

	bool expression2addr(qword&, const std::string&);
	bool addr2symbol(qword,Symbol&);
public:
	Debugger(const char* p, const char* d, const char* e);
	~Debugger(void);
	void run(void);
	void pause(void);

};