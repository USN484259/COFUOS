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

text_buffer::text_buffer(dword lim) : \
	buffer((char*)operator new(lim)), limit(lim)
{
	clear();
}

text_buffer::~text_buffer(void){
	operator delete(buffer,(size_t)limit);
}

void text_buffer::clear(void){
	tail = edit = 0;
}

text_buffer::iterator text_buffer::end(bool input) const{
	return iterator(this,input ? edit : tail);
}

char text_buffer::at(iterator it) const{
	assert(it.pos < limit);
	return buffer[it.pos];
}

text_buffer::iterator text_buffer::push(char ch,bool input){
	auto pos = edit;
	if (input){
		buffer[edit++] = ch;
		if (edit >= limit)
			edit = 0;
	}
	else{
		if (tail != edit){
			dword it = edit;
			do{
				if (!it){
					it = limit - 1;
					buffer[0] = buffer[it];
				}
				else{
					--it;
					buffer[it + 1] = buffer[it];
				}
			}while(it != tail);
		}
		buffer[tail++] = ch;
		if (tail >= limit)
			tail = 0;
		if (++edit >= limit)
			edit = 0;
	}
	return iterator(this,pos);
}

char text_buffer::pop(void){
	if (tail == edit)
		return 0;
	if (edit == 0)
		edit = limit;
	return buffer[--edit];
}

text_buffer::iterator text_buffer::commit(void){
	auto pos = tail;
	tail = edit;
	return iterator(this,pos);
}

text_buffer::iterator::iterator(const text_buffer* o,dword p) : owner(o),pos(p) {}

text_buffer::iterator& text_buffer::iterator::operator=(const iterator& other){
	owner = other.owner;
	pos = other.pos;
	return *this;
}
bool text_buffer::iterator::operator==(const iterator& cmp) const{
	return owner == cmp.owner && pos == cmp.pos;
}
bool text_buffer::iterator::operator!=(const iterator& cmp) const{
	return ! operator==(cmp);
}
char text_buffer::iterator::operator*(void) const{
	if (owner){
		assert(pos < owner->limit);
		return owner->buffer[pos];
	}
	return 0;
}
text_buffer::iterator& text_buffer::iterator::operator++(void){
	assert(owner);
	if (++pos >= owner->limit)
		pos = 0;
	return *this;
}
text_buffer::iterator& text_buffer::iterator::operator--(void){
	assert(owner);
	if (pos == 0)
		pos = owner->limit;
	--pos;
	return *this;
}

screen_buffer::screen_buffer(dword* ptr,word ls,word lc,word ws) : \
	buffer(ptr),line_size(ls),line_count(lc),window_size(ws)
{
	assert(window_size < line_size);
}

bool screen_buffer::set_focus(bool f){
	auto old = focus;
	focus = f;
	return old;
}

bool screen_buffer::clear(void){
	if (!focus)
		return false;
	zeromemory(buffer,sizeof(dword)*line_size*window_size);
	head_line = 0;
	return true;
}

bool screen_buffer::scroll(word dy){
	if (!focus || 0 == dy)
		return false;
	// [head_line + window_size,head_line + window_size + dy)
	auto tail = (head_line + window_size) % line_count;
	if (tail + dy <= line_count){
		zeromemory(buffer + line_size*tail,sizeof(dword)*line_size*dy);
	}
	else{
		auto count = line_count - tail;
		zeromemory(buffer + line_size*tail,sizeof(dword)*line_size*count);
		zeromemory(buffer,sizeof(dword)*line_size*(dy - count));
	}
	head_line = (head_line + dy) % line_count;
	return true;
}

bool screen_buffer::pixel(word x,word y,dword color){
	if (!focus || x >= line_size || y >= window_size)
		return false;
	auto line = (head_line + y) % line_count;
	buffer[line_size*line + x] = color;
	return true;
}

bool screen_buffer::fill(const rectangle& rect){
	if (!focus || rect.left >= rect.right || rect.top >= rect.bottom \
		|| rect.right > line_size || rect.bottom > window_size)
			return false;
	auto line = (head_line + rect.top) % line_count;
	for (auto cnt = rect.top;cnt != rect.bottom;++cnt){
		zeromemory(buffer + line_size*line + rect.left,sizeof(dword)*(rect.right - rect.left));
		if (++line >= line_count)
			line = 0;
	}
	return true;
}

