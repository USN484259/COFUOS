#include <stdexcept>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <set>

using namespace std;

typedef unsigned char byte;

class font_exception : public runtime_error{
	public:
	font_exception(const char *errmsg) : runtime_error(errmsg){}
};
	

class dds{
	unsigned width,height;
	vector<byte> buf;
	
	public:
	
	dds(const char* filename){
		ifstream in(filename,ios::binary);
		if (!in.is_open())
			throw font_exception("cannot open dds file");
		
		do{
		
			unsigned tmpbuf;
			in.read(reinterpret_cast<char*>(&tmpbuf),4);
			if (!in.good() || 0x20534444!=tmpbuf)	//'DDS '
				break;
			in.read(reinterpret_cast<char*>(&tmpbuf),4);
			if (!in.good() || 124!=tmpbuf)
				break;
			
			in.seekg(4,ios::cur);
			
			in.read(reinterpret_cast<char*>(&tmpbuf),4);
			if (!in.good())
				break;
			height=tmpbuf;
			
			in.read(reinterpret_cast<char*>(&tmpbuf),4);
			if (!in.good())
				break;
			width=tmpbuf;		
			
			unsigned long len=(unsigned long)width*height;
			buf.reserve(len);
			in.seekg(128);
			
			//cout<<len<<endl;
			
			in.read(reinterpret_cast<char*>(&buf[0]),len);
			if (!in.good())
				break;
			
			in.close();
			return;
		}while(false);
		in.close();
		throw font_exception("bad dds format");
	}
	
	void select(vector<bool> dst,unsigned x,unsigned y,unsigned w,unsigned h) const{
		do{
			if (dst.capacity()<(unsigned long)w*h)
				break;
			
			vector<bool>::iterator it=dst.begin();
			
			while(h--){
				
				unsigned i;
				for (i=0;i<w;i++){
					unsigned off=x+i;
					if (off>=width)
						break;
					*it++ = buf.at(width*y + off)?true:false;
					
				}
				
				
				if (i!=w || ++y >= height)
					break;
			}
			
			if (++h)	//succees
				return;
			
		}while(false);
		throw font_exception("bad dds select");
	}
	
};


class fnt{
	
	struct fontchar{
		unsigned int id;
		unsigned short x;
		unsigned short y;
		unsigned short w;
		unsigned short h;
		unsigned short xoff;
		unsigned short yoff;
		unsigned short adv;
		byte page;
		byte chnl;
		
		bool operator<(const fontchar& cmp) const{
			return id<cmp.id;
		}
		
	};
	
	
	
	set<fontchar> charset;
	deque<dds> img;
	string fontname;
	public:
	fnt(const char * filename){
		ifstream in(filename,ios::binary);
		if (!in.is_open())
			throw font_exception("cannot open fnt file");
		
		do{
			
			unsigned tmpbuf;
			in.read(reinterpret_cast<char*>(&tmpbuf),4);
			if (!in.good() || 0x3464D42!=tmpbuf)	//'BMF\3'
				break;
			
			
			while(true){
				in.read(reinterpret_cast<char*>(&tmpbuf),1);
				if (!in.good()){
					in.close();
					return;
				}
				byte type = tmpbuf & 0xFF;
				in.read(reinterpret_cast<char*>(&tmpbuf),4);
				if (!in.good())
					break;
				unsigned next = (unsigned)in.tellg() + tmpbuf;

				//cout<<(unsigned)type<<'\t'<<next<<endl;
				
				switch(type){
					case 1:		//info
					{
						in.seekg(14,ios::cur);
						fontname.clear();
						char c;
						while(c=in.get()){
							if (!in.good())
								throw font_exception("bad fnt format");
							fontname+=c;
						}
												
						break;
					}
					case 2:		//common
					break;
					
					case 3:		//pages
					{
						string str;
						char c;
						
						while(true){
							c=in.get();
							
							if (!in.good())
								throw font_exception("bad fnt format");

							
							if (!c){
								//cout<<str<<endl;
								img.emplace(img.end(),str.c_str());
								str.clear();
								continue;
							}
							if (c<0x20)
								break;
							str+=c;
						}
						break;
					}
					
					case 4:		//chars
					
					
					do{
						fontchar buf;

						in.read(reinterpret_cast<char*>(&buf),sizeof(fontchar));
						if (!in.good())
							throw font_exception("bad fnt format");
						
						charset.insert(buf);
						
						
					}while(in.tellg() < next);
					
					break;
					
					default:
					
					throw font_exception("bad fnt format");
					
				}
				
				in.seekg(next);
			}
			
			
			
		}while(false);
		in.close();
		throw font_exception("bad fnt format");
	}
	
	const string& name(void) const{
		return fontname;
	}
	
	pair<unsigned,unsigned> count(void) const{
		return pair<unsigned,unsigned>(charset.size(),img.size());
	}
};


int main(int argc,char** argv){
	int ret=0;
	try{
		if (argc<2)
			throw font_exception("too few arguments");
		
		fnt font(argv[1]);
		
		cout<<"font\t"<<font.name()<<endl;
		pair<unsigned,unsigned> info=font.count();
		cout<<info.first<<" chars\t"<<info.second<<" images"<<endl;
	
	}
	catch(exception &e){
		cerr<<e.what()<<endl;
		ret=-1;
	}
	
	return ret;
}