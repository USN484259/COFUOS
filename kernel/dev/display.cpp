#include "display.hpp"
#include "util.hpp"
#include "sysinfo.hpp"
#include "memory/include/vm.hpp"
#include "string.hpp"
#include "assert.hpp"

using namespace UOS;

extern "C" const byte compact_font;

Display::Display(void){
	if (sysinfo->VBE_bpp != 32)
		bugcheck("bpp = %d not implemented",sysinfo->VBE_bpp);
	auto aligned_pbase = align_down(sysinfo->VBE_addr,PAGE_SIZE);

	line_size = sysinfo->VBE_scanline;
	width = sysinfo->VBE_width;
	height = sysinfo->VBE_height;

	if (width*sysinfo->VBE_bpp/8 > line_size)
		bugcheck("invalid line size %d",line_size);
	if (line_size & 0x03)  //assume each line DWORD aligned
		bugcheck("line size %d not DWORD aligned",line_size);
	
	auto size = line_size*height;
	auto page_count = (align_up(sysinfo->VBE_addr + size,PAGE_SIZE) - aligned_pbase) >> 12;
	auto vbase = vm.reserve(0,page_count);
	if (!vbase){
		bugcheck("vm.reserve failed with 0x%x pages",page_count);
	}
	auto res = vm.assign(vbase,aligned_pbase,page_count);
	if (!res){
		bugcheck("vm.assign failed @ %p",aligned_pbase);
	}
	vbe_memory = (dword volatile*)(vbase + (sysinfo->VBE_addr - aligned_pbase));

	dbgprint("Video memory mapped to %p",vbe_memory);
}

word Display::get_width(void) const{
	return width;
}

word Display::get_height(void) const{
	return height;
}

bool Display::fill(const Rect& rect,dword color){
	if (rect.left >= rect.right || rect.top >= rect.bottom)
		return false;
	if (rect.left >= width || rect.right <= 0)
		return false;
	if (rect.top >= height || rect.bottom <= 0)
		return false;

	for (auto line = max(0,rect.top);line < min((int)height,rect.bottom);++line){
		auto dst = vbe_memory + line*(line_size/4);

		for (auto i = max(0,rect.left);i < min((int)width,rect.right);++i){
			//TODO apply alpha
			dst[i] = color & 0x00FFFFFF;
		}
	}
	return true;
}

bool Display::draw(const Rect& rect,const dword* sor,dword tint){
	if (!sor)
		return false;
	if (rect.left >= rect.right || rect.top >= rect.bottom)
		return false;
	if (rect.left >= width || rect.right <= 0)
		return false;
	if (rect.top >= height || rect.bottom <= 0)
		return false;

	auto length = rect.right - rect.left;
	if (rect.top < 0){
		sor += length*(-rect.top);
	}
	for (auto line = max(0,rect.top);line < min((int)height,rect.bottom);++line,sor += length){
		auto line_ptr = sor;

		auto dst = vbe_memory + line*(line_size/4);
		if (rect.left < 0){
			line_ptr += (-rect.left);
		}
		for (auto i = max(0,rect.left);i < min((int)width,rect.right);++i){
			dword val = tint & *line_ptr++;
			//TODO apply alpha
			if (val & 0xFF000000){	//not transparent
				dst[i] = val & 0x00FFFFFF;
			}
		}
	}
	return true;
}

Font::Font(void){
	const byte* cur = &compact_font;
	word length = *(const word*)(cur+2);
	cur += 4;
	while(length){
		assert(length > 6);
		byte charcode = *cur;
		word width = *(cur+2);
		word height = *(cur+3);
		word line_size = align_up(width,8) >> 3;
		assert(charcode && width && height);
		word this_size = 6 + line_size*height;
		auto next = cur + this_size;
		assert(length >= this_size);
		length -= this_size;
		if (charcode >= printable_min && charcode < printable_max){
			auto& cur_font = font_map[charcode - printable_min];
			assert(cur_font.info == nullptr && cur_font.buffer == nullptr);
			cur_font.info = (const font_info*)cur;
			cur += 6;
			auto buffer = new dword[width*height];
			cur_font.buffer = buffer;
			for (word h = 0;h < height;++h){
				byte data;
				for (word w = 0;w < width;++w){
					if (0 == (w & 7))
						data = *cur++;
					dword value = (data & 1) ? 0xFFFFFFFF : 0;
					data >>= 1;
					buffer[h*width + w] = value;
				}
			}
		}
		cur = next;
	}
}

byte Font::line_height(void) const{
	return compact_font;
}


const Font::font_info* Font::get(char ch,const dword*& buffer){
	if (ch >= printable_min && ch < printable_max){
		auto& cur_font = font_map[ch - printable_min];
		assert(cur_font.info && cur_font.buffer);
		buffer = cur_font.buffer;
		return cur_font.info;
	}
	return nullptr;
}