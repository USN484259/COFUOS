#pragma once
#include "types.hpp"
#include "util.hpp"

namespace UOS{
	constexpr dword RGB(byte R,byte G,byte B,byte A = 0xFF){ return ((dword)A << 24) | ((dword)R << 16) | ((dword)G << 8) | (dword)B; }
	class Display{
		dword volatile* vbe_memory = nullptr;
		dword line_size = 0;
		word width = 0;
		word height = 0;

		//dword volatile* at(int x,int y);
	public:
		Display(void);
		bool fill(const Rect& rect,dword color);
		bool draw(const Rect& rect,const dword* sor,dword tint = 0xFFFFFFFF);
		word get_width(void) const;
		word get_height(void) const;
	};
	class Font{
	public:
		struct font_info{
			byte charcode;
			byte advance;
			byte width;
			byte height;
			signed char xoff;
			signed char yoff;
		};
	private:
		struct fontchar{
			font_info const* info;
			dword const* buffer;
		};
		static constexpr byte printable_min = 0x20;
		static constexpr byte printable_max = 0x80;

		fontchar font_map[printable_max - printable_min] = {0};
	public:
		Font(void);
		byte line_height(void) const;
		//byte max_height(void) const;
		const font_info* get(char ch,const dword*& buffer);
	};
	//extern Font text_font;
	//extern Display display;
}