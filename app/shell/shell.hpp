#pragma once
#include "uos.h"
#include "cui.hpp"

class terminal{
	const word width;
	const word height;
	const word line_size;
	dword* const back_buffer;

	HANDLE lock = 0;
	HANDLE barrier = 0;
	HANDLE out_pipe = 0;
	HANDLE in_pipe = 0;
	HANDLE ps = 0;
	bool in_pipe_dirty = false;

	CUI cui;

	static void thread_reader(void*,void*);
	static void thread_monitor(void*,void*);
	void dispatch(void);
	void print(const char* str);
	void show_shell(void);
public:
	terminal(word w,word h,const char* = nullptr);
	void begin_paint(void);
	void end_paint(void);
	void put(char ch);
	//void blink(void);
};
