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

dword Debugger::getnumber(istream& ss) {
	string str;
	ss >> str;
	return strtoul(str.c_str(), nullptr, 0);
}


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

void Debugger::w::put(const string& str) {
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

unsigned Debugger::addr2symbol(qword addr, Debugger::Symbol& symbol) {
	sql.command("select symbol,address from Symbol where ?1 between address and address+length-1");
	sql << addr;
	if (!sql.step())
		return 0;
	sql >> symbol.symbol >> symbol.base;

	sql.command("select address,disasm,length,line,name,id from Asm,File where Asm.fileid=File.id and ?1 between address and address+length-1");
	sql << addr;
	if (!sql.step())
		return 1;
	int id = 0;
	sql >> symbol.address >> symbol.disasm >> symbol.length >> symbol.line >> symbol.file >> id;

	sql.command("select text from Source where fileid=?1 and line=?2");
	sql << id << symbol.line;
	if (!sql.step())
		return 2;
	sql >> symbol.source;
	return 3;
/*
	sql.command("select Asm.address,Asm.disasm,Asm.length,Asm.line,File.name,Symbol.symbol,Symbol.address from Asm,file,Symbol where Asm.fileid=File.id and Asm.address between Symbol.address and Symbol.address+Symbol.length-1 and Asm.address=?1");
	sql << addr;
	if (!sql.step())
		return false;
	sql >> symbol.address >> symbol.disasm >> symbol.length >> symbol.line >> symbol.file >> symbol.symbol >> symbol.base;
	sql.command("select Source.text from Source,Asm where Source.fileid=Asm.fileid and Source.line=Asm.line and Asm.address=?1");
	sql << symbol.address;
	if (sql.step())
		sql >> symbol.source;

	return true;
	*/
}

bool Debugger::expression2addr(qword& res, const string& s) {

	istringstream ss(s);
	string str;

	qword base = cur_addr;
	unsigned index = 0;
	int off = 0;
	do{
		char c = ss.get();
		switch (c) {
		case istringstream::traits_type::eof():
			break;
		case '+':
			off = getnumber(ss);
			//ss >> dec >> off;
			break;
		case '-':
			off = -(int)getnumber(ss);
			//ss >> dec >> off;
			//off = -off;
			break;
		case '#':
			index = getnumber(ss);
			//ss >> dec >> index;
			break;
		//case '%':
		case '_':
		case '\\':
			str += '\\';
		default:
			str += c;
			continue;
		}
		if (!str.empty()) {
			if (str.at(0) == '0' && (~0x20 & str.at(1)) == 'X') {
				base = strtoull(str.c_str(), nullptr, 0);
			}
			else {
				sql.command("select address from Symbol where symbol like ?1 escape \'\\\' order by address");
				sql << str;
				if (!sql.step())
					return false;

				do {
					if (!index)
						break;
				} while (index-- && sql.step());
				if (index)
					return false;

				sql >> base;

			}
			str.clear();
		}
	} while (ss.good());

	res = base + off;
	return true;
/*
	if (str.at(0) == '0' && str.at(1) == 'x') {
		res = strtoull(str.c_str(), nullptr, 0);
		return true;
	}
	//TODO complex expressions
	sql.command("select address from Symbol where symbol=?1 order by address");
	sql << str;
	if (sql.step()) {
		sql >> res;
		return true;
	}

	//res = symbol.resolve(str);
	return false;
	*/
}


void Debugger::pump(void) {
	string str;
	while (init == false)
		this_thread::yield();
	try {
		while (!quit) {
			pipe.read(str);

			istringstream ss(str);
			ss.exceptions(ios::badbit | ios::failbit | ios::eofbit);
			try {
				switch (ss.get()) {
				case 'A':	//address
				{
					ss.read((char*)&cur_addr, sizeof(qword));
					cout << "at 0x" << hex << cur_addr;
					Symbol symbol;
					if (addr2symbol(cur_addr, symbol)) {
						cout << '\t' << symbol.symbol << " + 0x" << hex << (cur_addr - symbol.base) << endl;
						if (cur_addr == symbol.address)
							cout << '\t' << symbol.disasm << "\t; " << symbol.source;
						else
							cout << "\t????\t(no assembly)";
					}
					cout << endl;
					control.fin();
					break;
				}
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
						Symbol symbol;
						if (addr2symbol(addr, symbol))
							cout << '\t' << symbol.symbol << " + 0x" << hex << (addr - symbol.base);
						//if (symbol.resolve(addr, str))
						//	cout << '\t' << str;
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
					if (count < 16) {
						cout << hex << base << '\t';
						while (count--) {
							byte c = ss.get();
							cout << hexchar[(c >> 4)] << hexchar[(c & 0x0F)] << ' ';
						}
						cout << endl;
					}
					else {
						if (base & 0x0F) {
							cout << hex << (base & ~0x0F) << '\t';
							str.assign(3 * (base & 0x0F), ' ');
							cout << str;
						}
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
					}
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
					case 'D':	//DR
					{
						DR_STATE dr;

						ss.read((char*)&dr, sizeof(DR_STATE));

						qword mask = 0x03;

						for (int i = 0; i <= 3; i++, mask <<= 2) {
							if (dr.dr7 & mask) {
								cout << i << "\tat\t" << hex << dr.dr[i];
								Symbol symbol;
								if (addr2symbol(dr.dr[i], symbol)) {
									cout << '\t' << symbol.symbol << " + 0x" << hex << (dr.dr[i] - symbol.base) << endl;
									if (dr.dr[i] == symbol.address)
										cout << '\t' << symbol.disasm << "\t; " << symbol.source;
									else
										cout << "\t????\t(no assembly)";

								}
								//if (symbol.resolve(dr.dr[i], str))
								//	cout << '\t' << str;
								cout << endl;
							}
						}
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
	catch (exception& e) {
		cerr << typeid(e).name() << '\t' << e.what() << endl;
	}
}

void Debugger::run(void) {
	string str;
	init = true;
	while (!quit) {
		ui_reply.reset();
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
			str.clear();
			switch (~0x20 & ss.get()) {
			case 'Q':	//quit
				quit = true;
				str = "Q";
				break;
			case 'H':	//help
				cout << "Q\tquit" << endl;
				cout << "H\thelp" << endl;
				cout << "G [symbol]\tgo" << endl;
				cout << "S\tstep" << endl;
				cout << "P\tpass" << endl;
				cout << "T [count]\tstack dump" << endl;
				cout << "U [base [len]]\tshow disassembly" << endl;
				cout << "Y [symbol]\tsearch symbols" << endl;
				cout << "B symbol\tset breakpoint" << endl;
				cout << "R\tshow registers" << endl;
				cout << "C\tcontrol registers" << endl;
				cout << "D id\tdelete breakpoint" << endl;
				cout << "L\tlist breakpoints" << endl;
				cout << "X\tSSE registers" << endl;
				cout << "E\tprocessor environment" << endl;
				cout << "V base len\tvirtual memory" << endl;
				cout << "M base len\tphysical memory" << endl;
				cout << "O [symbol]\topen source file" << endl;
				continue;
			case 'G':	//go
				ss >> str;
				if (!str.empty()) {
					qword addr = 0;
					if (!expression2addr(addr, str)) {
						cout << "unresolved symbol:\t" << str << endl;
						continue;
					}
					str = "B=";
					str.append((char*)&addr, sizeof(qword));
					control.put(str);

				}
				str = "G";
				break;
			case 'P':	//pass

				sql.command("select min(address) from Asm where address between ?1 and (select address+length-1 from Symbol where ?1 between address and address+length-1) and (line,fileid)!=(select line,fileid from Asm where ?1 between address and address+length-1)");
				sql << cur_addr;
				if (sql.step()) {
					qword addr;
					sql >> addr;
					str = "B=";
					str.append((char*)&addr, sizeof(qword));
					control.put(str);

					str = "G";
					break;
				}

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
				dword cnt = getnumber(ss);
				if (!cnt)
					cnt = 8;
				
				//ss >> dec >> cnt;

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
				ss >> str;
				if (str.empty()) {
					cout << "B symbol/address" << endl;
					continue;
				}
				qword addr = 0;
				if (!expression2addr(addr,str)){
					cout << "unresolved symbol:\t" << str << endl;
					continue;
				}

				str = "B+";
				str.append((char*)&addr, sizeof(qword));

				break;
			}
			case 'D':	//delete breakpoint
			{
				unsigned i = 0;
				ss >> i;
				if (i && i <= 3)
					;
				else {
					cout << "index out of range" << endl;
					continue;
				}
				str = "B-";
				str += (byte)i;
				break;
			}
			case 'R':	//regs
				str = "IR";
				break;
			case 'C':	//cr
				str = "IC";
				break;
			case 'L':	//dr
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
				ss >> str;// >> dec >> size;
				size = getnumber(ss);
				//ss >> hex >> base >> dec >> size;
				if (!size) {
					cout << "v base len" << endl;
					continue;
				}

				if (!expression2addr(base, str)) {
					cout<< "unresolved symbol:\t" << str << endl;
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
				ss >> str;// >> dec >> size;
				size = getnumber(ss);
				//ss >> hex >> base >> dec >> size;
				if (!size) {
					cout << "m base len" << endl;
					continue;
				}
				if (!expression2addr(base, str)) {
					cout << "unresolved symbol:\t" << str << endl;
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
			case 'Y':	//symbol
			{
				str = "%";
				ss >> ws;
				do {
					char c = ss.get();
					switch (c) {
					case stringstream::traits_type::eof():
					case ' ':
						break;
					case '_':
					case '\\':
						str += '\\';
					default:
						str += c;
						continue;
					}
					break;
				} while (true);
				str += '%';

				sql.command("select address,symbol from Symbol where symbol like ?1 escape \'\\\' order by address");
				sql << str;
				unsigned index = 0;
				while (sql.step()) {
					qword addr;
					sql >> addr >> str;
					cout << dec << index++ << '\t' << hex << addr << '\t' << str << endl;
				}
				continue;
			}
			case 'U':	//disasm
			{
				qword addr = cur_addr;
				qword lim = 0;
				ss >> str;
				if (!str.empty()) {
					if (!expression2addr(addr, str)) {
						cout << "unresolved symbol:\t" << str << endl;
						continue;
					}
					lim = getnumber(ss);

					//ss >> dec >> lim;
				}
				if (!lim)
					lim = 16;

				//string filename;
				//std::pair<int, string> source;
				Symbol symbol;
				lim += addr;
				qword base = addr;
				int lvl = addr2symbol(addr, symbol);
				if (!lvl) {
					cout << "unresolved address\t" << hex << addr << endl;
					continue;
				}
				//symbol.address != addr) {
				//	cout << hex << addr << "\t????\t(no assembly)" << endl;
				//	continue;
				//}
				string filename(symbol.file);
				std::pair<int, string> source;
				string sym(symbol.symbol);

				cout << sym << " + 0x" << hex << addr - symbol.base << endl << endl;

				do {
					if (lvl < 2 || sym != symbol.symbol)
						break;

					if (filename != symbol.file) {
						cout << endl;
						filename = symbol.file;
						cout << "File :\t" << filename << endl;
					}
					if (lvl == 3 && (source.first != symbol.line || source.second != symbol.source)) {
						cout << endl;
						source.first = symbol.line;
						source.second = symbol.source;
						cout << "line " << dec << source.first << " :\t" << source.second << endl << endl;
					}

					cout << '+' << hex << addr - base << '\t' << addr << '\t';
					if (symbol.address == addr)
						cout << symbol.disasm << endl;
					else
						cout << "????\t(no assembly)" << endl;

					addr = symbol.address + symbol.length;

					//if (addr >= lim)
					//	break;

					//if (!addr2symbol(addr, symbol)) {
					//	cout << hex << addr << "\t????\t(no assembly)" << endl;
					//	break;
					//}

				} while (addr < lim && (lvl = addr2symbol(addr, symbol)));
				cout << endl;

				continue;
			}
			case 'O':	//open source file
			{
				ss >> str;
				qword addr = cur_addr;
				if (!expression2addr(addr, str)) {
					cout << "unresolved symbol:\t" << str << endl;
					continue;
				}
				Symbol symbol;
				if (3 != addr2symbol(cur_addr, symbol) || symbol.source.empty())
				//auto line = symbol.source(cur_addr);
				//if (!line.first)
					cout << "no source at " << hex << cur_addr << endl;
				else {
					ss.str(string());
					ss.clear();
					ss << '\"' << symbol.file << "\" -n" << dec << symbol.line;
					str = ss.str();
					ShellExecute(NULL, "open", editor.c_str(), str.c_str(), NULL, SW_SHOWNA);
				}
				continue;
			}


			default:
				cout << "unknown command" << endl;
				continue;
			}
			ui_reply.reset();
			byte c = str.at(0);
			if (c == 'G' || c == 'S' || c=='Q') {
				ui_break.reset();
				control.put(str);
				break;
			}
			if (c == 'B') {
				control.put(str);
				continue;
			}
			pipe.write(str);
			ui_reply.wait(chrono::milliseconds(2000));

		}
	}
}


Debugger::Debugger(const char* p, const char* d, const char* e) : pipe(p), sql(d,true), editor(e), command("h"), control(pipe), quit(false),init(false) {
	thread(mem_fn(&Debugger::pump),this).detach();
}

Debugger::~Debugger(void) {
	quit = true;
}

void Debugger::pause(void) {
	control.put(string("P"));
	ui_break.notify();
}


