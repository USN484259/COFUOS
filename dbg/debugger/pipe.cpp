#include "pipe.h"
#include <stdexcept>
#include <sstream>
#include <thread>
using namespace std;
using namespace UOS;


Pipe::Pipe(const char* name) : avl(0),hPipe(CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) {
	if (hPipe == INVALID_HANDLE_VALUE)
		throw runtime_error("CreateFile");
}

Pipe::~Pipe(void) {
	if (hPipe != INVALID_HANDLE_VALUE)
		CloseHandle(hPipe);
}

byte Pipe::get(void) {
	while (!peek()) {
			Sleep(100);
	}
	DWORD cnt;
	byte buf;
	if (!ReadFile(hPipe, &buf, 1, &cnt, NULL) || cnt != 1)
		throw runtime_error("ReadFile");

	avl--;
	return buf;


}


bool Pipe::peek(void) {
	if (!avl) {

		if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &avl, NULL))
			throw runtime_error("PeekNamedPipe");
	}
	return avl ? true : false;
}


void Pipe::wait(void) {
	while (!peek())
		this_thread::yield();
}


void Pipe::read(string& buf) {
	while (true) {
		wait();
		while (get() != '$');
		byte chk_req = get();
		byte chk_rel = '$';
		word len = get();
		chk_rel += (byte)len;
		byte cur = get();
		chk_rel += cur;
		if (cur >= 4)	//should less than 1KB
			continue;
		len |= (word)cur << 8;
		buf.clear();
		while (len--) {
			cur = get();
			chk_rel += cur;
			buf += cur;
		}
		if (chk_rel == chk_req)
			return;

	}
}

void Pipe::write(const string& sor) {
	string str("$\0\0\0",4);
	byte checksum = '$';
	for (auto it = sor.cbegin(); it != sor.cend(); ++it) {
		str += *it;
		checksum += *it;
	}
	checksum += (str.at(2) = (byte)sor.size());
	checksum += (str.at(3) = (byte)(sor.size() >> 8));
	str.at(1) = checksum;

	DWORD len;
	lock_guard<mutex> lck(m);
	if (!WriteFile(hPipe, str.c_str(), str.size(), &len, NULL) || len != str.size())
		throw runtime_error("WriteFile");

	return;
}

