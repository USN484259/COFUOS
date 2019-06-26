#include <iostream>
//#include <deque>
#include <map>
#include <Windows.h>
#include "dbghelp.h"
#pragma comment(lib,"dbghelp.lib")
#include "..\sqlite.h"
#include "fs_util.h"
#include "cod_scan.h"

using namespace std;



class dbgen {
	const HANDLE id;
	Sqlite sql;
	DWORD64 base;
	map<string, int> filelist;

	static BOOL __stdcall OnEnumSymbol(PSYMBOL_INFO info, ULONG size, PVOID p);

	void process_symbol(const PSYMBOL_INFO info);
public:
	dbgen(const char* database, const char* module);
	~dbgen(void);
	void run(void);

};


BOOL __stdcall dbgen::OnEnumSymbol(PSYMBOL_INFO info, ULONG size, PVOID p) {
	try {
		((dbgen*)p)->process_symbol(info);
		return TRUE;
	}
	catch (exception& e) {
		cerr << typeid(e).name() << '\t' << e.what() << endl;
		return FALSE;
	}
}

void dbgen::process_symbol(const PSYMBOL_INFO info) {

	sql.command("insert into Symbol values (?1,?2,?3)");
	sql << info->Address << (int)info->Size << info->Name;
	sql.step();

	IMAGEHLP_LINE64 line;
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
	DWORD off;
	if (SymGetLineFromAddr64(id, info->Address, &off, &line)) {
		//string str(line.FileName);
		//off = str.find_last_of('\\') + 1;
		//if (off >= str.size())
		//	throw runtime_error(str);
		//str = str.substr(off);
		string str = filepath(line.FileName).name_ext();
		if (filelist.find(str) == filelist.end()) {
			size_t i = filelist.size() + 1;
			filelist[str] = i+1;
			//pair<deque<string>::iterator,bool> res=filelist.insert(str.substr(off));
			sql.command("insert into File values(?1,?2)");
			sql << (int)filelist.size() << str;
			sql.step();
		}
	}
}

dbgen::dbgen(const char* database, const char* module) : id((HANDLE)(rand() & 0xFF)),sql(database) {
	if (!SymInitialize(id, NULL, FALSE))
		throw runtime_error("SymInitialize");
	SymSetOptions(SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES);
	base = SymLoadModuleEx(id, NULL, module, NULL, 0, 0, NULL, 0);
	if (!base)
		throw runtime_error(module);
	sql.transcation();
}

dbgen::~dbgen(void) {
	SymCleanup(id);
}

void dbgen::run(void) {
	try {

		sql.command("delete from Symbol");
		sql.step();
		sql.command("delete from File");
		sql.step();
		sql.command("delete from Source");
		sql.step();
		sql.command("delete from Asm");
		sql.step();

		if (!SymEnumSymbols(id, base, "*!*", OnEnumSymbol, this))
			throw runtime_error("SymEnumSymbols");

		cod_scanner cod(sql, filelist);

		file_finder finder;

		finder.find("*.cod");
		for (auto it = finder.begin(); it != finder.end(); ++it) {
			cod.scan_cod(*it);
		}
		finder.clear();

		finder.find("*.lst");
		for (auto it = finder.begin(); it != finder.end(); ++it) {
			string name = filepath(*it).name_ext();
			if (filelist.find(name) != filelist.end())
				throw runtime_error(*it);
			size_t i = filelist.size() + 1;
			filelist[name] = i;
			sql.command("insert into File values(?1,?2)");
			sql << (int)filelist.size() << name;
			sql.step();

			cod.scan_lst(*it, filelist.size());
		}

		sql.commit();

		int cnt;
		sql.command("select count(*) from Symbol");
		sql.step();
		sql >> cnt;
		cout << dec << cnt << " symbols\t";
		sql.command("select count(*) from File");
		sql.step();
		sql >> cnt;
		cout << cnt << " files\t";
		sql.command("select count(*) from Asm");
		sql.step();
		sql >> cnt;
		cout << cnt << " assembly lines" << endl;

	}
	catch (Sqlite::SQL_exception&) {
#ifdef _DEBUG
		sql.commit();
#else
		sql.rollback();
#endif
		throw;
	}
}

int main(int argc,char** argv) {
	const char* database = nullptr;
	const char* module = nullptr;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-')
			continue;
		switch (~0x20 & argv[i][1]) {
		case 'D':	//database
			database = argv[i] + 2;
			break;
		case 'M':	//module
			module = argv[i] + 2;
			break;
		}
	}
	
	if (database && module)
		;
	else {
		cout << "dbgen -d(database) -m(module)" << endl;
		return 0;
	}

	try {
		dbgen(database, module).run();
	}
	catch (exception& e) {
		cerr << typeid(e).name() << '\t' << e.what() << endl;
	}
	return 0;
}