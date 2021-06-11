#pragma once
#include "uos.h"
#include "cui.hpp"

struct back_buffer{
	const word width;
	const word height;
	const word line_size;
	const word line_count;
	dword* const buffer;
	back_buffer(word w,word h);
	~back_buffer(void);
};

class work_dir{
	char* buffer = nullptr;
	dword count = 0;
	dword limit = 0;
	void update(void);
public:
	work_dir(void);
	bool set(const char* str,dword len);
	const char* get(void) const{
		return buffer;
	}
	dword size(void) const{
		return count;
	}
};

class terminal{
	back_buffer& buffer;

	HANDLE lock = 0;
	HANDLE barrier = 0;
	HANDLE out_pipe = 0;
	HANDLE in_pipe = 0;
	HANDLE ps = 0;
	CUI cui;
	bool in_pipe_dirty = false;

	work_dir wd;

	static void thread_reader(void*,void*);
	static void thread_monitor(void*,void*);
	void dispatch(void);
	void print(const char* str);
	void show_shell(void);
	void show_help(void);
public:
	terminal(back_buffer& buf,const char* = nullptr);
	void begin_paint(void);
	void end_paint(void);
	void put(char ch);
	void set_focus(bool f){
		cui.set_focus(f);
	}
	//void blink(void);
};
