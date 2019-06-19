#include "sqlite.h"

using namespace std;

Sqlite::SQL_exception::SQL_exception(sqlite3* sql) : runtime_error(sqlite3_errmsg(sql)) {}
Sqlite::SQL_exception::SQL_exception(int code) : runtime_error(sqlite3_errstr(code)) {}
Sqlite::SQL_exception::SQL_exception(const char* msg) : runtime_error(msg) {}

Sqlite::Sqlite(const char* filename, bool read_only) : con(nullptr), cmd(nullptr), in_transcation(false) {
	if (SQLITE_OK != sqlite3_open_v2(filename, &con, read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE, nullptr))
		throw SQL_exception(con);
}

Sqlite::~Sqlite(void) {
		//if (SQLITE_OK != sqlite3_finalize(cmd))
		//	throw SQL_exception(con);
		//if (SQLITE_OK != sqlite3_close(con))
		//	throw SQL_exception(con);
	try {
		if (in_transcation)
			rollback();
	}
	catch (SQL_exception&) {}
	sqlite3_finalize(cmd);
	sqlite3_close(con);
}

void Sqlite::command(const char* sql) {
	if (SQLITE_OK != sqlite3_finalize(cmd))
		throw SQL_exception(con);
	if (SQLITE_OK != sqlite3_prepare_v2(con, sql, -1, &cmd, nullptr))
		throw SQL_exception(con);
	buffer.clear();
	status = SET;
	index = 1;
}

Sqlite& Sqlite::operator<<(int val) {
	if (status != SET)
		throw SQL_exception("<< not in SET mode");
	int res = sqlite3_bind_int(cmd, index++, val);
	if (res!=SQLITE_OK)
		throw SQL_exception(res);
	return *this;
}

Sqlite& Sqlite::operator<<(unsigned __int64 val) {
	if (status != SET)
		throw SQL_exception("<< not in SET mode");
	int res = sqlite3_bind_int64(cmd, index++, val);
	if (res!=SQLITE_OK)
		throw SQL_exception(res);
	return *this;
}

Sqlite& Sqlite::operator<<(const char* str) {
	if (status != SET)
		throw SQL_exception("<< not in SET mode");
	
	int res = sqlite3_bind_text(cmd, index++, (*buffer.insert(string(str)).first).c_str(), -1, nullptr);
	if (res!=SQLITE_OK)
		throw SQL_exception(res);
	return *this;
}

Sqlite& Sqlite::operator<<(const string& str) {
	return operator<< (str.c_str());
}

bool Sqlite::step(void) {
	if (status == FIN)
		throw SQL_exception("step in FIN mode");
	int res = sqlite3_step(cmd);
	switch (res) {
	case SQLITE_DONE:
		status = FIN;
		return false;
	case SQLITE_ROW:
		status = GET;
		index = 0;
		return true;
	}
	throw SQL_exception(res);
}

Sqlite& Sqlite::operator >> (int& val) {
	if (status != GET)
		throw SQL_exception(">> in SET mode");
	if (SQLITE_INTEGER != sqlite3_column_type(cmd, index))
		throw SQL_exception("type mismatch");
	val = sqlite3_column_int(cmd, index++);

	return *this;
}

Sqlite& Sqlite::operator >> (unsigned __int64& val) {
	if (status != GET)
		throw SQL_exception(">> in SET mode");
	if (SQLITE_INTEGER != sqlite3_column_type(cmd, index))
		throw SQL_exception("type mismatch");
	val = sqlite3_column_int64(cmd, index++);

	return *this;
}

Sqlite& Sqlite::operator >> (string& str) {
	if (status != GET)
		throw SQL_exception(">> in SET mode");
	if (SQLITE3_TEXT != sqlite3_column_type(cmd, index))
		throw SQL_exception("type mismatch");
	str.assign((const char*)sqlite3_column_text(cmd, index++));

	return *this;
}

void Sqlite::transcation(void) {
	if (in_transcation)
		throw SQL_exception("nested transcation");
	char* msg = nullptr;
	sqlite3_exec(con, "begin", nullptr, nullptr, &msg);
	if (msg)
		throw SQL_exception(msg);
	in_transcation = true;
}

void Sqlite::commit(void) {
	if (!in_transcation)
		throw SQL_exception("no transcation");
	char* msg = nullptr;
	sqlite3_exec(con, "commit", nullptr, nullptr, &msg);
	if (msg)
		throw SQL_exception(msg);
	in_transcation = false;
}
void Sqlite::rollback(void) {
	if (!in_transcation)
		throw SQL_exception("no transcation");
	char* msg = nullptr;
	sqlite3_exec(con, "rollback", nullptr, nullptr, &msg);
	if (msg)
		throw SQL_exception(msg);
	in_transcation = false;
}