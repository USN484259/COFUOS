#include "debugger.h"
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>


using namespace std;
using namespace UOS;


static const char* hexchar = "0123456789ABCDEF";

fill_guard::fill_guard(ostream& out, char c) : o(out), fill(out.fill(c)) {}
fill_guard::~fill_guard(void) {
	o.fill(fill);
}

void Debugger::color(word color) {
	static const HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hCon, color);
}

dword Debugger::getnumber(istream& ss) {
	string str;
	ss >> str;
	return strtoul(str.c_str(), nullptr, 0);
}


void Debugger::show_reg(const UOS::CONTEXT& p) {
	//cout << hex << "registers" << endl;
	fill_guard fill(cout, '0');
	cout << hex;
	cout << "rax  " << setw(16) << p.rax << "\tr8   " << setw(16) << p.r8 << endl;
	cout << "rcx  " << setw(16) << p.rcx << "\tr9   " << setw(16) << p.r9 << endl;
	cout << "rdx  " << setw(16) << p.rdx << "\tr10  " << setw(16) << p.r10 << endl;
	cout << "rbx  " << setw(16) << p.rbx << "\tr11  " << setw(16) << p.r11 << endl;
	cout << "rsi  " << setw(16) << p.rsi << "\tr12  " << setw(16) << p.r12 << endl;
	cout << "rdi  " << setw(16) << p.rdi << "\tr13  " << setw(16) << p.r13 << endl;
	cout << "rbp  " << setw(16) << p.rbp << "\tr14  " << setw(16) << p.r14 << endl;
	cout << "rsp  " << setw(16) << p.rsp << "\tr15  " << setw(16) << p.r15 << endl;
	cout << "CS   " << setw(16) << p.cs << "\tSS   " << setw(16) << p.ss << endl;
	cout << "rip  " << setw(16) << p.rip << "\tflag " << setw(16) << p.rflags << endl;
}



