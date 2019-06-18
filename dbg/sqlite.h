#pragma once
#include "sqlite3.h"
#include <string>
#include <stdexcept>

class Sqlite {
	sqlite3* con;
	sqlite3_stmt* cmd;
	const bool read_only;
	bool in_transcation;
	enum { SET, GET,FIN }status;
	int index;
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

	Sqlite& operator>>(int&);
	Sqlite& operator>>(unsigned __int64&);
	Sqlite& operator>>(std::string&);

	void transcation(void);
	void commit(void);
	void rollback(void);
};