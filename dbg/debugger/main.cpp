#include <iostream>
#include <sstream>
#include "debugger.h"

using namespace std;

Debugger* dbg = nullptr;


BOOL __stdcall OnCtrlC(DWORD reason) {
	if (reason != CTRL_C_EVENT)
		return FALSE;
	dbg->pause();
	//cout << "\nbreak sent" << endl;
	return TRUE;
}

int main(int argc, char** argv) {


	const char* pipe = nullptr;
	const char* database = nullptr;
	const char* editor = "notepad.exe";
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-')
			continue;
		switch (~0x20 & argv[i][1]) {
		case 'P':
			pipe = argv[i] + 2;
			break;
		case 'D':
			database = argv[i] + 2;
			break;
		case 'E':
			editor = argv[i] + 2;
			break;
		default:
			continue;
		}
	}

	try {
		if (pipe && database && editor)
			dbg = new Debugger(pipe, database, editor);
		else {
			cout << "dbg -p(pipe) -d(database) [-e(editor)]" << endl;
			throw runtime_error("too few arguments");
		}
		if (!SetConsoleCtrlHandler(OnCtrlC, TRUE))
			throw runtime_error("SetConsoleCtrlHandler");

		dbg->run();
	}
	catch (exception& e) {
		cerr << typeid(e).name() << " : " << e.what() << endl;
	}
	delete dbg;

	return 0;
}
