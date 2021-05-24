#include "shell.hpp"
#include "cui.hpp"
#include "font.hpp"

using namespace UOS;

enum {CAPSLOCK = 0,CTRL = 1,ALT = 2,SHIFT = 3,SUPER = 4};

//static constexpr word border_size = 2;

word resolution[2] = {0};
bool key_state[5] = {0};
byte active_index = 0;
word clock_pos = 0;

label* wall_clock = nullptr;
terminal* term[12] = {0};

inline word count_line(word height){
	return align_up(height + sys_fnt.line_height()*2,0x10);
}

terminal::terminal(word w,word h,const char* msg) : \
	width(w),height(h),line_size(align_up(w,0x10)), \
	back_buffer((dword*)operator new(sizeof(dword)*line_size*count_line(h))), \
	cui(width,height,back_buffer,line_size,count_line(h))
{
	dword res;
	res = create_object(OBJ_SEMAPHORE,1,0,&lock);
	assert(SUCCESS == res);
	res = create_object(OBJ_EVENT,0,0,&barrier);
	assert(SUCCESS == res);
	res = create_object(OBJ_PIPE,0x100,0,&out_pipe);
	assert(SUCCESS == res);
	res = create_object(OBJ_PIPE,0x100,1,&in_pipe);
	assert(SUCCESS == res);

	cui.set_focus(true);
	begin_paint();
	if (msg)
		print(msg);
	show_shell();
	end_paint();

	HANDLE th;
	res = create_thread(thread_reader,this,0,&th);
	assert(SUCCESS == res);
	close_handle(th);

	res = create_thread(thread_monitor,this,0,&th);
	assert(SUCCESS == res);
	close_handle(th);
}

void terminal::thread_reader(void*,void* ptr){
	auto self = (terminal*)ptr;
	while(true){
		char buffer[0x40];
		dword size = sizeof(buffer);
		dword res;
		res = stream_read(self->out_pipe,buffer,&size);
		assert(0 == res);
		if (size == 0){
			res = wait_for(self->out_pipe,0);
			assert(NOTIFY == res || PASSED == res);
			res = stream_state(self->out_pipe,&size);
			assert(0 == res);
			if (size == 0)
				continue;
		}
		self->begin_paint();
		for (dword i = 0;i < size;++i){
			self->cui.put(buffer[i]);
		}
		self->end_paint();
	}
}

void terminal::thread_monitor(void*,void* ptr){
	auto self = (terminal*)ptr;
	while(true){
		dword res;
		res = wait_for(self->barrier,0);
		assert(NOTIFY == res || PASSED == res);
		//fprintf(stderr,"monitoring %x",self->ps);
		res = wait_for(self->ps,0);
		if (NOTIFY == res || PASSED == res){
			sleep(0);
			self->begin_paint();
			//dword id = process_id(self->ps);
			//dword exit_code = 0;
			//bool stopped = (SUCCESS == process_result(self->ps,&exit_code));
			//fprintf(stderr,"process %u exited with 0%c%x",id,stopped ? 'x' : '?',exit_code);
			
			close_handle(self->ps);
			self->ps = 0;

			if (self->in_pipe_dirty){
				close_handle(self->in_pipe);			
				res = create_object(OBJ_PIPE,0x100,1,&self->in_pipe);	//owner_write
				assert(SUCCESS == res);
				self->in_pipe_dirty = false;
			}
			self->show_shell();
			self->end_paint();
		}
	}
}

