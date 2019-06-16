#include <iostream>
#include <string>
#include <stdexcept>
#include <sstream>
#include "pipe.h"
#include "sim.h"
#include "..\..\types.hpp"
#include "..\..\context.hpp"

using namespace std;

Pipe* pipe = nullptr;


extern "C"
void dispatch_exception(byte id, qword errcode, UOS::CONTEXT* context);

int main(int argc,char** argv) {
	try {
		if (argc < 2)
			throw runtime_error("too few arguments");

		pipe = new Pipe(argv[1]);
		UOS::CONTEXT context = { 0 };


		context.rip = 0xFFFF800002001000;
		context.rsp = 0xFFFF800001FFE000;

		Sim sim;
		while (sim) {
			dispatch_exception(3, 0, &context);
			context.rip = sim.dr_match(context.rip + (rand() & 0xFF), context.rip);
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
