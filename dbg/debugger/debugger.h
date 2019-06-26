#pragma once
#include <string>
#include <atomic>
#include <condition_variable>
#include "pipe.h"
#include "..\sqlite.h"
//#include "pump.h"
#include "..\..\context.hpp"


/*
kernel commands
Confirm
Break type addr errcode
Print text

kernel data
Info catagory {regs}		--Reg Cr Dr Sse Ps
sTack count {stkdump}
Mem base count {data}



debugger commands
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

class fill_guard {
	std::ostream& o;
	char fill;
public:
	fill_guard(std::ostream&, char);
	~fill_guard(void);

};



class Debugger {
	Pipe pipe;
	//Symbol symbol;
	Sqlite sql;
	std::string editor;
	std::string command;
	qword cur_addr;

	//Pump pump;

	//std::atomic<bool> quit;
	//std::atomic<bool> init;

	std::atomic<bool> running;
	bool auto_clear;
	bool step_source;
	bool step_pass;
	std::pair<int, int> step_info;


	struct Symbol {
		qword address;
		int length;
		std::string symbol;

	};

	struct Disasm {
		qword address;
		int length;
		int line;
		int fileid;
		std::string disasm;
	};


	void color(word);

	static dword getnumber(std::istream&);

	void show_reg(const UOS::CONTEXT& p);
	void show_packet(const std::string& str);

	//void show_source(qword addr);
	void show_source(qword addr, int line = 0);

	//void pump(void);
	void stub(void);
	bool ui(void);

	void clear(bool = false);

	bool expression(qword&, const std::string&);
	bool get_symbol(qword,Symbol&);
	bool get_disasm(qword, Disasm&);
	//bool get_source(const std::string& filename, int line, std::string& source);

	//void open_source(const std::string&, int, bool = false);

	void step_arm(bool pass);
	bool step_check(void);
public:
	Debugger(const char* p, const char* d, const char* e);
	void run(void);
	void pause(void);

};

