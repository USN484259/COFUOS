#pragma once
#include "types.h"
#include "util.hpp"

class font{
public:
	struct fontchar{
		byte charcode;
		byte advance;
		byte width;
		byte height;
		signed char xoff;
		signed char yoff;
		word reserved;
		
		template<typename F>
		void render(F func) const{
			const unsigned line_size = UOS::align_up(width,8) >> 3;
			for (unsigned y = 0;y < height;++y){
				auto ptr = (byte const*)(this + 1) + y*line_size;
				byte data = 0;
				for (unsigned x = 0;x < width;++x){
					if (0 == (x & 7))
						data = *ptr++;
					func(*this,x,y,data & 1);
					data >>= 1;
				}
			}
		}
	};
	struct fontinfo{
		char name[0x0C];
		word line_height;
		word max_width;
	};
private:
	static_assert(sizeof(fontchar) == 8,"fontchar size mismatch");
	static_assert(sizeof(fontinfo) == 0x10,"fontinfo size mismatch");

	static constexpr byte printable_min = 0x20;
	static constexpr byte printable_max = 0x80;

	fontchar const* font_map[printable_max - printable_min] = {0};

public:
	font(void const* base,dword len);
	word line_height(void) const;
	word max_width(void) const;
	const char* name(void) const;
	const fontchar* get(byte ch) const;
};
extern font sys_fnt;