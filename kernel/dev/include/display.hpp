#pragma once
#include "types.h"
#include "util.hpp"
#include "interface/include/interface.h"

namespace UOS{
	//constexpr dword RGB(byte R,byte G,byte B,byte A = 0xFF){ return ((dword)A << 24) | ((dword)R << 16) | ((dword)G << 8) | (dword)B; }
	class screen_buffer{
		dword volatile* vbe_memory = nullptr;
		dword line_size = 0;
		dword width = 0;
		dword height = 0;
	public:
		screen_buffer(void);
		bool fill(const rectangle& rect,dword color);
		bool draw(const rectangle& rect,const dword* sor);
		inline dword get_width(void) const {
			return width;
		}
		inline dword get_height(void) const {
			return height;
		}
	};
	extern screen_buffer display;
}