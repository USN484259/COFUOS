#include "display.hpp"
#include "util.hpp"
#include "sysinfo.hpp"
#include "memory/include/vm.hpp"
#include "string.hpp"
#include "assert.hpp"

using namespace UOS;

video_memory::video_memory(void){
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
	features.set(decltype(features)::SCR);
	dbgprint("Video memory mapped to %p",vbe_memory);
}

bool video_memory::fill(const rectangle& rect,dword color){
	if (rect.left >= rect.right || rect.top >= rect.bottom)
		return false;
	if (rect.right > width || rect.bottom > height)
		return false;
	color &= 0x00FFFFFF;

	auto dst = vbe_memory + line_size/4*rect.top + rect.left;
	for (auto line = 0;line < (rect.bottom - rect.top);++line){
		for (auto i = 0;i < (rect.right - rect.left);++i){
			dst[i] = color;
		}
		dst += line_size/4;
	}
	return true;
}

bool video_memory::draw(const rectangle& rect,const dword* sor,word advance){
	if (!sor)
		return false;
	if (rect.left >= rect.right || rect.top >= rect.bottom)
		return false;
	if (rect.right > width || rect.bottom > height)
		return false;
	if (rect.right - rect.left > advance)
		return false;

	auto dst = vbe_memory + line_size/4*rect.top + rect.left;
	for (auto line = 0;line < (rect.bottom - rect.top);++line){
		for (auto i = 0;i < (rect.right - rect.left);++i){
			dst[i] = 0x00FFFFFF & sor[i];
		}
		dst += line_size/4;
		sor += advance;
	}
	return true;
}
