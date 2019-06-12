#include "debugger.h"
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <thread>


using namespace std;
using namespace UOS;


static const char* hexchar = "0123456789ABCDEF";

//BOOL __stdcall OnEnumSymbol(PSYMBOL_INFO info, ULONG size, PVOID id) {
//	if (size) {
//		IMAGEHLP_LINE64 line;
//		DWORD off = 0;
//		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
//
//		if (SymGetLineFromAddr64(id, info->Address, &off, &line))
//			cout << "in " << line.FileName << " line " << dec << line.LineNumber << "\t";
//		cout << info->Name << "\tat 0x" << hex << info->Address << " in " << dec << info->Size << " bytes" << endl;
//	}
//	return TRUE;
//}




void Debugger::reg_dump(const UOS::CONTEXT& p) {
	cout << hex << "registers" << endl;
	cout << "rax  " << p.rax << "\tr8   " << p.r8 << endl;
	cout << "rcx  " << p.rcx << "\tr9   " << p.r9 << endl;
	cout << "rdx  " << p.rdx << "\tr10  " << p.r10 << endl;
	cout << "rbx  " << p.rbx << "\tr11  " << p.r11 << endl;
	cout << "rsi  " << p.rsi << "\tr12  " << p.r12 << endl;
	cout << "rdi  " << p.rdi << "\tr13  " << p.r13 << endl;
	cout << "rbp  " << p.rbp << "\tr14  " << p.r14 << endl;
	cout << "rsp  " << p.rsp << "\tr15  " << p.r15 << endl;
	cout << "CS   " << p.cs << "\tSS   " << p.ss << endl;
	cout << "rip  " << p.rip << "\tflag " << p.rflags << endl;

}


void Debugger::packet_dump(const string& str) {
	unsigned cnt = 0;
	for (auto it = str.cbegin(); it != str.cend(); ++it, ++cnt) {
		if (cnt & 0x0F)
			;
		else
			cout << endl;

		cout << hexchar[(*it >> 4)] << hexchar[(*it & 0x0F)] << ' ';
	}
	cout << endl;

}

Debugger::w::w(Pipe& p) : pipe(p) {}

void Debugger::w::put(const char* str) {
	unique_lock<mutex> lck(m);
	while(!data.empty())
		avl.wait(lck);
	data = str;

	pipe.write(data);
	//pipe.wake();
	//lock_guard<mutex> lcka(avl);
	avl.wait(lck);
}


//void Debugger::w::step(void) {
//	lock_guard<mutex> lck(m);
//
//	if (!data.empty())
//		pipe.write(data);
//}

void Debugger::w::fin(void) {
	unique_lock<mutex> lck(m);
	data.clear();
	avl.notify_all();
}



void Debugger::pump(void) {
	string str;
	while (init == false)
		this_thread::yield();

	while (!quit) {
		pipe.read(str);

		istringstream ss(str);
		ss.exceptions(ios::badbit | ios::failbit | ios::eofbit);
		try {
			switch (ss.get()) {
			case 'A':	//address
				ss.read((char*)&cur_addr, sizeof(qword));
				cout << "at 0x" << hex << cur_addr;
				if (symbol.resolve(cur_addr, str))
					cout << '\t' << str;
				cout << endl;
				control.fin();
				break;
			case 'C':	//comfirm
				control.fin();
				continue;
			case 'B':	//breakpoint
			{
				static const char* breakname[] = { "DE","DB","NMI","BP","OF","BR","UD","NM","DF","??","TS","NP","SS","GP","PF","??","MF","AC","MC","XM" };

				byte type = ss.get();
				cout << (type < 19 ? breakname[type] : "??") << " : " << dec << (dword)type << endl;

				ui_break.notify();
				continue;
				//control transfer
				//ui_lock.notify_one();
				//break;
			}
			case 'P':	//print
				cout << ss.rdbuf() << endl;
				continue;
			case 'T':	//stack
			{
				byte count = ss.get();
				while (count--) {
					qword addr;
					ss.read((char*)&addr, sizeof(qword));
					cout << hex << addr;
					if (symbol.resolve(addr, str))
						cout << '\t' << str;
					cout << endl;
				}

				break;
			}
			case 'M':	//memory
			{
				qword base;
				word count;
				ss.read((char*)&base, sizeof(qword));
				ss.read((char*)&count, sizeof(word));
				while (count--) {
					if (base & 0x0F)
						;
					else
						cout << endl << hex << base << '\t';

					byte c = ss.get();
					cout << hexchar[(c >> 4)] << hexchar[(c & 0x0F)] << ' ';
					base++;
				}
				cout << endl;
				break;
			}
			case 'I':	//info
				switch (ss.get()) {
				case 'R':	//reg
				{
					UOS::CONTEXT reg;
					ss.read((char*)&reg, sizeof(UOS::CONTEXT));
					reg_dump(reg);
					break;
				}




				}
				break;
			}
			//this_thread::yield();
			ui_reply.notify();

		}
		catch (ios::failure&) {
			;
		}

	}
}

