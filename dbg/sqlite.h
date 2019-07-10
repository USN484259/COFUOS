#pragma once
#include "sqlite3.h"
#include <string>
#include <unordered_set>
#include <stdexcept>

class Sqlite {
	sqlite3* con;
	sqlite3_stmt* cmd;
	bool in_transcation;
	enum { SET, GET,FIN }status;
	int index;
	std::unordered_set<std::string> buffer;
public:
	class SQL_exception : public std::runtime_error {
	public:
		SQL_exception(sqlite3*);
		SQL_exception(int);
		SQL_exception(const char*);
	};



	Sqlite(const char*,bool = false);
	~Sqlite(void);

	void command(const char*);
	Sqlite& operator<<(int);
	Sqlite& operator<<(unsigned __int64);
	Sqlite& operator<<(const char*);
	Sqlite& operator<<(const std::string&);
	bool step(void);

	//type peek_type(void);
	//type peek_type(int);

	Sqlite& operator>>(int&);
	Sqlite& operator>>(unsigned __int64&);
	Sqlite& operator>>(std::string&);

	void transcation(void);
	void commit(void);
	void rollback(void);
};