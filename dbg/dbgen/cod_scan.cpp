#include "cod_scan.h"
#include "fs_util.h"
#include <fstream>
#include <string>
#include <sstream>

#ifdef _DEBUG
#include <iostream>
#endif

using namespace std;

cod_scanner::cod_scanner(Sqlite& q,const map<string,int>& lst) : sql(q),filelist(lst) {}

void cod_scanner::scan(const string& filename) {
	fstream sor(filename);
	if (!sor.is_open())
		throw runtime_error(filename);
	int line = 0;
	int fileid = 0;
	unsigned __int64 addr = 0;
	while (true) {
		string str;
		getline(sor, str);
		if (!sor.good())
			break;
#ifdef _DEBUG
		cout << str << endl;
#endif
		stringstream ss(str);
		ss >> str;
		if (!ss.good() || str.empty())
			continue;

		//disasm
		if ('0'==str.at(0)) {
			if (!line || !addr)
				throw runtime_error("bad cod format");

			string disasm;
			int len = 0;
			do {
				while (ss.good()) {
					ss >> str;
					if (!disasm.empty()) {
						disasm += ' ';
						disasm += str;
						continue;
					}
					if (str.size() > 2 || !isxdigit(str.at(0)) || !isxdigit(str.at(1))) {
						disasm = str;
						continue;
					}
					len++;

				}
				if (disasm.empty()) {
					getline(sor, str);
					if (!sor.good())
						break;
					ss.str(str);
				}
				else
					break;
			} while (true);

			addr += len;

			sql.command("select disasm,fileid,line from Asm where address=?1");
			sql << addr;
			if (sql.step()) {
				string tmpstr;
				int tmpid, tmpline;
				sql >> tmpstr >> tmpid >> tmpline;
				if (tmpstr == disasm && tmpid == fileid && tmpline == line)
					continue;
				ss.str(string());
				ss << disasm << '\t' << fileid << ' ' << line << " #mismatch# " << tmpstr << '\t' << tmpid << ' ' << tmpline;
				str = ss.str();
				throw runtime_error(str.c_str());
			}

			sql.command("insert into Asm values(?1,?2,?3,?4,?5)");
			sql << addr << len << disasm.c_str() << fileid << line;
			sql.step();

			continue;
		}

		//source line
		if (str.at(0) == ';') {
			ss >> str;

			if (str == "File") {
				ss >> ws;
				getline(ss, str);
				auto it = filelist.find(filepath(str).name_ext());
				if (it == filelist.end())
					throw runtime_error(str);
				fileid = it->second;
				continue;
			}
			int t = atoi(str.c_str());
			if (!t)
				continue;

			line = t;

			while (ss.good() && ss.get() != ':');
			ss >> ws;
			if (!ss.good())
					continue;

			if (!fileid)
				throw runtime_error("bad cod format");

			getline(ss, str);

			sql.command("select text from Source where fileid=?1 and line=?2");
			sql << fileid << line;
			if (sql.step()) {
				string tmp;
				sql >> tmp;
				if (str == tmp)
					continue;
				str += " #mismatch# ";
				str += tmp;
				throw runtime_error(str.c_str());
			}

			sql.command("insert into Source values(?1,?2,?3)");
			sql << fileid << line << str.c_str();
			sql.step();
			continue;

		}


		while (ss.good()) {
			string prev;
			prev.swap(str);
			ss >> str;


			//cin >> ws;
			if (str == "PROC") {

				while (ss.good() && ss.get() != ';');
				ss >> ws;
				if (!ss.good())
					str.swap(prev);
				else
					getline(ss, str, ',');

				sql.command("select address from Symbol where symbol=?1");
				sql << str.c_str();
				if (!sql.step())
					throw runtime_error(str.c_str());
				sql >> addr;
				continue;
			}
			if (str == "ENDP") {
				line = fileid = 0;
				addr = 0;
				break;
			}
		}
	}
}