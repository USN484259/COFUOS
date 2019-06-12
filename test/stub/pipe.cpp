#include "pipe.h"
#include <iostream>
#include <stdexcept>


using namespace std;

Pipe::Pipe(const char* name) : dir(NONE),avl(0), hPipe(CreateNamedPipe(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 0x200, 0x200, 0, NULL)) {
	if (hPipe == INVALID_HANDLE_VALUE || !ConnectNamedPipe(hPipe, NULL))
		throw runtime_error("CreateNamedPipe");
	
}


Pipe::~Pipe()
{
	if (hPipe != INVALID_HANDLE_VALUE)
		CloseHandle(hPipe);
}

byte Pipe::get(void) {
	DWORD len;
	byte val;
	
	if (!ReadFile(hPipe, &val, 1, &len, NULL) || len != 1)
		throw runtime_error("ReadFile");

	if (avl)
		avl--;

	if (dir != READ) {
		dir = READ;
		cout << endl << "read:\t";
	}
	cout << hex << (DWORD)val << ' ';

	return val;
}

void Pipe::put(byte val) {
	DWORD len;
	if (!WriteFile(hPipe, &val, 1, &len, NULL) || len != 1)
		throw runtime_error("WriteFile");

	if (dir != WRITE) {
		dir = WRITE;
		cout << endl << "write:\t";
	}
	cout << hex << (DWORD)val << ' ';

}

bool Pipe::peek(void) {
	if (0 == avl) {
		if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &avl, NULL))
			throw runtime_error("PeekNamedPipe");
	}
	return avl ? true : false;
}

size_t Pipe::read(void* dst, size_t lim) {

	while (!peek())
		Sleep(50);

	DWORD len;
	if (!ReadFile(hPipe, dst, lim, &len, NULL))
		throw runtime_error("ReadFile");

	if (len < avl)
		avl -= len;
	else
		avl = 0;

	return len;

}


void Pipe::write(const void* sor, size_t len) {
	DWORD tmp;
	if (!WriteFile(hPipe, sor, len, &tmp, NULL) || tmp!=len)
		throw runtime_error("WriteFile");


}

