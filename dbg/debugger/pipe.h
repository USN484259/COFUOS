#pragma once
#include <exception>
#include <string>
#include <mutex>
//#include "event.h"
#include "..\..\types.hpp"
#include "Windows.h"

class Pipe {
	HANDLE hPipe;
	DWORD avl;
	std::mutex m;
	//Event w;
	byte get(void);
	//void put(byte);
	void wait(void);
	bool peek(void);

public:


	Pipe(const char* name);
	~Pipe(void);
	

	void read(std::string&);
	void write(const std::string&);
	//void wake(void);
};