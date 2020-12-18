#include "display.hpp"
#include "util.hpp"
#include "sysinfo.hpp"
#include "../memory/include/vm.hpp"
#include "bugcheck.hpp"
#include "assert.hpp"

using namespace UOS;

Display::Display(void){
	if (sysinfo->VBE_bpp != 32)
		BugCheck(not_implemented,sysinfo->VBE_bpp);
	auto aligned_pbase = align_down(sysinfo->VBE_addr,PAGE_SIZE);
/*  Possibly Compiler Bug
	Version: Microsoft (R) Optimizing Compiler Version 19.00.23918.0

	What's in .cod file:
; 14   :     if (sysinfo->VBE_width*sysinfo->VBE_bpp/8 > sysinfo->VBE_scanline)
00038	0f b7 0c 25 00
00 00 00	 movzx	 ecx, WORD PTR ds:-140737488352206  -> FFFF800000000C32 (sysinfo->VBE_scanline)
00040	48 89 6c 24 30	 mov	 QWORD PTR [rsp+48], rbp

	Disassembly result:
0F B7 0C 25 00 00 00 00		movzx   ecx, word ptr ds:0

	For reference, Another access to VBE_scanline in COFUOS.cpp@print_sysinfo:
48 B8 32 0C 00 00 00 80 FF FF	mov     rax, 0FFFF800000000C32h
	...
0F B7 10	movzx   eax, word ptr [rax]
*/
	dword scanline = sysinfo->VBE_scanline;
	if (sysinfo->VBE_width*sysinfo->VBE_bpp/8 > scanline)
		BugCheck(hardware_fault,scanline);
	if (scanline & 0x03)  //assume each line DWORD aligned
		BugCheck(not_implemented,scanline);
	
	auto size = scanline*sysinfo->VBE_height;
	auto page_count = (align_up(sysinfo->VBE_addr + size,PAGE_SIZE) - aligned_pbase) >> 12;
	auto vbase = vm.reserve(0,page_count);
	if (!vbase){
		BugCheck(bad_alloc,page_count);
	}
	auto res = vm.assign(vbase,aligned_pbase,page_count);
	if (!res){
		BugCheck(bad_alloc,vbase);
	}
	vbe_buffer = (dword volatile*)(vbase + (sysinfo->VBE_addr - aligned_pbase));
	line_size = scanline;
	width = sysinfo->VBE_width;
	height = sysinfo->VBE_height;
}

dword volatile* Display::at(int x,int y){
	if (x < 0 || y < 0)
		return nullptr;
	if (x >= width || y >= height)
		return nullptr;
	return vbe_buffer + ((y*line_size/4) + x);
}

bool Display::draw(const Rect& rect,const void* sor,size_t limit){
	if (!sor || !limit)
		return false;
	if (rect.left >= rect.right || rect.top >= rect.bottom)
		return false;
	if (rect.left >= width || rect.right <= 0)
		return false;
	if (rect.top >= height || rect.bottom <= 0)
		return false;
	auto w = rect.right - rect.left;
	auto h = rect.bottom - rect.top;
	if (limit < ((size_t)4*w*h))
		return false;
	auto line_ptr = (const dword*)sor;
	lock_guard<spin_lock> guard(lock);
	for (auto line = rect.top;line < rect.bottom;++line,line_ptr += w){
		if (line < 0)
			continue;
		if (line >= height)
			break;
		auto head = line_ptr;
		auto tail = head + w;
		if (rect.left < 0){
			head += (-rect.left);
		}
		if (rect.right > width){
			tail -= (rect.right - width);
		}
		assert(head < tail);
		auto dst = at(max(0,rect.left),line);
		assert(dst);
		while(head < tail){
			*dst++ = *head++;
		}
	}
	return true;
}