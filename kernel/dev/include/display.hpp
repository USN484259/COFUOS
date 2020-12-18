#pragma once
#include "types.hpp"
#include "../../sync/include/spin_lock.hpp"

namespace UOS{
	struct Rect{
		int left;
		int top;
		int right;
		int bottom;
	};
	constexpr dword RGB(byte R,byte G,byte B){ return ((dword)R << 16) | ((dword)G << 8) | (dword)B; }
	class Display{
		spin_lock lock;

		dword volatile* vbe_buffer = nullptr;
		dword line_size = 0;
		word width = 0;
		word height = 0;

		dword volatile* at(int x,int y);
	public:
		Display(void);
		bool draw(const Rect& pos,const void* sor,size_t limit);
	};
	extern Display display;
}