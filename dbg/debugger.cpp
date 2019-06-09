#include "debugger.h"
#include <stdexcept>
#include <sstream>
#include <iostream>

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

bool Debugger::stub(void) {

	while (true) {
		string str;
		pipe.read(str);
		istringstream ss(str);
		ss.exceptions(ios::badbit | ios::failbit | ios::eofbit);

		try {
			switch (ss.get()) {
				//case 'I':	//initialize
				//{
				//	if (symbol)
				//		throw runtime_error("re-initialized");

				//	string name;
				//	getline(ss, name, (char)0);

				//	symbol = new Symbol(name.c_str());


				//	cout << name << " connected" << endl;
				//	continue;
				//}
			case 'P':	//dbgprint
				cout << (str.c_str() + 1) << endl;
				continue;

			case 'B':	//break
			{
				union {
					byte type;
					qword addr;
				}u;
				ss.read((char*)&u.addr, sizeof(qword));
				cur_addr = u.addr;

				ss.read((char*)&u.type, sizeof(byte));

				static const char* breakname[] = { "DE","DB","NMI","BP","OF","BR","UD","NM","DF","??","TS","NP","SS","GP","PF","??","MF","AC","MC","XM" };

				cout << (u.type < 19 ? breakname[u.type] : "Unknown break") << " : " << dec << (dword)u.type << endl;

				break;
			}
			case 'C':	//context
				switch (ss.get()) {
				case 'R':	//reg
				{
					UOS::CONTEXT reg;
					ss.read((char*)&reg, sizeof(UOS::CONTEXT));
					reg_dump(reg);

					break;
				}

				default:
					cout << "unknown context type" << endl;

				}
				break;

			case 'S':	//stack
			{
				byte count = ss.get();

				while (count--) {
					qword addr;
					ss.read((char*)&addr, sizeof(qword));

					string str;
					cout << hex << addr << '\t' << (symbol.resolve(addr, str) ? str : "") << endl;

				}

				break;
			}
			case 'M':	//mem
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
			default:
				packet_dump(str);
				throw runtime_error("unknown command");
			}
			break;

		}
		catch (ios::failure&) {
			packet_dump(str);
			throw runtime_error("bad packet format");
		}

	}
	return true;
}

bool Debugger::shell(void) {
	string str;

	cout << "at 0x" << hex << cur_addr;
	if (symbol.resolve(cur_addr, str))
		cout << '\t' << str;
	cout << endl;

	while (true) {
		cout << "dbg > ";
		do {
			getline(cin, str);
		}while (cin.good() ? false : (cin.clear(), true));


		stringstream ss(str);

		switch (~0x20 & ss.get()) {
		case 'Q':	//quit
			return false;

		case 'H':	//help
			cout << "Q\tquit" << endl;
			cout << "H\thelp" << endl;
			cout << "G\tgo" << endl;
			cout << "S\tstep" << endl;
			cout << "P\tpass" << endl;
			cout << "T [count]\tstack dump" << endl;
			cout << "B symbol\tset breakpoint" << endl;
			cout << "R\tshow registers" << endl;
			cout << "O\topen source file" << endl;
			continue;
		case 'G':	//go
			str = "GC";
			break;

		case 'S':	//step in
			str = "GS";
			break;
		case 'P':	//pass
			str = "GP";
			break;
			//case 'R':	//step out
			//	str = "GR";
			//	break;
		case 'T':	//stack
		{
			dword cnt;
			ss >> cnt;
			if (!ss.good())
				cnt = 8;

			if (cnt >= 0x100) {
				cout << "Stack dump supports up to 255 levels" << endl;
				continue;
			}
			str = "S";
			str += (char)cnt;
			break;
		}
		case 'B':	//breakpoint
		{
			string sym;
			ss >> sym;
			if (!ss.good()) {
				cout << "B symbol/address" << endl;
				continue;
			}
			qword addr = 0;
			if (sym.at(0) == '0' && sym.at(1) == 'x') {
				addr = (qword)atoll(sym.c_str() + 2);
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

		pipe.write(str);
		break;
	}
	return true;
}


Debugger::Debugger(const char* p, const char* s, const char* e) : pipe(p), symbol(s), editor(e) {}

void Debugger::run(void) {

	while (stub() && shell());

}

void Debugger::pause(void) {
	pipe.write("GB");
}