void Debugger::show_packet(const string& str) {
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

bool Debugger::get_symbol(qword addr, Debugger::Symbol& symbol) {
	sql.command("select address,length,symbol from Symbol where ?1 between address and address+length-1");
	sql << addr;
	if (sql.step()) {
		sql >> symbol.address >> symbol.length >> symbol.symbol;
		return true;
	}
	return false;
}

bool Debugger::get_disasm(qword addr, Debugger::Disasm& disasm) {
	sql.command("select address,length,line,fileid,disasm from Asm where ?1 between address and address+length-1");
	sql << addr;
	if (sql.step()) {
		sql >> disasm.address >> disasm.length >> disasm.line >> disasm.fileid >> disasm.disasm;
		return true;
	}
	return false;
}
/*
bool Debugger::get_source(const string& filename, int line, string& source) {
	sql.command("select text from Source where line=?1 and fileid=(select id from File where name=?2)");
	sql << line << filename;
	if (sql.step()) {
		sql >> source;
		return true;
	}
	return false;
}
*/
bool Debugger::expression(qword& res, const string& s) {

	istringstream ss(s);
	string str;

	qword base = cur_addr;
	unsigned index = 0;
	int off = 0;
	do {
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
}

//#error "count == 0 -> only symbol"
void Debugger::show_source(qword addr, int count) {
	Symbol symbol;
	fill_guard fill(cout, '0');

	cout << hex << setw(16) << addr;

	if (!get_symbol(addr, symbol)) {
		cout << "\t(no source)" << endl;
		return;
	}
	cout << '\t' << symbol.symbol << " + 0x" << hex << (addr - symbol.address) << endl;

	if (!count)
		return;

	qword base = addr;

	sql.command("select address,length from Asm where (fileid,line) in (select fileid,line from Asm where ?1 between address and address+length-1) order by address");
	sql << base;
	
	qword pred = base;
	while (sql.step()) {
		qword cur;
		int len;
		sql >> cur >> len;

		if (cur != pred) {
			addr = cur;
		}
		if (cur == base)
			break;

		pred = cur + len;
	}

	if (addr > base)
		addr = base;

	Disasm disasm;

	int fileid = 0;
	int line = 0;

	do {
		if (!get_disasm(addr, disasm)) {
			break;
		}
		if (fileid != disasm.fileid) {
			sql.command("select name from File where id=?1");
			sql << disasm.fileid;
			sql.step();
			string str;
			sql >> str;
			cout << "; File " << str << endl;
			fileid = disasm.fileid;
		}

		sql.command("select text from Source where fileid=?1 and line=?2");
		sql << disasm.fileid << disasm.line;
		if (sql.step()) {

			if (fileid != disasm.fileid || line != disasm.line) {
				cout << endl;
				if (!count--)
					return;
				string str;
				sql >> str;
				cout << "line " << dec << disasm.line << "\t\t" << str << endl << endl;
				fileid = disasm.fileid;
				line = disasm.line;
			}
		}
		else
			if (!count--)
				return;

		bool color_reset = false;
		if (disasm.address == cur_addr) {
			color_reset = true;
			color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_GREEN);
		}

		if (disasm.address < base)
			cout << "- " << hex << (base - disasm.address);
		else{
			cout << "+ " << hex << (disasm.address - base);
		}

		cout << '\t' << hex << setw(16) << disasm.address << '\t' << disasm.disasm << endl;

		if (color_reset)
			color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

		addr = disasm.address + disasm.length;

	} while (addr < symbol.address + symbol.length);

	if (count) {
		cout << hex << setw(16) << addr << "\t(no assembly)" << endl;
	}
}

void Debugger::step_arm(bool pass) {
	//if (step_info != std::pair<int, int>(0, 0))
	//	throw runtime_error("Sebugger::step_arm re-entry");
	step_info = std::pair<int, int>(0, 0);
	Disasm disasm;
	if (get_disasm(cur_addr, disasm)) {

		if (step_source)
			step_info = std::pair<int, int>(disasm.fileid, disasm.line);


		if ((step_pass = pass) && disasm.disasm.substr(0, 4) == "call") {
			string str = "B=";
			qword addr = disasm.address + disasm.length;
			str.append((const char*)&addr, sizeof(qword));
			pipe.write(str);
			return;
		}

	}
	pipe.write("S");
}

bool Debugger::step_check(void) {
	if (std::pair<int, int>(0, 0) == step_info)
		return true;

	Disasm disasm;
	if (!get_disasm(cur_addr, disasm) || std::pair<int, int>(disasm.fileid, disasm.line) != step_info) {
		step_info = std::pair<int, int>(0, 0);
		return true;
	}

	if (step_pass && disasm.disasm.substr(0, 4) == "call") {
		string str = "B=";
		qword addr = disasm.address + disasm.length;
		str.append((const char*)&addr, sizeof(qword));
		pipe.write(str);
		return false;
	}
	pipe.write("S");
	return false;
}

/*
void Debugger::open_source(const string& filename, int line, bool force_open) {
	if (auto_open || force_open) {


		stringstream ss;
		ss << '\"' << filename << "\" -n" << dec << line;
		string str = ss.str();
		ShellExecute(NULL, "open", editor.c_str(), str.c_str(), NULL, SW_SHOWNA);
	}
}
*/

void Debugger::clear(bool force) {
	if (auto_clear || force)
		system("cls");
}

void Debugger::stub(void) {
	string str;
	while (true) {
		pipe.read(str);

		istringstream ss(str);
		ss.exceptions(ios::badbit | ios::failbit | ios::eofbit);
		try {
			switch (ss.get()) {
			case 'C':	//comfirm
				break;
			case 'B':	//breakpoint
			{
				static const char* breakname[] = { "DE","DB","NMI","BP","OF","BR","UD","NM","DF","??","TS","NP","SS","GP","PF","??","MF","AC","MC","XM" };

				byte type = ss.get();
				ss.read((char*)&cur_addr, sizeof(qword));
				if (type != 1 || step_check()) {
					cout << endl;
					color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_RED);
					if (type == 0xFF) {
						cout << "BugCheck : ";
						qword errcode;
						ss.read((char*)&errcode, sizeof(qword));
						cout << dec << errcode;
					}
					else {
						cout << "Break\t" << (type < 19 ? breakname[type] : "??") << " : " << dec << (dword)type;
					}
					switch (type) {
					case 10:
					case 11:
					case 12:
					case 13:
					case 14:
					{
						qword errcode;
						ss.read((char*)&errcode, sizeof(qword));
						cout << "\terrcode " << hex << errcode;
					}
					}
					cout << endl;
					color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

					show_source(cur_addr, 2);

					running = false;
				}
				break;
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
					show_source(addr);
					/*
					cout << hex << setw(16) << addr;
					Symbol symbol;
					if (get_symbol(addr, symbol))
					cout << '\t' << symbol.symbol << " + 0x" << hex << (addr - symbol.address);
					//if (symbol.resolve(addr, str))
					//	cout << '\t' << str;
					cout << endl;
					*/
				}
				break;
			}
			case 'M':	//memory
			{
				qword base;
				word count;
				fill_guard fill(cout, '0');
				ss.read((char*)&base, sizeof(qword));
				ss.read((char*)&count, sizeof(word));
				if (count < 16) {
					cout << hex << setw(16) << base << '\t';
					while (count--) {
						byte c = ss.get();
						cout << hexchar[(c >> 4)] << hexchar[(c & 0x0F)] << ' ';
					}
					cout << endl;
				}
				else {
					if (base & 0x0F) {
						cout << hex << setw(16) << (base & ~0x0F) << '\t';
						str.assign(3 * (base & 0x0F), ' ');
						cout << str;
					}
					while (count--) {
						if (base & 0x0F)
							;
						else
							cout << endl << hex << setw(16) << base << '\t';

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
					show_reg(reg);
					break;
				}
				case 'D':	//DR
				{
					DR_STATE dr;

					ss.read((char*)&dr, sizeof(DR_STATE));

					qword mask = 0x03;
					for (int i = 0; i <= 3; i++, mask <<= 2) {
						if (dr.dr7 & mask) {
							cout << dec << i << "\tat\t";// << hex << setw(16) << dr.dr[i];
							show_source(dr.dr[i]);

							//Symbol symbol;
							//if (get_symbol(dr.dr[i], symbol)) {
							//	cout << '\t' << symbol.symbol << " + 0x" << hex << (dr.dr[i] - symbol.address) << endl;
							//if (dr.dr[i] == symbol.address)
							//	cout << '\t' << symbol.disasm;
							//else
							//	cout << "\t????\t(no assembly)";

							//}
							cout << endl;
						}
					}
					break;
				}



				}
				return;
			}
			break;
		}
		catch (ios::failure&) {
			;
		}
	}
}

