#include <iostream>
#include <string>
#include <stdexcept>
#include <sstream>
#include "pipe.h"
#include "..\..\sysinfo.hpp"
#include "..\..\context.hpp"

using namespace std;
using namespace UOS;

SYSINFO info = { 0 };
SYSINFO* UOS::sysinfo = &info;

Pipe* pipe = nullptr;

extern "C"
void dispatch_exception(byte id, qword errcode, UOS::CONTEXT* context);

int main(int argc,char** argv) {
	try {
		if (argc < 2)
			throw runtime_error("too few arguments");

		pipe = new Pipe(argv[1]);
		UOS::CONTEXT context = { 0 };

		context.rip = 0xFFFF800002002000;

		while (true) {
			dispatch_exception(3, 0, &context);
			context.rip += 0x10;
		}

	}
	catch (exception& e) {
		cerr << "exception:\t" << e.what() << endl;
	}
	delete pipe;
	return 0;
}

extern "C"
byte serial_get(word) {
	return pipe->get();
}

extern "C"
void serial_put(word, byte val) {
	return pipe->put(val);
}

extern "C"
byte serial_peek(word port) {
	return pipe->peek() ? 1 : 0;
}