#include "types.hpp"
//#include "kdb.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <deque>
#include <string>
#include "pipe.h"
#include "symbol.h"
#include "disasm.h"
#include "exception_context.hpp"

using namespace std;
using namespace UOS;


class Debugger{
	static const unsigned default_len = 4;
	static const char* hexchar;

	Pipe pipe;
	Symbol symbol;
	Disasm disasm;
	string last_command;

	qword rip;
	enum {SOURCE,ASSEMBLY} mode;
	struct{
		enum {
			IDLE = 0,STEP,PASS
		} mode;
		qword line_base;
	} step_rec;

	bool virtual_terminal;
	enum COLOR : word {BLACK = 0,RED,GREEN,YELLOW,BLUE,MAGENTA,CYAN,WHITE};
	void color_set(COLOR);
	void color_reset(void);

public:
	Debugger(const char*,const char*);
	void run(void);
private:
	bool stub_get(void);
	bool stub_command(void);
	
	qword resolve(const string&);
	bool step_check(void);	//true -> handled, continue
	void show_addr_symbol(qword addr);
	void show_source(qword addr);
	//void show_asm(qword start,qword end = (-1),size_t max_line = 2*default_len);
	void windowed_asm(qword line_base,qword next_line);
	void show_asm(qword addr,size_t line_count);
	bool on_break(byte type,qword errcode);
	void on_stack_trace(const vector<qword>&);
	void on_memory_dump(qword base,const vector<byte>& buffer);
	void on_show_context(const exception_context& context);
	void on_show_dr(const DR_STATE& dr);

	//true -> packet sent, wait
	bool cmd_go(qword addr);
	bool cmd_pass(char = 0);
	bool cmd_step(char = 0);
	bool cmd_stack(unsigned level);
	bool cmd_bp(qword addr);
	bool cmd_del(unsigned index);
	bool cmd_reg(void);
	bool cmd_list(void);
	bool cmd_mem(char cmd,qword addr,unsigned len);
	void cmd_symbol(const string& str);
	void cmd_disasm(qword addr,unsigned count);
};

const char* Debugger::hexchar = "0123456789ABCDEF";

