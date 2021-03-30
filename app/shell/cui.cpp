#include "cui.hpp"
#include "font.hpp"

using namespace UOS;

inline void assign(rectangle& dst,const rectangle& sor){
	dst.left = sor.left;
	dst.top = sor.top;
	dst.right = sor.right;
	dst.bottom = sor.bottom;
}

inline void merge(rectangle& dst,const rectangle& sor){
	if (sor.right <= sor.left || sor.bottom <= sor.top)
		return;
	if (dst.right <= dst.left || dst.bottom <= dst.top){
		assign(dst,sor);
		return;
	}
	dst.left = min(dst.left,sor.left);
	dst.top = min(dst.top,sor.top);
	dst.right = max(dst.right,sor.right);
	dst.bottom = max(dst.bottom,sor.bottom);
}

CUI::CUI(word w,word h,dword* buffer,word linesize) : \
	width(w),height(h), \
	limit(align_up(sys_fnt.line_height()*width,PAGE_SIZE)), \
	text_buffer((char*)operator new(limit)), \
	back_buffer(buffer),line_size(linesize)
{
	clear();
}

void CUI::set_focus(bool f){
	if (f && !focus)
		redraw = true;
	focus = f;
}

void CUI::put(char ch,bool input){
	auto pos = edit;
	if (input){
		switch(ch){
			case '\n':
			case '\t':
				return;
		}
		text_buffer[edit++] = ch;
		if (edit >= limit)
			edit = 0;
	}
	else{
		if (tail != edit){
			dword it = edit;
			do{
				if (!it){
					it = limit - 1;
					text_buffer[0] = text_buffer[it];
				}
				else{
					--it;
					text_buffer[it + 1] = text_buffer[it];
				}
			}while(it != tail);
			redraw = true;
		}
		text_buffer[tail++] = ch;
		if (tail >= limit)
			tail = 0;
		if (++edit >= limit)
			edit = 0;
	}
	draw(ch,pos);
}

char CUI::back(void){
	if (tail == edit)
		return 0;
	if (edit == 0)
		edit = limit;
	--edit;
	auto ch = text_buffer[edit];
	auto fc = sys_fnt.get(ch);
	if (fc == nullptr)
		return ch;

	while(cursor.x < fc->advance){
		if (lines.size() <= 1 || cursor.y < sys_fnt.line_height()){
			redraw = true;
			return ch;
		}
		cursor.y -= sys_fnt.line_height();
		cursor.x = lines.back().prev_len;
		lines.pop_back();
	}

	cursor.x -= fc->advance;
	if (!focus)
		return ch;
	//clear current character
	rectangle rect;
	rect.left = cursor.x + fc->xoff;
	rect.top = cursor.y + fc->yoff;
	rect.right = rect.left + fc->width;
	rect.bottom = rect.top + fc->height;
	fill(rect);

	//redraw previous character
	dword it = edit;
	while(it != tail){
		if (it == 0)
			it = limit;
		fc = sys_fnt.get(text_buffer[--it]);
		if (fc == nullptr)
			continue;
		if (cursor.x < fc->advance)
			break;
		cursor.x -= fc->advance;
		draw(fc->charcode,it);
		break;
	}
	return ch;
}

dword CUI::get(char* dst,dword len){
	dword top = (edit >= tail) ? edit : limit;
	dword size = min(len,top - tail);
	memcpy(dst,text_buffer + tail,size);
	if (size < len && top == limit){
		dst += size;
		len -= size;
		memcpy(dst,text_buffer,min(len,edit));
		size += min(len,edit);
	}
	tail = edit;
	return size;
}

void CUI::clear(void){
	cursor = {0};
	fill(rectangle{0,0,width,height});
	tail = edit = 0;
	lines.clear();
	lines.push_back(line_info{0});
}

void CUI::fill(const rectangle& rect){
	if (!focus)
		return;
	
	auto dst = back_buffer + line_size*rect.top + rect.left;
	for (unsigned y = 0;y < (rect.bottom - rect.top);++y){
		zeromemory(dst,sizeof(dword)*(rect.right - rect.left));
		dst += line_size;
	}

	merge(update_region,rect);
}

