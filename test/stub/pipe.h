#pragma once
#include "windows.h"

class Pipe{
	HANDLE hPipe;
	DWORD avl;
	enum { NONE, READ, WRITE } dir;

public:
	Pipe(const char* name);
	~Pipe(void);

	byte get(void);
	void put(byte);
	bool peek(void);

	size_t read(void* dst, size_t lim);
	void write(const void* sor, size_t len);

};