void screen_buffer::imp_render(word xoff,word yoff,const rectangle& rect,dword* line_base){
	rectangle region(rect);
	region.left += xoff;
	region.top += yoff;
	region.right += xoff;
	region.bottom += yoff;
	display_draw(line_base + region.left,&region,line_size);
}

bool screen_buffer::render(word xoff,word yoff,const rectangle& rect){
	if (!focus || rect.left >= rect.right || rect.top >= rect.bottom)
		return false;
	//rectangle region(rect);
	auto line = (head_line + rect.top) % line_count;
	if (line + (rect.bottom - rect.top) <= line_count){
		imp_render(xoff,yoff,rect,buffer + line_size*line);
	}
	else{
		auto slice = line_count - line;
		rectangle region(rect);
		region.bottom = rect.top + slice;
		imp_render(xoff,yoff,region,buffer + line_size*line);
		region.top = rect.top + slice;
		region.bottom = rect.bottom;
		imp_render(xoff,yoff,region,buffer);
	}
	return true;
}

CUI::CUI(word w,word h,dword* buffer,word ls,word lc) : \
	width(w),height(h),
	text(align_up((dword)(h/sys_fnt.line_height()*width),PAGE_SIZE)),
	screen(buffer,ls,lc,h)
{
	clear();
}

void CUI::set_focus(bool f){
	if (!screen.set_focus(f) && f)
		redraw = true;
}

void CUI::put(char ch,bool input){
	auto pos = text.push(ch,input);
	draw(ch,pos);
}

char CUI::back(void){
	if (!text.has_edit())
		return 0;
	auto ch = text.pop();
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
	if (!screen.has_focus())
		return ch;
	//clear current character
	rectangle rect;
	rect.left = cursor.x + fc->xoff;
	rect.top = cursor.y + fc->yoff;
	rect.right = rect.left + fc->width;
	rect.bottom = rect.top + fc->height;
	fill(rect);

	//redraw previous character
	auto it = text.end(true);
	while(it != text.end(false)){
		fc = sys_fnt.get(text.at(--it));
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
	if (!text.has_edit())
		return 0;
	dword count = 0;
	for (auto it = text.commit();count < len && it != text.end(false);++count,++it){
		dst[count] = *it;
	}
	return count;
}

void CUI::clear(void){
	cursor = {0};
	fill(rectangle{0,0,width,height});
	text.clear();
	lines.clear();
	lines.push_back(line_info{text.end(false),0});
}

void CUI::fill(const rectangle& rect){
	if (screen.fill(rect))
		merge(update_region,rect);
}

void CUI::draw(char ch,text_buffer::iterator pos){
	switch(ch){
		case '\n':
		{
			scroll(++pos);
			return;
		}
		case '\t':
		{
			cursor.x += sys_fnt.get(' ')->advance;
			auto tab_size = sys_fnt.table_size();
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

	if (screen.has_focus()){
		fc->render([=](const font::fontchar& obj,word px,word py,bool fg){
			if (fg){
				auto x = cursor.x + obj.xoff + px;
				auto y = cursor.y + obj.yoff + py;
				screen.pixel(x,y,color);
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

void CUI::scroll(text_buffer::iterator pos){
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
	screen.scroll(dy);
	assign(update_region,rectangle{0,0,width,height});
}

void CUI::render(word xoff,word yoff){
	if (!screen.has_focus())
		return;
	if (redraw){
		auto head = lines.front().head;
		lines.clear();
		cursor = {0};
		screen.clear();
		redraw = false;
		lines.push_back(line_info{head,0});
		for (;head != text.end(true);++head){
			auto ch = text.at(head);
			draw(ch,head);
		}
		assign(update_region,rectangle{0,0,width,height});
	}
	screen.render(xoff,yoff,update_region);
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
	if (ch == '\t'){
		cursor += sys_fnt.get(' ')->advance;
		auto tab_size = sys_fnt.table_size();
		cursor = (cursor / tab_size + 1)*tab_size;
		return true;
	}
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
			buffer[limit*y + x] = 0x00FFFFFF;
		}
	});
	len = cursor + fc->xoff + fc->width;
	cursor += fc->advance;
	return true;
}

bool label::put(const char* str){
	while(*str){
		if (!put(*str++))
			return false;
	}
	return true;
}

void label::render(word xoff,word yoff){
	rectangle rect{xoff,yoff,xoff + len,yoff + sys_fnt.line_height()};
	display_draw(buffer,&rect,limit);
}