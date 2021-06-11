#include "font.hpp"
#include "uos.h"

using namespace UOS;

extern "C"
byte font_base[];

extern "C"
dword font_size;

extern "C"
font::fontinfo font_info;

font sys_fnt(font_base,font_size);

font::font(void const* base,dword len){
	auto ptr = (byte const*)base;
	while(len){
		assert(len > sizeof(fontchar));
		byte code = ptr[0];
		word width = ptr[2];
		word height = ptr[3];
		word line_size = align_up(width,8) >> 3;
		assert(code && width && height && line_size);
		word size = sizeof(fontchar) + line_size*height;
		assert(size <= len);
		if (code >= printable_min && code < printable_max){
			code -= printable_min;
			assert(font_map[code] == nullptr);
			font_map[code] = (fontchar const*)ptr;
		}
		ptr += size;
		len -= size;
	}
}
word font::line_height(void) const{
	return font_info.line_height;
}
word font::max_width(void) const{
	return font_info.max_width;
}
word font::table_size(void) const{
	return 4*font_info.max_width;
}
const char* font::name(void) const{
	return font_info.name;
}
const font::fontchar* font::get(byte ch) const{
	if (ch >= printable_min && ch < printable_max){
		return font_map[ch - printable_min];
	}
	return nullptr;
}
