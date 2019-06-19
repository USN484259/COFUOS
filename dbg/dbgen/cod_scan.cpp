#include "cod_scan.h"
#include "fs_util.h"

#ifdef _DEBUG
#include <iostream>
#endif

#include <fstream>
#include <string>
#include <sstream>


using namespace std;

cod_scanner::cod_scanner(Sqlite& q,const map<string,int>& lst) : sql(q),filelist(lst) {}

void cod_scanner::scan_lst(const string& filename,int fileid) {
	ifstream sor(filename);
	if (!sor.is_open())
		throw runtime_error(filename);
	int line = 0;
	unsigned __int64 base_addr = 0;
	int base_off = -1;
	int length = 0;
#ifdef _DEBUG
	cout << "\t$" << filename << endl;
#endif

	while (sor.good()) {
		string str;
		getline(sor, str);
#ifdef _DEBUG
		cout << str << endl;
#endif

		stringstream ss(str);
		ss >> dec >> line >> str;
		if (str.empty())
			continue;

		auto it = str.rbegin();
		if (':' == *it) {
			if (str.at(0) != '.') {
				*it = 0;
				if (length) {
					sql.command("update Symbol set length=?1 where address=?2");
					sql << length << base_addr;
					sql.step();
				}
//#error "TODO alter Symbol size to actual size"
				sql.command("select address from Symbol where symbol=?1");
				sql << str;
				if (!sql.step())
					base_addr = 0;
				else
					sql >> base_addr;
				base_off = -1;
				length = 0;

			}
			continue;
		}
		if ('0' != str.at(0))
			continue;

		if (!base_addr)
			continue;

		int off = stoi(str, 0, 16);
		if (base_off == -1)
			base_off = off;

		ss >> str >> ws;	//op-code

		if (!ss.good())
			continue;

		int len = 0;
		do {
			bool ignore = false;
			for (auto it = str.cbegin(); it != str.cend(); ++it) {
				switch (*it) {
				case '<':
					ignore = true;
					continue;
				case '>':
					ignore = false;
					continue;
				}
				if (!ignore && isxdigit(*it))
					len++;
			}
			if (*str.crbegin() != '-')
				break;

			getline(sor, str);
#ifdef _DEBUG
			cout << str << endl;
#endif

			stringstream tmpss(str);
			tmpss >> str;
			tmpss >> str;
			tmpss >> str;
			if (!tmpss.good())
				throw runtime_error("bad lst formst");
			tmpss >> str;
		} while (true);

		if (len%2)
			throw runtime_error("bad lst formst");
		len /= 2;

		length = off - base_off + len;
		getline(ss, str);

		sql.command("insert into Asm values(?1,?2,?3,?4,?5)");
		sql << base_addr + off - base_off << len << str << fileid << line;
		sql.step();

	}
}

//#error "TODO crash with multi-line op-code -> see lock.cod line 121"
void cod_scanner::scan_cod(const string& filename) {
	ifstream sor(filename);
	if (!sor.is_open())
		throw runtime_error(filename);
	int line = 0;
	int fileid = 0;
	unsigned __int64 addr = 0;
#ifdef _DEBUG
	cout << "\t$" << filename << endl;
#endif

	while (sor.good()) {
		string str;
		getline(sor, str);
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
#ifdef _DEBUG
					cout << str << endl;
#endif

					ss.str(str);
					ss.clear();
				}
				else
					break;
			} while (sor.good());

			addr += len;

#error "function overload problem -> see heap.cod line 973 and line 1014"

			sql.command("select disasm,fileid,line from Asm where address=?1");
			sql << addr;
			if (sql.step()) {
				string tmpstr;
				int tmpid, tmpline;
				sql >> tmpstr >> tmpid >> tmpline;
				if (tmpstr == disasm && tmpid == fileid && tmpline == line)
					continue;
				ss.str(string());
				ss.clear();
				ss << disasm << '\t' << fileid << ' ' << line << " #mismatch# " << tmpstr << '\t' << tmpid << ' ' << tmpline;
				str = ss.str();
				throw runtime_error(str.c_str());
			}

			sql.command("insert into Asm values(?1,?2,?3,?4,?5)");
			sql << addr << len << disasm << fileid << line;
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
			sql << fileid << line << str;
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
				sql << str;
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