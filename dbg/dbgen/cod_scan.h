#pragma once
//#include <fstream>
#include <map>
#include "..\sqlite.h"

class cod_scanner {
	Sqlite& sql;
	const std::map<std::string,int> filelist;
public:
	cod_scanner(Sqlite&,const std::map<std::string,int>&);

	void scan_cod(const std::string&);

	void scan_lst(const std::string&,int);

};
