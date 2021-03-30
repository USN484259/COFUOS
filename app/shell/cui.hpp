#pragma once
#include "uos.h"
#include "vector.hpp"

class CUI{
	word width;
	word height;
	dword const limit;

	char* const text_buffer;
	dword* const back_buffer;

	const word line_size;
	bool focus = false;
	bool redraw = false;

	dword color = 0x00FFFFFF;

	rectangle update_region = {0};

	dword tail = 0;
	dword edit = 0;

	struct line_info{
		dword head;
		word prev_len;
		//line_info(dword h,word l) : head(h), length(l) {}
	};
	UOS::vector<line_info> lines;

	struct {word x;word y;} cursor = {0};

	void fill(const rectangle& rect);
	//returns true if new_line
	void draw(char ch,dword pos);
	void scroll(dword pos);
public:
	CUI(word w,word h,dword* buffer,word linesize);

	bool is_newline(void) const{
		return cursor.x == 0;
	}
	void set_focus(bool f);
	void put(char ch,bool input = false);
	char back(void);
	dword get(char* dst,dword len);
	void clear(void);
	void render(word xoff,word yoff);

};

class label{
	word cursor = 0;
	word len = 0;
	const word limit;

	dword* const buffer;

	size_t size(void) const;
public:
	label(word len);
	~label(void);

	word length(void) const{
		return len;
	}
	void clear(void);
	bool put(char ch);
	void render(word xoff,word yoff);
};