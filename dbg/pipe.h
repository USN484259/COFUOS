#pragma once
#include <exception>
#include <string>
#include <mutex>

#include "Windows.h"

class Pipe {
	HANDLE hPipe;
	DWORD avl;
	std::mutex m;
	static const DWORD interval;
	class bad_packet : public std::exception {};

	byte get(void);
	void put(byte);
	void wait(void);
	bool peek(void);

public:


	Pipe(const char* name);
	~Pipe(void);
	

	void read(std::string&);
	void write(const std::string&);

};