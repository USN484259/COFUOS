#include "pipe.h"
#include <stdexcept>
#include <sstream>
using namespace std;

const DWORD Pipe::interval = 5000;


Pipe::Pipe(const char* name) : avl(0),hPipe(CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) {
	if (hPipe == INVALID_HANDLE_VALUE)
		throw runtime_error("CreateFile");
}

Pipe::~Pipe(void) {
	if (hPipe != INVALID_HANDLE_VALUE)
		CloseHandle(hPipe);
}

byte Pipe::get(void) {
	DWORD cnt = interval / 100;
	while (!peek()) {
		if (cnt--)
			Sleep(100);
		else
			throw bad_packet();
	}

	byte buf;
	if (!ReadFile(hPipe, &buf, 1, &cnt, NULL) || cnt != 1)
		throw runtime_error("ReadFile");

	avl--;
	return buf;


}

void Pipe::put(byte c) {
	DWORD size;
	if (!WriteFile(hPipe, &c, 1, &size, NULL) || size != 1)
		throw runtime_error("WriteFile");
}

void Pipe::wait(void) {
	while (!peek())
		Sleep(500);
}

bool Pipe::peek(void) {
	if (!avl) {

		if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &avl, NULL))
			throw runtime_error("PeekNamedPipe");
	}
	return avl ? true : false;
}

void Pipe::read(string& buf) {
	wait();
	lock_guard<mutex> lck(m);
	while (true) {
		try {
			while (get() != '$');
			buf.clear();
			BYTE checksum = 0;
			while (true) {
				byte c = get();
				while (c == '#') {
					byte t = get();
					if (!peek()) {
						if (t == checksum) {
							put('+');
							return;
						}
						else
							throw bad_packet();
					}
					checksum += c;
					buf += (char)c;
					c = t;
				}
				checksum += c;
				buf += (char)c;
			}
		}
		catch (bad_packet&) {
			put('-');
		}
		wait();

	}
}

void Pipe::write(const string& sor) {
	string str("$");
	byte checksum = 0;
	for (auto it = sor.cbegin(); it != sor.cend(); ++it) {
		str += *it;
		checksum += *it;
	}
	str += '#';
	str += (char)checksum;
	lock_guard<mutex> lck(m);
	do {
		DWORD len;
		if (!WriteFile(hPipe, str.c_str(), str.size(), &len, NULL) || len != str.size())
			throw runtime_error("WriteFile");
		wait();
	} while (get() != '+');

}