void terminal::dispatch(void){
	char buffer[0x100];
	dword size = cui.get(buffer,sizeof(buffer));
	cui.put('\n');

	if (ps){
		if (size < sizeof(buffer)){
			buffer[size++] = '\n';
		}
		dword res = stream_write(in_pipe,buffer,&size);
		assert(0 == res);
		in_pipe_dirty = true;
		return;
	}
	do{
		if (size == 0)
			break;

		bool su = false;
		bool bg = false;
		auto cmd = buffer;
		//parse modifier
		do{
			switch(*cmd){
				case '!':
					su = true;
					continue;
				case '~':
					bg = true;
					continue;
			}
			break;
		}while(++cmd,--size);
		if (size == 0)
			break;
		
		//skip leading spaces
		do{
			if (*cmd != ' ')
				break;
		}while(++cmd,--size);
		if (size == 0)
			break;

		if (size >= 5 && match<const char*>(cmd,"abort",5,equal_to<char>()) == 5){
			abort();
		}
		if (size >= 5 && match<const char*>(cmd,"clear",5,equal_to<char>()) == 5){
			cui.clear();
			break;
		}
		if (size >= 6 && match<const char*>(cmd,"reload",6,equal_to<char>()) == 6){
			cui.set_focus(false);
			cui.set_focus(true);
			break;
		}
		if (size >= 4 && match<const char*>(cmd,"halt",4,equal_to<char>()) == 4){
			osctl(halt,nullptr,&size);
			abort();
		}
		if (size >= 5 && match<const char*>(cmd,"break",5,equal_to<char>()) == 5){
			osctl(dbgbreak,nullptr,&size);
			break;
		}

		STARTUP_INFO info = {0};
		info.commandline = cmd;
		info.cmd_length = size;
		info.flags = su ? SHELL : NORMAL;
		info.std_handle[0] = bg ? 0 : in_pipe;
		info.std_handle[1] = out_pipe;
		info.std_handle[2] = STDERR;

		HANDLE hps = 0;
		assert(ps == 0);
		if (SUCCESS == create_process(&info,sizeof(STARTUP_INFO),&hps)){
			if (!bg){
				ps = hps;
				signal(barrier,1);
				return;
			}
			else{
				close_handle(hps);
				sleep(0);
				break;
			}
		}
		if (size){
			unsigned i;
			for (i = 0;i < size;++i){
				if (cmd[i] == ' ')
					break;
			}
			if (cmd + i == buffer + sizeof(buffer))
				--i;
			cmd[i] = 0;

			print("command \'");
			print(cmd);
			print("\' not found");
		}

	}while(false);
	show_shell();
}

void terminal::begin_paint(void){
	//fprintf(stderr,"begin_paint");
	auto res = wait_for(lock,0);
	assert(NOTIFY == res || PASSED == res);
	//fprintf(stderr,"painting");
}

void terminal::end_paint(void){
	cui.render(0,0);
	auto res = signal(lock,0);
	assert(res < FAILED);
	//fprintf(stderr,"end_paint");
}

void terminal::print(const char* str){
	assert(0 == check(lock));
	while(*str){
		cui.put(*str++);
	}
}

void terminal::show_shell(void){
	if (!cui.is_newline())
		cui.put('\n');
	print("/ $ ");
	//TODO show current directory
}

void terminal::put(char ch){
	if (ch == 0x08)	//backspace
		cui.back();
	else{
		if (ch == 0x0A)	//enter
			dispatch();
		else if (ch != '\t')
			cui.put(ch,true);
	}
}

char parse(byte key){
	if (key_state[CTRL] || key_state[ALT] || key_state[SUPER])
		return 0;
	if (isspace(key))
		return key;
	switch(key){
		case 0x08:	//backspace
		case 0x1B:	//esc
		case 0x7F:	//del
			return key;
	}
	if (isupper(key)){
		return key | (key_state[SHIFT] == key_state[CAPSLOCK] ? 0x20 : 0);
	}
	if (isdigit(key)){
		static const char* const sh_num = ")!@#$%^&*(";
		return key_state[SHIFT] ? sh_num[key - '0'] : key;
	}
	switch(key){
		case '`':
		case ',':
		case '.':
		case '/':
		case ';':
		case '\'':
		case '[':
		case ']':
		case '\\':
		case '-':
		case '=':
			break;
		default:
			return 0;
	}
	if (!key_state[SHIFT])
		return key;
	switch(key){
		case '`':
			return '~';
		case ',':
			return '<';
		case '.':
			return '>';
		case '/':
			return '?';
		case ';':
			return ':';
		case '\'':
			return '\"';
		case '[':
			return '{';
		case ']':
			return '}';
		case '\\':
			return '|';
		case '-':
			return '_';
		case '=':
			return '+';
	}
	return 0;
}

