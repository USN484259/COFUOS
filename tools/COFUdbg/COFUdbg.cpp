#include "types.hpp"
#include "context.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include "pipe.h"
#include "symbol.h"
#include "disasm.h"

using namespace std;
using namespace UOS;

/*
void get_uint(istream& ss,unsigned& out){
	string str;
	ss >> str;
	if (!str.empty){
		size_t len = 0;
		unsigned res = stoul(str,&len,0);
		if (len)
			out = res;
	}
}
*/

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
public:
	Debugger(const char*,const char*);
	void run(void);
private:
	bool stub_get(void);
	bool stub_command(void);
	
	qword resolve(const string&);
	bool step_check(void);	//true -> handled, continue
	void show_source(qword addr,unsigned count,bool force_asm = false);

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
	mode(SOURCE)
{
	step_rec.mode = step_rec.IDLE;
}

void Debugger::run(void){
	do{
		while(false == stub_get());
	}while(stub_command());
}


bool Debugger::stub_get(void){
	string str;
	pipe.read(str);
	
	cout << "Packet data : ";
	for (auto c : str){
		cout << hexchar[(c >> 4) & 0x0F] << hexchar[c & 0x0F] << ' ';
	}
	cout << endl;


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
		cout << "COFU > ";
		string str;
		getline(cin,str);
		if (str.empty())
			str = last_command;
		else
			last_command = str;
		
		if (str.empty())
			continue;
		
		istringstream ss(str);
		ss >> str >> ws;

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
				unsigned len = default_len;
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
				cout << dec << i << '\t' << im.second << endl;
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

void Debugger::show_source(qword addr,unsigned count,bool show_asm){
	string sym_name;
	qword sym_base = symbol.get_symbol(addr,sym_name);
	qword line_base = symbol.get_line(addr);
	//if (!cur)
	//	cur = addr;

	if (sym_base)
		cout << sym_name << " +0x" << hex << ((line_base ? line_base : addr) - sym_base) << endl;
	
	if (mode == ASSEMBLY)
		show_asm = true;
	
	string source_file;
	while (count--){
		qword next_line = 0;
		if (line_base){
			auto& cur_file = symbol.line_file();
			if (cur_file != source_file){
				source_file = cur_file;
				auto pos = cur_file.find_last_of('\\');
				cout << "@ " << (pos == string::npos ? cur_file : cur_file.substr(pos + 1)) << endl;
			}
			cout << symbol.line_number();
			if (!show_asm && line_base == rip)
				cout << "==>\t\t";
			else
				cout << ":\t\t";
			cout << symbol.line_content() << endl;
			
			next_line = symbol.next_line();
			if (!show_asm){
				line_base = next_line;
				continue;
			}
		}

		qword cur = line_base ? line_base : addr;
		do{
			
			if (!sym_base)
				cout << hex << setw(16) << cur << '\t';
			else{
				if (cur > rip)
					cout << "+0x" << hex << (cur - rip) << '\t';
				else if (rip > cur)
					cout << "-0x" << hex << (rip - cur) << '\t';
				else
					cout << "==>\t";
				
			}
			string str;
			auto len = disasm.get(cur,str);
			if (!len){
				cout << "(no assembly)" << endl;
				return;
			}
			cout << str << endl;
			cur += len;
		}while(cur < next_line);
	}

}

bool Debugger::on_break(byte type,qword errcode){
	static const char* breakname[] = { "DE","DB","NMI","BP","OF","BR","UD","NM","DF","??","TS","NP","SS","GP","PF","??","MF","AC","MC","XM" };
	if (type == 1 && step_check())
		return false;	//handled, continue

	step_rec.mode = step_rec.IDLE;

	cout << endl;
	if (type == 0xFF)
		cout << "BugCheck : " << dec << errcode;
	else
		cout << "Break\t" << (type < 19 ? breakname[type] : "??") << " : " << dec << (dword)type;
	switch(type){
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
			cout <<"\terrcode = " << hex << errcode;
	}

	cout << endl;
	show_source(rip,default_len);
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
	if (!level || level >= 0x100){
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
	string str("B-\0");
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
	if (!len || len > 0x200){
		cout << "Mem dump no more than 512 bytes" << endl;
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
	show_source(addr,count,true);
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
