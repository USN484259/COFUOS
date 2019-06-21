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

static const HANDLE id = (HANDLE)0xFF;

DWORD64 base = 0;

BOOL __stdcall OnEnumSymbol(PSYMBOL_INFO info, ULONG size, PVOID p) {
	auto arg = (pair<Sqlite*, map<string,int>&>*)p;
	Sqlite* sql = arg->first;
	map<string,int>& filelist = arg->second;

	try {

		//{
		//	wchar_t* type = nullptr;
		//	if (SymGetTypeInfo(id, base, info->TypeIndex, TI_GET_SYMNAME, &type)) {

		//		cout << type << endl;
		//		LocalFree(type);
		//	}
		//}

		sql->command("insert into Symbol values (?1,?2,?3)");
		*sql << info->Address << (int)info->Size << info->Name;
		sql->step();

		IMAGEHLP_LINE64 line;
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		DWORD off;
		if (SymGetLineFromAddr64(id, info->Address, &off, &line)){
			//string str(line.FileName);
			//off = str.find_last_of('\\') + 1;
			//if (off >= str.size())
			//	throw runtime_error(str);
			//str = str.substr(off);
			string str = filepath(line.FileName).name_ext();
			if (filelist.find(str) == filelist.end()){
				filelist[str] = filelist.size();
			//pair<deque<string>::iterator,bool> res=filelist.insert(str.substr(off));
				sql->command("insert into File values(?1,?2)");
				*sql << (int)filelist.size() << str;
				sql->step();
			}
		}


	}
	catch (Sqlite::SQL_exception& e) {
		cerr << e.what() << endl;
		return FALSE;
	}
	return TRUE;
}





int main(int argc,char** argv) {
	Sqlite* sql = nullptr;
	try {
		if (!SymInitialize(id, NULL, FALSE))
			throw runtime_error("SymInitialize");
		SymSetOptions(SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES);
		base = SymLoadModuleEx(id, NULL, argv[1], NULL, 0, 0, NULL, 0);
		if (!base)
			throw runtime_error("SymLoadModuleEx");

		sql=new Sqlite(argv[2]);
		sql->transcation();

		sql->command("delete from Symbol");
		sql->step();
		sql->command("delete from File");
		sql->step();
		//sql->command("delete from Source");
		//sql->step();
		sql->command("delete from Asm");
		sql->step();
		
		map<string,int> filelist;
		auto arg = pair<Sqlite*, map<string,int>&>(sql, filelist);

		if (!SymEnumSymbols(id, base, "*!*", OnEnumSymbol, &arg))
			throw runtime_error("SymEnumSymbols");

		//sql->commit();
/*
		sql->command("select * from Symbol");
		while (sql->step()) {
			DWORD64 addr;
			string symbol;
			*sql >> addr >> symbol;
			cout << hex << addr << '\t' << symbol << endl;
		}
		map<int,string> source;
		sql->command("select * from File");
		while (sql->step()) {
			int index;
			string name;
			*sql >> index >> name;
			cout << dec << index << '\t' << name << endl;

			size_t base = name.find_last_of('\\') + 1;
			size_t lim = name.find_last_of('.');
			if (base >= name.size() || base >= lim)
				continue;
			if (".cpp" != name.substr(lim))
				continue;
			source[index] = name.substr(base, lim - base) + ".cod";
		}
		*/
		cod_scanner cod(*sql,filelist);

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
			filelist[name] = filelist.size();
			sql->command("insert into File values(?1,?2)");
			*sql << (int)filelist.size() << name;
			sql->step();

			cod.scan_lst(*it,filelist.size());
		}


		sql->commit();


	}
	catch (exception& e) {
		cerr << typeid(e).name() << '\t' << e.what() << endl;
		try {
#ifdef _DEBUG
			sql->commit();
#else
			sql->rollback();
#endif
		}
		catch (Sqlite::SQL_exception&) {}
	}
	delete sql;
	SymCleanup(id);
	return 0;
}