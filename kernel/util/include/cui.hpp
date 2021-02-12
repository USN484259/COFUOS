#pragma once
#include "types.hpp"
#include "util.hpp"
#include "dev/include/display.hpp"

#define THROW(x) bugcheck(unhandled_exception,x)
#include "string.hpp"
#include "linked_list.hpp"

namespace UOS{
	class CUI{
		Rect window;
		Point cursor;
		dword color = RGB(0xFF,0xFF,0xFF);
		word width = 0;
		word height = 0;
		dword* buffer = nullptr;
		string str;
		static constexpr size_t tab_size = 4;
		
		void line_feed(void);
		void imp_put(char ch);
		void fill(const Rect&);
		void draw(const Rect&,const dword*);
		void scroll(word);
	public:
		CUI(const Rect&);
		~CUI(void);
		void set_color(dword);
		void clear();
		void put(char);
		char back(void);
		void print(const char*);
	};
}