void Debugger::run(void) {
	string str;
	init = true;
	while (!quit) {
		ui_break.wait();
		control.put("W");
		ui_reply.wait(chrono::milliseconds(2000));

		while (!quit) {
			cout << "dbg > ";
			do {
				getline(cin, str);
			} while (cin.good() ? false : (cin.clear(),ui_break.wait(), true));
			if (str.empty())
				str = command;
			else
				command = str;
			stringstream ss(str);

			switch (~0x20 & ss.get()) {
			case 'Q':	//quit
				quit = true;
				str = "Q";
				break;
			case 'H':	//help
				cout << "Q\tquit" << endl;
				cout << "H\thelp" << endl;
				cout << "G\tgo" << endl;
				cout << "S\tstep" << endl;
				//cout << "P\tpass" << endl;
				cout << "T [count]\tstack dump" << endl;
				cout << "B symbol\tset breakpoint" << endl;
				cout << "R\tshow registers" << endl;
				cout << "C\tcontrol registers" << endl;
				cout << "D\tdebug registers" << endl;
				cout << "X\tSSE registers" << endl;
				cout << "E\tprocessor environment" << endl;
				cout << "V base len\tvirtual memory" << endl;
				cout << "M base len\tphysical memory" << endl;
				cout << "O\topen source file" << endl;
				continue;
			case 'G':	//go
				str = "G";
				break;

			case 'S':	//step in
				str = "S";
				break;
			//case 'P':	//pass
			//	str = "GP";
			//	TODO set bp instead
			//	break;
				//case 'R':	//step out
				//	str = "GR";
				//	break;
			case 'T':	//stack
			{
				dword cnt = 8;
				ss >> dec >> cnt;

				if (cnt >= 0x100) {
					cout << "Stack dump supports up to 255 levels" << endl;
					continue;
				}
				str = "T";
				str += (byte)cnt;
				break;
			}
			case 'B':	//breakpoint
			{
				string sym;
				ss >> sym;
				if (sym.empty()) {
					cout << "B symbol/address" << endl;
					continue;
				}
				qword addr = 0;
				if (sym.at(0) == '0' && sym.at(1) == 'x') {
					addr = strtoull(sym.c_str(), NULL, 0);
				}
				else {
					addr = symbol.resolve(sym);
				}

				if (!addr) {
					cout << "unresolved symbol:\t" << sym << endl;
					continue;
				}

				str = "BA";
				str.append((char*)&addr, sizeof(qword));

				break;
			}
			case 'R':	//regs
				str = "IR";
				break;
			case 'C':	//cr
				str = "IC";
				break;
			case 'D':	//dr
				str = "ID";
				break;
			case 'X':	//sse
				str = "IE";
				break;
			case 'E':	//ps
				str = "IP";
				break;
			case 'V':	//vm
			{
				qword base = 0;
				word size = 0;
				ss >> hex >> base >> dec >> size;
				if (!size) {
					cout << "v base len" << endl;
					continue;
				}
				ss.str(string());
				ss.clear();
				ss << 'V';
				ss.write((char*)&base, sizeof(qword));
				ss.write((char*)&size, sizeof(word));
				str = ss.str();

				break;
			}
			case 'M':	//pm
			{
				qword base = 0;
				word size = 0;
				ss >> hex >> base >> dec >> size;
				if (!size) {
					cout << "m base len" << endl;
					continue;
				}
				ss.str(string());
				ss.clear();
				ss << 'M';
				ss.write((char*)&base, sizeof(qword));
				ss.write((char*)&size, sizeof(word));
				str = ss.str();

				break;
			}
			case 'O':	//open source file
			{
				auto line = symbol.source(cur_addr);
				if (!line.first)
					cout << "no source at " << hex << cur_addr << endl;
				else {
					ss.str(string());
					ss << '\"' << line.first << "\" -n" << dec << line.second;
					str = ss.str();
					ShellExecute(NULL, "open", editor.c_str(), str.c_str(), NULL, SW_SHOW);
				}
				continue;
			}


			default:
				cout << "unknown command" << endl;
				continue;
			}

			byte c = str.at(0);
			if (c == 'G' || c == 'S' || c=='Q') {
				ui_break.reset();
				control.put(str.c_str());
				break;
			}

			pipe.write(str);
			ui_reply.wait(chrono::milliseconds(2000));

		}
	}
}


Debugger::Debugger(const char* p, const char* s, const char* e) : pipe(p), symbol(s), editor(e), command("h"), control(pipe), quit(false),init(false) {
	thread(mem_fn(&Debugger::pump),this).detach();
}

Debugger::~Debugger(void) {
	quit = true;
}

void Debugger::pause(void) {
	control.put("P");
	ui_break.notify();
}


