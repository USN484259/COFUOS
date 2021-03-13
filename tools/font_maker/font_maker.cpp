#include "types.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>

using namespace std;
using namespace UOS;

// see https://www.angelcode.com/products/bmfont/


struct fontchar{
	dword charcode;
	word xpos;
	word ypos;
	word width;
	word height;
	short xoff;
	short yoff;
	word advance;
	byte page;
	byte channel;
};
static_assert(sizeof(fontchar) == 0x14,"fontchar size mismatch");

class DDS_image{
	dword width;
	dword height;
	dword depth;
	byte* buffer = nullptr;

public:
	DDS_image(const char* filename);
	DDS_image(const DDS_image&) = delete;
	DDS_image(DDS_image&&);
	~DDS_image(void);
	dword at(dword x,dword y) const;
};

class Font{
	ifstream font_file;
	vector<DDS_image> images;
	word line_height = 0;
	word max_height = 0;
	word page_count = 0;
public:
	Font(const char* filename);
	template<typename F>
	void proceed(F func);
	word get_line_height(void) const;
	word get_max_height(void) const;
};

DDS_image::DDS_image(const char* filename){
	ifstream dds_file(filename,ios::binary);
	if (!dds_file.is_open())
		throw runtime_error(string("cannot open ") + filename);
	do{
		dds_file.exceptions(ios::failbit | ios::badbit | ios::eofbit);
		dword buf;
		dds_file.read((char*)&buf,4);
		if (buf != 0x20534444)	//'DDS '
			break;
		dds_file.read((char*)&buf,4);
		if (buf != 0x7C)
			break;
		dds_file.seekg(4,ios::cur);
		dds_file.read((char*)&height,4);
		dds_file.read((char*)&width,4);
		dds_file.seekg(0x38,ios::cur);
		dds_file.read((char*)&buf,4);
		if (buf != 0x20)
			break;
		dds_file.seekg(8,ios::cur);
		dds_file.read((char*)&depth,4);
		if (!depth || depth > 32)
			break;

		auto size = (size_t)((depth + 7)/8)*width*height;
		cout << filename << '\t' << size << ':' << width << '*' << height << endl;
		buffer = new byte[size];
		dds_file.seekg(0x80,ios::beg);
		dds_file.read((char*)buffer,size);
		return;
	}while(false);
	throw runtime_error("bad DDS format");
}

DDS_image::DDS_image(DDS_image&& other){
	swap(width,other.width);
	swap(height,other.height);
	swap(depth,other.depth);
	swap(buffer,other.buffer);
}

DDS_image::~DDS_image(void){
	delete[] buffer;
}

dword DDS_image::at(dword x,dword y) const{
	if (x >= width || y >= height)
		throw out_of_range("DDS_image::at");
	auto size = (depth + 7)/8;
	dword res = 0;
	auto pos = buffer + size*(y * width + x);
	for (unsigned i = 0;i < size;++i){
		res |= (*pos++) << (8*i);
	}
	return res;
}

Font::Font(const char* filename) : font_file(filename,ios::binary){
	if (!font_file.is_open())
		throw runtime_error(string("cannot open ")+filename);
	font_file.exceptions(ios::failbit | ios::badbit);
	dword magic;
	font_file.read((char*)&magic,4);
	if (!font_file.good() || magic != 0x3464D42)	//'BMF\3'
		throw runtime_error("bad FNT format");
}

template<typename F>
void Font::proceed(F func){
	do{
		auto type = font_file.get();
		if (!font_file.good())
			break;
		font_file.exceptions(ios::failbit | ios::badbit | ios::eofbit);
		dword size = 0;
		font_file.read((char*)&size,4);
		auto next = (size_t)font_file.tellg() + size;
		switch(type){
		case 2:
			font_file.read((char*)&line_height,2);
			font_file.read((char*)&max_height,2);
			font_file.seekg(4,ios::cur);
			font_file.read((char*)&page_count,2);
			break;
		case 3:
			for (word i = 0;i < page_count;++i){
				string str;
				while(true){
					auto c = font_file.get();
					if (c == 0)
						break;
					str.append(1,(char)c);
				}
				images.emplace_back(str.c_str());
			}
			break;
		case 4:
			while(size){
				fontchar fc;
				font_file.read((char*)&fc,sizeof(fontchar));
				auto& image = images.at(fc.page);
				func(fc,image);
				size -= sizeof(fontchar);
			}
		}
		font_file.exceptions(ios::badbit);
		font_file.seekg(next,ios::beg);
	}while(font_file.good());
}

word Font::get_line_height(void) const{
	return line_height;
}

word Font::get_max_height(void) const{
	return max_height;
}

/*
byte line_height;
byte max_height;
word size;

struct UOS_compressed_font{
	byte code;
	byte xadv;
	byte width;
	byte height;
	byte xoff;
	byte yoff;
	byte data[align_up(width,8)*height/8];
}font_table[];
*/
void generate_font(ostream& out,const fontchar& fc,const DDS_image& image){
	out.put((byte)fc.charcode);
	out.put((byte)fc.advance);
	out.put((byte)fc.width);
	out.put((byte)fc.height);
	out.put((byte)fc.xoff);
	out.put((byte)fc.yoff);
	unsigned size = 6;
	for (word h = 0;h < fc.height;++h){
		byte data = 0;
		word w = 0;
		while(w < fc.width){
			auto pixel = image.at(fc.xpos + w,fc.ypos + h);
			if (pixel){
				auto off = w % 8;
				data |= (1 << off);
			}
			if (0 == (++w % 8)){
				out.put(data);
				++size;
				data = 0;
			}
		}
		if (w % 8){
			out.put(data);
			++size;
		}
	}
}


int main(int argc,char** argv){
	if (argc < 3){
		cout << "font_maker font_file out_file" << endl;
		return 1;
	}
	int res = 0;
	try{
		Font font(argv[1]);
		ofstream out_file(argv[2],ios::binary);
		if (!out_file.is_open())
			throw runtime_error(string("cannot open ") + argv[2]);
		dword padding = 0;
		out_file.write((const char*)&padding,4);

		font.proceed([&](const fontchar& fc,const DDS_image& image) {
			cout << "0x" << hex << fc.charcode << ' ' << (char)fc.charcode \
				<< '\t' << dec << fc.width << '*' << fc.height \
				<< "\t@ " << (unsigned)fc.page << ':' << fc.xpos << ',' << fc.ypos << endl;
			generate_font(out_file,fc,image);
		});
		size_t size = out_file.tellp();
		if (size > 0x10004)
			throw runtime_error("compact font length overflow");
		out_file.seekp(0,ios::beg);
		word length = size - 4;
		out_file.put((byte)font.get_line_height());
		out_file.put((byte)font.get_max_height());
		out_file.write((const char*)&length,2);
	}
	catch(exception& e){
		cerr << typeid(e).name() << '\t' << e.what() << endl;
		res = -1;
	}
	return res;
}