void CUI::draw(char ch,dword pos){
	switch(ch){
		case '\n':
		{
			if (++pos >= limit)
				pos = 0;
			scroll(pos);
			return;
		}
		case '\t':
		{
			auto tab_size = 4*sys_fnt.max_width();
			cursor.x = (cursor.x / tab_size + 1)*tab_size;
			return;
		}
	}
	auto fc = sys_fnt.get(ch);
	if (fc == nullptr)
		return;
	if(cursor.x + fc->xoff + fc->width > width)
		scroll(pos);
	cursor.x = max<int>(-fc->xoff,cursor.x);
	cursor.y = max<int>(-fc->yoff,cursor.y);

	if (focus){
		fc->render([=](const font::fontchar& obj,word px,word py,bool fg){
			if (fg){
				auto x = cursor.x + obj.xoff + px;
				auto y = cursor.y + obj.yoff + py;
				if (x >= width || y >= height)
					return;
				back_buffer[line_size*y + x] = color;
			}
		});
		rectangle rect;
		rect.left = cursor.x + fc->xoff;
		rect.top = cursor.y + fc->yoff;
		rect.right = rect.left + fc->width;
		rect.bottom = rect.top + fc->height;
		merge(update_region,rect);
	}
	cursor.x += fc->advance;
	return;
}

void CUI::scroll(dword pos){
	auto length = cursor.x;
	cursor.y += sys_fnt.line_height();
	cursor.x = 0;
	if (cursor.y + sys_fnt.line_height() < height){
		lines.push_back(line_info{pos,length});
		return;
	}
	auto dy = cursor.y + sys_fnt.line_height() - height + 1;
	cursor.y -= dy;
	
	if (dy < sys_fnt.line_height()){
		lines.push_back(line_info{pos,length});
	}
	else{
		for (unsigned i = 1;i < lines.size();++i){
			lines[i - 1] = lines[i];
		}
		lines.back() = line_info{pos,length};
	}
	
	if (focus){
		for (auto line = 0;line + dy < height;++line){
			memcpy(back_buffer + line_size*line,
				back_buffer + line_size*(line + dy),
				sizeof(dword)*line_size);
		}
		zeromemory(back_buffer + line_size*cursor.y,
			sizeof(dword)*line_size*(height - cursor.y));
	}
	assign(update_region,rectangle{0,0,width,height});
}

void CUI::render(word xoff,word yoff){
	if (!focus)
		return;
	if (redraw){
		auto head = lines.front().head;
		lines.clear();
		cursor = {0};
		zeromemory(back_buffer,sizeof(dword)*line_size*height);
		redraw = false;
		lines.push_back(line_info{head,0});
		while(head != edit){
			auto pos = head;
			auto ch = text_buffer[head++];
			if (head == limit)
				head = 0;
			draw(ch,pos);
		}

		assign(update_region,rectangle{0,0,width,height});
	}
	rectangle rect(update_region);
	rect.left += xoff;
	rect.top += yoff;
	rect.right += xoff;
	rect.bottom += yoff;
	display_draw(back_buffer + line_size*update_region.top + update_region.left,&rect,line_size);

	assign(update_region,rectangle{0});
}

label::label(word len) : limit(align_up(len,0x10)), \
	buffer((dword*)operator new(size()))
{
	clear();
}

label::~label(void){
	operator delete(buffer,size());
}

size_t label::size(void) const{
	return sizeof(dword)*limit*sys_fnt.line_height();
}

void label::clear(void){
	cursor = len = 0;
	zeromemory(buffer,size());
}

bool label::put(char ch){
	auto fc = sys_fnt.get(ch);
	if (fc == nullptr)
		return false;
	if (cursor + fc->xoff + fc->width > limit)
		return false;
	cursor = max<int>(-fc->xoff,cursor);
	fc->render([=](const font::fontchar& obj,word px,word py,bool fg){
		if (fg){
			auto x = cursor + obj.xoff + px;
			auto y = max<int>(obj.yoff,0) + py;
			if (x >= limit || y >= sys_fnt.line_height())
				return;
			buffer[limit*y + x] = 0x00FFFFFF;
		}
	});
	len = cursor + fc->xoff + fc->width;
	cursor += fc->advance;
	return true;
}

void label::render(word xoff,word yoff){
	rectangle rect{xoff,yoff,xoff + len,yoff + sys_fnt.line_height()};
	display_draw(buffer,&rect,limit);
}