Debugger::Debugger(const char* pipe_name,const char* symbol_name) : 
	pipe(pipe_name),
	symbol(symbol_name),
	disasm(symbol_name),
	mode(SOURCE),
	virtual_terminal(false)
{
	step_rec.mode = step_rec.IDLE;
	do{
		HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
		if (out == INVALID_HANDLE_VALUE)
			break;
		dword mode;
		if (!GetConsoleMode(out, &mode))
			break;
		if (!SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
			break;
		virtual_terminal = true;
	}while(false);
}

void Debugger::run(void){
	do{
		while(false == stub_get());
	}while(stub_command());
}

void Debugger::color_set(COLOR color){
	if (virtual_terminal)
		cout << "\x1B[" << dec << (30 + color) << 'm';
}

void Debugger::color_reset(void){
	if (virtual_terminal)
		cout << "\x1B[0m";
}

bool Debugger::stub_get(void){
	string str;
	pipe.read(str);
	/*
	cout << "Packet data : ";
	for (auto c : str){
		cout << hexchar[(c >> 4) & 0x0F] << hexchar[c & 0x0F] << ' ';
	}
	cout << endl;
	*/
	istringstream ss(str);
	switch(ss.get()){
		case 'C':
			return true;
		case 'B':
		{
			byte type = ss.get();
			qword errcode = 0;
			ss.read((char*)&rip,sizeof(qword));
			ss.read((char*)&errcode,sizeof(qword));
			return on_break(type,errcode);
		}
		case 'P':
			cout << ss.rdbuf() << endl;
			return false;
		case 'T':
		{
			byte count = ss.get();
			vector<qword> stk((size_t)count);
			ss.read((char*)stk.data(),count*sizeof(qword));
			on_stack_trace(stk);
			return true;
		}
		case 'M':
		{
			qword base = 0;
			word len = 0;
			ss.read((char*)&base,sizeof(qword));
			ss.read((char*)&len,sizeof(word));
			vector<byte> buffer(len);
			ss.read((char*)buffer.data(),len);
			on_memory_dump(base,buffer);
			return true;
		}
		case 'I':
		{
			switch(ss.get()){
				case 'R':
				{
					exception_context context;
					ss.read((char*)&context,sizeof(exception_context));
					on_show_context(context);
					break;
				}
				case 'D':
				{
					DR_STATE dr;
					ss.read((char*)&dr,sizeof(DR_STATE));
					on_show_dr(dr);
					break;
				}
			}
			return true;
		}

		//default:
	}
	throw runtime_error("Unknown packet");
}

bool Debugger::stub_command(void){
	bool res;
	do{
		res = false;
		color_set(YELLOW);
		cout << "COFU > ";
		color_reset();
		string str;
		getline(cin,str);
		if (str.empty())
			str = last_command;
		else
			last_command = str;
		
		istringstream ss(str);
		ss >> str >> ws;
		if (str.empty())
			continue;
		
		switch(~0x20 & str.at(0)){
			case 'Q':
				pipe.write("Q");
				return false;
			case 'H':
				//TODO
				
				break;
			case 'G':
			{
				ss >> str;
				qword addr = resolve(str);
				res = cmd_go(addr);
				break;
			}
			case 'P':
				res = cmd_pass(ss.get());
				break;
			case 'S':
				res = cmd_step(ss.get());
				break;
			case 'T':
			{
				unsigned level = default_len;
				ss >> setbase(0) >> level;
				res = cmd_stack(level);
				break;
			}
			case 'B':
			{
				ss >> str;
				qword addr = resolve(str);
				res = cmd_bp(addr);
				break;
			}
			case 'D':
			{
				unsigned index = 0;
				ss >> setbase(0) >> index;
				res = cmd_del(index);
				break;
			}
			case 'R':
				res = cmd_reg();
				break;
			case 'L':
				res = cmd_list();
				break;
			case 'V':
			{
				unsigned len = default_len;
				ss >> str;
				ss >> setbase(0) >> len;
				qword addr = resolve(str);
				res = cmd_mem('V',addr,len);
				break;
			}
			case 'M':
			{
				unsigned len = default_len;
				ss >> str;
				ss >> setbase(0) >> len;
				qword addr = resolve(str);
				res = cmd_mem('M',addr,len);
				break;
			}
			case 'Y':
				ss >> str;
				cmd_symbol(str);
				break;
			case 'U':
			{
				unsigned len = default_len*2;
				ss >> str;
				ss >> setbase(0) >> len;
				qword addr = resolve(str);
				cmd_disasm(addr,len);
				break;
			}
			
			default:
				cout << "Unknown command" << endl;		
		}

	}while(res == false);
	return true;
}

qword Debugger::resolve(const string& str){
	istringstream ss(str);
	string sym_name;
	long offset = 0;
	unsigned index = 0;
	do {
		auto c = ss.get();
		switch(c){
			case istringstream::traits_type::eof():
				break;
			case '+':
			case '-':
			{
				ss >> setbase(0) >> offset;
				if (c == '-')
					offset *= (-1);
				break;
			}
			case '#':
				ss >> setbase(0) >> index;
				break;
			default:
				sym_name.append(1,c);
		}
	
	} while(ss.good());
	qword addr = 0;
	if (sym_name.empty())
		addr = rip;
	else if (sym_name.size() > 2 && sym_name.at(0) == '0' && (~0x20 & sym_name.at(1)) == 'X')
		addr = stoull(sym_name,nullptr,0);
	else{
		map<qword,string> table;
		symbol.match(sym_name,table);
		if (table.empty())
			return 0;
		if (table.size() == 1 && index < 2)
			addr = table.cbegin()->first;
		else if (index && index <= table.size()){
			auto it = table.cbegin();
			while(--index)
				++it;
			addr = it->first;
		}
		else{
			auto i = 1;
			for (auto im : table){
				cout << dec << i++ << '\t' << im.second << endl;
			}
		}
	}
	if (addr)
		return addr + offset;
	return 0;
}

//true -> handled, continue
bool Debugger::step_check(void){
	switch(step_rec.mode){
		case step_rec.PASS:
		{
			string str;
			auto len = disasm.get(rip,str);
			if (len && str.substr(0,4) == "call"){
				str.assign("B=");
				qword target = rip + len;
				str.append((const char*)&target,sizeof(qword));
				pipe.write(str);
				return true;
			}
		}
		case step_rec.STEP:
			if (step_rec.line_base == symbol.get_line(rip)){
				pipe.write("S");
				return true;
			}
	}
	return false;
}

void Debugger::show_addr_symbol(qword addr){
	string sym_name;
	qword sym_base = symbol.get_symbol(addr,sym_name);
	if (sym_base){
		color_set(GREEN);
		cout << sym_name << " +0x" << hex << (addr - sym_base);
		color_reset();
		cout << endl;
	}
}

void Debugger::show_source(qword addr){
	qword line_base = symbol.get_line(addr);

	unsigned line_count = default_len;
	string source_file;
	while(line_base && line_count){
		{
			auto& cur_file = symbol.line_file();
			if (cur_file != source_file){
				source_file = move(cur_file);
				auto pos = source_file.find_last_of('\\');
				cout << "in " << (pos == string::npos ? source_file : source_file.substr(pos + 1)) << endl;
			}
		}
		auto line_number = symbol.line_number();
		auto& line_content = symbol.line_content();
		auto next_line = symbol.next_line();
		bool this_line = (mode == SOURCE && rip >= line_base && (!next_line || rip < next_line));
		if (this_line)
			color_set(BLUE);
		cout << "line " << dec << line_number;
		if (this_line)
			cout << "==>\t\t";
		else
			cout << ":\t\t";
		color_set(CYAN);
		cout << line_content;
		color_reset();
		cout << endl;
		if (mode == ASSEMBLY){
			windowed_asm(line_base,next_line);
		}
		line_base = next_line;
		--line_count;
	}
	if (line_count){
		if (mode == SOURCE)
			cout << "(no source)" << endl;
		else if (line_count == default_len)
			windowed_asm(addr,0);
	}

}
void Debugger::windowed_asm(qword line_base,qword next_line){
	deque<qword> lines;
	qword cur = line_base;
	unsigned pinned = 0;
	do{
		auto len = disasm.get(cur);
		if (len == 0)
			break;
		while (lines.size() >= 2*default_len){
			lines.pop_front();
		}
		lines.push_back(cur);
		if (pinned){
			if (0 == --pinned)
				break;
		}
		else{
			if (cur >= rip)
				pinned = default_len;
		}
		cur += len;

		if (next_line && cur >= next_line){
			break;
		}
	}while(true);
	if (lines.empty()){
		cout << "(no assembly)" << endl;
	}
	else{
		if (lines.front() != line_base)
			cout << "\t..." << endl;
		show_asm(lines.front(),lines.size());
	}
}

void Debugger::show_asm(qword addr,size_t line_count){
	auto fill = cout.fill('0');
	while(line_count--){
		if (addr == rip)
			color_set(BLUE);
		cout << hex << setw(16) << addr;

		if (addr > rip)
			cout << " (+0x" << hex << setw(4) << (addr - rip) << ")\t";
		else if (rip > addr)
			cout << " (-0x" << hex << setw(4) << (rip - addr) << ")\t";
		else
			cout << " ( 0  ==>)\t";
		color_reset();
		string str;
		auto len = disasm.get(addr,str);
		if (len == 0){
			cout << "(no assembly)" << endl;
			return;
		}
		cout << str << endl;
		addr += len;
	}
	cout.fill(fill);
}

bool Debugger::on_break(byte type,qword errcode){
	static const char* breakname[] = { "DE","DB","NMI","BP","OF","BR","UD","NM","DF","??","TS","NP","SS","GP","PF","??","MF","AC","MC","XM" };
	if (type == 1 && step_check())
		return false;	//handled, continue

	step_rec.mode = step_rec.IDLE;

	cout << endl;
	if (type == 0xFF){
		color_set(RED);
		cout << "BugCheck @ " << hex << rip << " : " << dec << errcode;
	}
	else{
		color_set(YELLOW);
		cout << (type < 19 ? breakname[type] : "??") << " : " << dec << (dword)type << " @ " << hex << rip;
		switch(type){
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
				cout <<"\terrcode = " << hex << errcode;
		}
	}
	color_reset();
	cout << endl;
	show_addr_symbol(rip);
	show_source(rip);
	return true;
}

void Debugger::on_stack_trace(const vector<qword>& stk){
	for (auto data : stk){
		cout << hex << data;
		string sym_name;
		qword sym_base = symbol.get_symbol(data,sym_name);
		if (sym_base){
			cout << '\t' << sym_name << "+0x" << hex << (data - sym_base);
		}
		cout << endl;
	}
}

void Debugger::on_memory_dump(qword base,const vector<byte>& buffer){
	auto fill = cout.fill('0');

	if (buffer.size() < 16){
		cout << hex << setw(16) << base << '\t';
		for (auto data : buffer){
			cout << hexchar[(data >> 4)] << hexchar[(data & 0x0F)] << ' ';
		}
	}
	else{
		if (base & 0x0F){
			cout << hex << setw(16) << (base & ~0x0F) << '\t';
			string str(3 * (base & 0x0F),' ');
			cout << str;
		}

		for (auto data : buffer){
			if (base & 0x0F)
				;
			else
				cout << endl << hex << setw(16) << base << '\t';
			cout << hexchar[(data >> 4)] << hexchar[(data & 0x0F)] << ' ';
			++base;
		}

	}
	cout << endl;
	cout.fill(fill);
}

void Debugger::on_show_context(const exception_context& context){
	auto fill = cout.fill('0');
	cout << hex;
	cout << "rax  " << setw(16) << context.rax << "\tr8   " << setw(16) << context.r8 << endl;
	cout << "rcx  " << setw(16) << context.rcx << "\tr9   " << setw(16) << context.r9 << endl;
	cout << "rdx  " << setw(16) << context.rdx << "\tr10  " << setw(16) << context.r10 << endl;
	cout << "rbx  " << setw(16) << context.rbx << "\tr11  " << setw(16) << context.r11 << endl;
	cout << "rsi  " << setw(16) << context.rsi << "\tr12  " << setw(16) << context.r12 << endl;
	cout << "rdi  " << setw(16) << context.rdi << "\tr13  " << setw(16) << context.r13 << endl;
	cout << "rbp  " << setw(16) << context.rbp << "\tr14  " << setw(16) << context.r14 << endl;
	cout << "rsp  " << setw(16) << context.rsp << "\tr15  " << setw(16) << context.r15 << endl;
	cout << "CS   " << setw(16) << context.cs << "\tSS   " << setw(16) << context.ss << endl;
	cout << "rip  " << setw(16) << context.rip << "\tflag " << setw(16) << context.rflags << endl;
	cout.fill(fill);
}

void Debugger::on_show_dr(const DR_STATE& dr){
	qword mask = 0x03;
	for (int i = 0; i <= 3; i++, mask <<= 2) {
		if (dr.dr7 & mask) {
			cout << dec << i << "\tat\t" << hex << setw(16) << dr.dr[i];
			string sym_name;
			qword sym_base = symbol.get_symbol(dr.dr[i],sym_name);
			if (sym_base)
				cout << '\t' << sym_name << "+0x" << hex << (dr.dr[i] - sym_base);

			cout << endl;
		}
	}
}


bool Debugger::cmd_go(qword addr){
	if (!addr){
		pipe.write("G");
		return true;
	}
	string str("B=");
	str.append((const char*)&addr,sizeof(qword));
	pipe.write(str);
	return true;
}

bool Debugger::cmd_pass(char m){
	switch(~0x20 & m){
		case 'A':
			mode = ASSEMBLY;
			break;
		case 'S':
			mode = SOURCE;
			break;
	}
	qword addr = 0;
	if (mode == SOURCE){
		step_rec.mode = step_rec.PASS;
		return cmd_step();
	}
	else{	//ASSEMBLY
		string str;
		auto len = disasm.get(rip,str);
		if (len && str.substr(0,4) == "call")
				addr = rip + len;
	}
	if (!addr)
		return cmd_step();
	string str("B=");
	str.append((const char*)&addr,sizeof(qword));
	pipe.write(str);
	return true;
}

bool Debugger::cmd_step(char m){
	switch(~0x20 & m){
		case 'A':
			mode = ASSEMBLY;
			break;
		case 'S':
			mode = SOURCE;
			break;
	}
	if (mode == SOURCE){
		if (step_rec.mode != step_rec.PASS)
			step_rec.mode = step_rec.STEP;
		step_rec.line_base = symbol.get_line(rip);
	}
	pipe.write("S");
	return true;
}

bool Debugger::cmd_stack(unsigned level){
	if (!level || level > 0xFF){
		cout << "Stack trace no more than 255 levels" << endl;
		return false;
	}
	string str("T\0",2);
	str.at(1) = (byte)level;
	pipe.write(str);
	return true;
}

bool Debugger::cmd_bp(qword addr){
	if (!addr){
		cout << "Unresolved symbol" << endl;
		return false;
	}
	string str("B+");
	str.append((const char*)&addr,sizeof(qword));
	pipe.write(str);
	return true;
}

bool Debugger::cmd_del(unsigned index){
	if (index && index <= 3)
		;
	else{
		cout << "Index out of range" << endl;
		return false;
	}
	string str("B-\0",3);
	str.at(2) = (byte)index;
	pipe.write(str);
	return true;
}

bool Debugger::cmd_reg(void){
	pipe.write("IR");
	return true;
}

bool Debugger::cmd_list(void){
	pipe.write("ID");
	return true;
}

bool Debugger::cmd_mem(char cmd,qword addr,unsigned len){
	if (!addr){
		cout << "Unresolved symbol" << endl;
		return false;
	}
	if (!len || len > 0xFFFF){
		cout << "Mem dump too large" << endl;
		return false;
	}
	word tmp = (word)len;
	string str(1,cmd);
	str.append((const char*)&addr,sizeof(qword));
	str.append((const char*)&tmp,sizeof(word));
	pipe.write(str);
	return true;
}

void Debugger::cmd_symbol(const string& str){
	map<qword,string> table;
	symbol.match(str.empty() ? "*" : str,table);
	if (table.empty()){
		cout << "Not found" << endl;
	}
	else{
		for (auto im : table){
			cout << hex << setw(16) << im.first << '\t' << im.second << endl;
		}
	}
}

void Debugger::cmd_disasm(qword addr,unsigned count){
	if (!addr)
		addr = rip;
	show_addr_symbol(addr);
	show_asm(addr,count);
}

int main(int argc,char** argv){
	if (argc < 3){
		cerr << "COFUdbg <pipe_name> <symbol_name>" << endl;
		return 1;
	}
	try{
		Debugger(argv[1],argv[2]).run();
		return 0;
	}
	catch(exception& e){
		cerr << e.what() << endl;
	}
	return 1;
}