bool Debugger::ui(void) {
	string str;
	while (true) {
		color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_BLUE);
		cout << "dbg > ";

		do {
			getline(cin, str);
		} while (cin.good() ? false : (cin.clear(), true));

		color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		clear();

		if (str.empty())
			str = command;
		else
			command = str;
		stringstream ss(str);
		str.clear();
		switch (~0x20 & ss.get()) {
		case 'Q':	//quit
			pipe.write("Q");
			return false;
		case 'H':	//help
			cout << "Q\tquit" << endl;
			cout << "H\thelp" << endl;
			cout << "G\tgo" << endl;
			cout << "G symbol\tgo and break at address" << endl;
			cout << "S[A/S]\tstep [on asm/sor level]" << endl;
			cout << "P[A/S]\tpass [on asm/sor level]" << endl;
			cout << "T [count]\tstack content" << endl;
			cout << "U [base [len]]\tshow disassembly and source" << endl;
			cout << "Y [symbol]\tsearch symbols" << endl;
			cout << "B symbol\tset breakpoint" << endl;
			cout << "R\tshow registers" << endl;
			cout << "C\tclear screen" << endl;
			cout << "C(+/-/?)\t toggle/query screen auto-clear" << endl;
			cout << "D id\tdelete breakpoint" << endl;
			cout << "L\tlist breakpoints" << endl;
			//cout << "X\tSSE registers" << endl;
			//cout << "E\tprocessor environment" << endl;
			cout << "V base len\tvirtual memory" << endl;
			cout << "M base len\tphysical memory" << endl;
			cout << "O [symbol]\topen source file" << endl;
			//cout << "O(+/-/?)\toggle/query source file auto open" << endl;
			continue;
		case 'G':	//go
			ss >> str;
			if (!str.empty()) {
				qword addr = 0;
				if (!expression(addr, str)) {
					cout << "unresolved symbol:\t" << str << endl;
					continue;
				}
				str = "B=";
				str.append((char*)&addr, sizeof(qword));
				//control.put(str);
				running = true;
				break;
			}
			str = "G";
			running = true;
			break;
		case 'P':	//pass
			switch (~0x20 & ss.peek()) {
			case 'A':
				step_source = false;
				break;
			case 'S':
				step_source = true;
				break;
			}
			step_arm(true);
			running = true;
			break;
			/*
			Disasm disasm;
			if (get_disasm(cur_addr, disasm)) {
			int fileid = disasm.fileid;
			int line = disasm.line;
			do {
			control.put("S");

			} while (get_disasm(cur_addr,disasm) && disasm.fileid == fileid && disasm.line == line);

			}
			sql.command("select address from Asm where address between ?1 and (select address+length-1 from Symbol where ?1 between address and address+length-1) and (line,fileid) not in (select line,fileid from Asm where ?1 between address and address+length-1) order by address");
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
			}*/
		case 'S':	//step in
			switch (~0x20 & ss.peek()) {
			case 'A':
				step_source = false;
				break;
			case 'S':
				step_source = true;
				break;
			}

			step_arm(false);
			running = true;
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
			if (!expression(addr, str)) {
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
		case 'L':	//dr
			str = "ID";
			break;
		//case 'X':	//sse
		//	str = "IE";
		//	break;
		//case 'E':	//ps
		//	str = "IP";
		//	break;
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

			if (!expression(base, str)) {
				cout << "unresolved symbol:\t" << str << endl;
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
			if (!expression(base, str)) {
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
			fill_guard fill(cout, '0');
			while (sql.step()) {
				qword addr;
				sql >> addr >> str;
				cout << dec << index++ << '\t' << hex << setw(16) << addr << '\t' << str << endl;
			}
			continue;
		}
		case 'U':	//disasm
		{
			qword addr = cur_addr;
			qword lim = 0;
			ss >> str;
			if (!str.empty()) {
				if (!expression(addr, str)) {
					cout << "unresolved symbol:\t" << str << endl;
					continue;
				}
				lim = getnumber(ss);

				//ss >> dec >> lim;
			}

			show_source(addr, lim ? lim : 4);
			continue;
		}
		case 'C':	//clear
			switch (ss.peek()) {
			case '+':
				auto_clear = true;
				continue;
			case '-':
				auto_clear = false;
				continue;
			case '?':
				cout << "screen auto-clear " << (auto_clear ? "true" : "false") << endl;
				continue;
			}
			clear(true);
			continue;
		case 'O':	//open source file
		{
			ss >> str;
			qword addr = cur_addr;
			if (!expression(addr, str)) {
				cout << "unresolved symbol:\t" << str << endl;
				continue;
			}
			Disasm disasm;
			if (get_disasm(addr, disasm) && disasm.fileid && disasm.line) {
				sql.command("select name from File where id=?1");
				sql << disasm.fileid;
				if (sql.step()) {
					sql >> str;
					ss.str(string());
					ss.clear();
					ss << '\"' << str << "\" -n" << dec << disasm.line;
					str = ss.str();
					ShellExecute(NULL, "open", editor.c_str(), str.c_str(), NULL, SW_SHOWNA);
				}
			}
			else {
				cout << "no source at " << hex << cur_addr << endl;
			}
			continue;
		}

		default:
			cout << "unknown command" << endl;
			continue;
		}
		if (!str.empty())
			pipe.write(str);
		return true;
	}

}

void Debugger::run(void) {
	color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	do {
		//clear();
		do {
			stub();
		} while (running);

	} while (ui());
}


Debugger::Debugger(const char* p, const char* d, const char* e) : pipe(p), sql(d, true), editor(e), command("h"), running(true),auto_clear(false), step_source(false), step_info(0, 0) {}


void Debugger::pause(void) {
	if (running) {
		pipe.write("P");
	}
}


