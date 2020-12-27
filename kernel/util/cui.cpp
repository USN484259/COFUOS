#include "cui.hpp"
#include "exception/include/kdb.hpp"

using namespace UOS;

CUI::CUI(const Rect& rect) : window(rect){
	assert(rect.right > rect.left && rect.bottom > rect.top);
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;
	buffer = new dword[width*height];
	clear();
}

CUI::~CUI(void){
	delete[] buffer;
}

void CUI::set_color(dword clr){
	color = clr | 0xFF000000;
}

void CUI::print(const char* s){
	while(*s){
		put(*s++);
	}
}

void CUI::put(char ch){
	if (ch == '\n'){
		line_feed();
		return;
	}
	if (ch == '\t'){
		auto cnt = str.size();
		cnt = align_up(cnt + 1,tab_size) - cnt;
		while (cnt--)
		{
			imp_put(' ');
		}
		return;
	}
	imp_put(ch);
}

void CUI::clear(void){
	str.clear();
	fill(Rect{0,0,width,height});
	cursor = {0,0};
}

void CUI::line_feed(void){
	auto line_height = text_font.line_height();
	cursor.x = 0;
	cursor.y += line_height;
	if (cursor.y + line_height > height){
		//scroll view down
		scroll(cursor.y + line_height - height);
		cursor.y = height - line_height;
	}
	str.clear();
}

char CUI::back(void){
	if (str.empty())
		return 0;
	char ch = str.back();
	str.pop_back();
	const dword* font = nullptr;
	auto info = text_font.get(ch,font);
	assert(info && font);

	cursor.x -= info->advance;
	assert(cursor.x >= 0);
	Rect font_rect {\
		cursor.x + info->xoff,\
		cursor.y + info->yoff,\
		cursor.x + info->xoff + info->width,\
		cursor.y + info->yoff + info->height };
	fill(font_rect);

	return ch;
}

void CUI::imp_put(char ch){
	const dword* font = nullptr;
	auto info = text_font.get(ch,font);
	if (!info)
		return;
	assert(font);
	if (cursor.x + info->width + info->xoff >= width)
		line_feed();
	Rect font_rect {\
		cursor.x + info->xoff,\
		cursor.y + info->yoff,\
		cursor.x + info->xoff + info->width,\
		cursor.y + info->yoff + info->height };
	draw(font_rect,font);
	cursor.x += info->advance;
	str.push_back(ch);
}

inline Rect operator+(const Rect& r,const Point& p){
	return Rect{\
		r.left + p.x, \
		r.top + p.y, \
		r.right + p.x, \
		r.bottom + p.y \
	};
}

void CUI::fill(const Rect& rect){
	//assert(rect.left >= 0 && rect.right <= width);
	//assert(rect.top >= 0 && rect.bottom <= height);
	assert(rect.right > rect.left && rect.bottom > rect.top);

	for (auto line = max(0,rect.top);line < min((int)height,rect.bottom);++line){
		auto ptr = buffer + width*line;
		for (auto i = max(0,rect.left);i < min((int)width,rect.right);++i)
			ptr[i] = RGB(0,0,0);
	}
	display.fill(rect + Point{window.left,window.top},RGB(0,0,0));
}

void CUI::draw(const Rect& rect,const dword* sor){
	//assert(rect.left >= 0 && rect.right <= width);
	//assert(rect.top >= 0 && rect.bottom <= height);
	assert(rect.right > rect.left && rect.bottom > rect.top);
	
	auto ptr = sor;
	if (rect.top < 0)
		ptr += (rect.right - rect.left)*(-rect.top);
	for (auto line = max(0,rect.top);line < min((int)height,rect.bottom);++line){
		auto dst = buffer + width*line;
		for (auto i = max(0,rect.left);i < min((int)width,rect.right);++i){
			if ((dst[i] & 0x00FFFFFF) == 0)
				dst[i] = (ptr[i - rect.left] & color) | 0xFF000000;
		}
		ptr += (rect.right - rect.left);
	}
	display.draw(rect + Point{window.left,window.top},sor,color);
}

void CUI::scroll(word dy){
	for (auto line = 0;line + dy< height;++line){
		memcpy(buffer + width*line,buffer + width*(line + dy),sizeof(dword)*width);
	}
	display.draw(Rect{window.left,window.top,window.right,window.bottom - dy},buffer);
	fill(Rect{0,height - dy,width,height});
}