static constexpr int GMT_OFFSET = (+8);
void update_clock(void){
	qword time = get_time() + 3600*GMT_OFFSET;
	word element[3];
	time %= 86400;
	element[0] = time / 3600;
	time %= 3600;
	element[1] = time / 60;
	element[2] = time % 60;

	rectangle rect{clock_pos,resolution[1] - sys_fnt.line_height(),resolution[0],resolution[1]};
	display_fill(0,&rect);

	wall_clock->clear();
	for (auto i = 0;i < 3;++i){
		if (i)
			wall_clock->put(':');
		wall_clock->put('0' + element[i]/10);
		wall_clock->put('0' + element[i]%10);
	}
	clock_pos = resolution[0] - 8 - wall_clock->length();
	wall_clock->render(clock_pos,resolution[1] - sys_fnt.line_height());
}

int main(int argc,char** argv){
	fprintf(stderr,"%s\n",argv[0]);
	{
		char str[0x80];
		OS_INFO info[2];
		dword len = sizeof(info);
		if (SUCCESS != os_info(info,&len))
			return -1;

		char total_unit = (info->total_memory > 0x100000) ? 'M' : 'K';
		unsigned total_divider = (total_unit == 'M') ? 0x100000 : 0x400;
		char used_unit = (info->used_memory > 0x100000) ? 'M' : 'K';
		unsigned used_divider = (used_unit == 'M') ? 0x100000 : 0x400;
		len = 1 + snprintf(str,sizeof(str),\
			"%s version %x, %hu cores, %u processes, memory (%llu%c/%llu%c), resolution %d*%d\n",\
			info + 1, info->version, info->active_core, info->process_count,\
			info->used_memory/used_divider, used_unit,\
			info->total_memory/total_divider, total_unit,
			info->resolution_x,info->resolution_y
		);
		//write(stderr,str,&len);

		resolution[0] = info->resolution_x;
		resolution[1] = info->resolution_y;

		wall_clock = new label(8*sys_fnt.max_width());
		term[0] = new terminal(resolution[0],resolution[1] - sys_fnt.line_height(),str);
	}
	while(true){
		byte buffer[0x10];
		dword size = 0x10;
		dword res;
		res = stream_read(STDIN,buffer,&size);
		assert(0 == res);
		if (size == 0){
			res = wait_for(STDIN,0);
			assert(NOTIFY == res || PASSED == res);
			res = stream_state(STDIN,&size);
			assert(0 == res);
			if (size == 0)
				continue;
		}
		bool painting = false;
		for (unsigned i = 0;i < size;++i){
			auto key = buffer[i] & 0x7F;
			bool pressed = (0 == (buffer[i] & 0x80));

			if (key == 0x40){
				update_clock();
				continue;
			}
			switch(key){
				case 0x21:
				case 0x22:
				case 0x23:
				case 0x24:
				case 0x71:
				case 0x72:
				case 0x73:
				case 0x74:
					key_state[key & 0x0F] = pressed;
					continue;
			}
			if (key == 0x1A && pressed){
				key_state[CAPSLOCK] = ! key_state[CAPSLOCK];
				continue;
			}

			if (pressed){
				if (key_state[SUPER] && key >= 0x61 && key <= 0x6C){
					// F1 .. F12
					term[active_index]->set_focus(false);
					if (painting){
						term[active_index]->end_paint();
					}
					active_index = key - 0x61;
					assert(active_index < 12);
					if (term[active_index]){
						term[active_index]->begin_paint();
						term[active_index]->set_focus(true);
						painting = true;
					}
					else{
						term[active_index] = new terminal(resolution[0],resolution[1] - sys_fnt.line_height());
						painting = false;
					}
					continue;
				}
				auto ch = parse(key);
				if (ch){
					if (!painting){
						term[active_index]->begin_paint();
						painting = true;
					}
					term[active_index]->put(ch);
				}
			}
		}
		if (painting)
			term[active_index]->end_paint();
	}
	return 0;
}