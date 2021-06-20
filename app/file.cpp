#include "uos.h"
#include "util.hpp"

using namespace UOS;

class file{
	HANDLE h = 0;
public:
	file(void) = default;
	~file(void){
		close();
	}
	void close(void){
		close_handle(h);
		h = 0;
	}
	bool open(const char* name,dword access = 0){
		close();
		return 0 == file_open(name,strlen(name),access,&h);
	}
	qword size(void){
		qword result[2];
		return SUCCESS == file_tell(h,result) ? result[1] : (-1);
	}
	qword tell(void){
		qword result[2];
		return SUCCESS == file_tell(h,result) ? result[0] : (-1);
	}
	bool seek(qword offset,dword mode = 0){
		return file_seek(h,offset,mode) == SUCCESS;
	}
	dword read(void* buffer,dword length){
		if (SUCCESS != stream_read(h,buffer,&length))
			return 0;
		if (length == 0){
			wait_for(h,0,0);
			stream_state(h,&length);
		}
		return length;
	}
	dword write(const void* data,dword length){
		if (SUCCESS != stream_write(h,data,&length))
			return 0;
		if (length == 0){
			wait_for(h,0,0);
			stream_state(h,&length);
		}
		return length;
	}
};

int main(int argc,char** argv){
	file f;
	if (argc >= 2){
		if (!f.open(argv[1])){
			printf("cannot open %s\n",argv[1]);
		}
	}
	while(true){
		fputs("FILE > ",stdout);
		char buffer[0x40];
		if (nullptr == fgets(buffer,sizeof(buffer),stdin))
			break;
		
		auto ptr = buffer;
		while(*ptr && isspace(*ptr))
			++ptr;

		if (ptr == strstr(ptr,"exit")){
			break;
		}
		if (ptr == strstr(ptr,"help")){
			puts("Available commands:");
			puts("help\t\tShow help");
			puts("exit\t\tExit program");
			puts("open (path)\t Open file");
			puts("close\t\tClose file");
			puts("size\t\tShow file size");
			puts("tell\t\tShow file offset");
			puts("seek (offset)\tSet file offset");
			puts("read [size]\tRead file");
			puts("write (string)\tWrite file");
			continue;
		}
		if (ptr == strstr(ptr,"open")){
			ptr += 4;
			while(*ptr && isspace(*ptr))
				++ptr;
			{
				auto tail = ptr;
				while(*tail && !isspace(*tail))
					++tail;
				*tail = 0;
			}
			if (!f.open(ptr)){
				printf("cannot open %s\n",ptr);
			}
			continue;
		}
		if (ptr == strstr(ptr,"close")){
			f.close();
			continue;
		}
		if (ptr == strstr(ptr,"size")){
			auto sz = f.size();
			if (sz < ((qword)1 << 63)){
				printf("file size = 0x%llx\n",sz);
			}
			else{
				puts("failed getting size");
			}
			continue;
		}
		if (ptr == strstr(ptr,"tell")){
			auto off = f.tell();
			if (off < ((qword)1 << 63)){
				printf("file offset = 0x%llx\n",off);
			}
			else{
				puts("failed getting offset");
			}
			continue;
		}
		if (ptr == strstr(ptr,"seek")){
			ptr += 4;
			while(*ptr && isspace(*ptr))
				++ptr;
			const char* end;
			auto off = strtoull(ptr,&end,0);
			if (end == ptr){
				puts("seek (offset)");
			}
			else{
				if (!f.seek(off)){
					printf("failed seeking file 0x%x\n",off);
				}
			}
			continue;
		}
		if (ptr == strstr(ptr,"read")){
			ptr += 4;
			while(*ptr && isspace(*ptr))
				++ptr;
			auto sz = strtoul(ptr,nullptr,0);
			if (sz == 0)
				sz = 0x10;
			else if (sz > 0x2000)
				sz = 0x2000;
			char data[sz + 1];
			auto res = f.read(data,sz);
			assert(res <= sz);
			if (res){
				data[res] = 0;
				printf("read data of 0x%x bytes\n%s\n",res,data);
			}
			else{
				printf("failed reading 0x%x bytes\n",sz);
			}
			continue;
		}
		if (ptr == strstr(ptr,"write")){
			ptr += 5;
			while(*ptr && isspace(*ptr))
				++ptr;
			auto tail = ptr;
			while(*tail && *tail != '\n')
				++tail;
			dword sz = tail - ptr;
			if (sz){
				auto res = f.write(ptr,sz);
				if (res == sz){
					printf("write data of 0x%x bytes\n",res);
				}
				else{
					printf("failed writing 0x%x bytes (0x%x)\n",sz,res);
				}
			}
			else{
				puts("write (string)");
			}
			continue;
		}
		
		printf("unknown command %s",ptr);
	}
	return 0;
}