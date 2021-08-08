#pragma once
#include "uos.h"
#include "vector.hpp"

class text_buffer{
	char* const buffer;
	dword const limit;

	dword tail;
	dword edit;
public:
	class iterator{
		friend class text_buffer;
		const text_buffer* owner = nullptr;
		dword pos;
		iterator(const text_buffer* o,dword p);
	public:
		iterator(void) = default;
		iterator(const iterator&) = default;
		iterator& operator=(const iterator&);
		bool operator==(const iterator&) const;
		bool operator!=(const iterator&) const;
		char operator*(void) const;
		iterator& operator++(void);
		iterator& operator--(void);
	};
public:
	text_buffer(dword lim);
	~text_buffer(void);

	void clear(void);
	bool has_edit(void) const{
		return tail != edit;
	}
	iterator end(bool input) const;
	char at(iterator it) const;
	iterator push(char ch,bool input);
	char pop(void);
	iterator commit(void);
};

class screen_buffer{
	dword* const buffer;
	const word line_size;
	const word line_count;
	const word window_size;
	word head_line = 0;
	bool focus = false;

	void imp_render(word xoff,word yoff,const rectangle& rect,dword* line_base);
public:
	screen_buffer(dword* ptr,word ls,word lc,word ws);
	bool has_focus(void) const{
		return focus;
	}
	bool set_focus(bool f);
	bool clear(void);
	bool scroll(word dy);
	bool pixel(word x,word y,dword color);
	bool fill(const rectangle& rect);
	bool render(word xoff,word yoff,const rectangle& rect);
};

class CUI{
	struct line_info{
		text_buffer::iterator head;
		word prev_len;
	};

	const word width;
	const word height;

	text_buffer text;
	screen_buffer screen;

	struct {word x;word y;} cursor = {0};
	dword color = 0x00FFFFFF;
	rectangle update_region = {0};

	UOS::vector<line_info> lines;
	bool redraw = false;

	void fill(const rectangle& rect);
	void draw(char ch,text_buffer::iterator pos);
	void scroll(text_buffer::iterator pos);
public:
	CUI(word w,word h,dword* buffer,word linesize,word linecount);

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
	bool put(const char* str);
	void render(word xoff,word yoff);
};