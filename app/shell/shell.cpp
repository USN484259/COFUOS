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

back_buffer* back_buf = nullptr;
label* wall_clock = nullptr;
label* status_bar = nullptr;
terminal* term[12] = {0};

back_buffer::back_buffer(word w,word h) : \
	width(w), height(h), \
	line_size(align_up(w,0x10)), \
	line_count(align_up(h + sys_fnt.line_height()*4,0x10)), \
	buffer((dword*)operator new(sizeof(dword)*line_size*line_count)) {}

back_buffer::~back_buffer(void){
	operator delete(buffer,sizeof(dword)*line_size*line_count);
}

work_dir::work_dir(void){
	limit = 16;
	buffer = (char*)operator new(limit);
	buffer[0] = '/';
	buffer[1] = 0;
	count = 1;
}

bool work_dir::set(const char* path,dword len){
	set_work_dir(buffer,count);
	if (SUCCESS != set_work_dir(path,len))
		return false;
	auto sz = limit - 1;
	switch(get_work_dir(buffer,&sz)){
		case SUCCESS:
			count = sz;
			buffer[count] = 0;
			return true;
		case TOO_SMALL:
			break;
		default:
			return false;
	}
	operator delete(buffer,limit);
	limit = UOS::align_up(sz + 1,0x10);
	buffer = (char*)operator new(limit);

	sz = limit - 1;
	if (SUCCESS != get_work_dir(buffer,&sz)){
		buffer[0] = 0;
		return false;
	}
	count = sz;
	buffer[count] = 0;
	return true;
}

terminal::terminal(back_buffer& buf,const char* msg) : buffer(buf), \
	cui(buf.width,buf.height,buf.buffer,buf.line_size,buf.line_count)
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

	HANDLE th = get_thread();
	auto priority = get_priority(th);
	close_handle(th);

	res = create_thread(thread_reader,this,0,&th);
	assert(SUCCESS == res);
	set_priority(th,priority - 1);
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
			res = wait_for(self->out_pipe,0,0);
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
		res = wait_for(self->barrier,0,0);
		assert(NOTIFY == res || PASSED == res);
		//fprintf(stderr,"monitoring %x",self->ps);
		res = wait_for(self->ps,0,0);
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

void terminal::show_help(void){
	print("Press APP + F1 - F12 to switch terminal\n");
	print("Use 'su' prefix to grant higher privilege\n");
	print("Use 'bg' prefix to run command in background\n");
	print("\nShell interal commands:\n");
	print("help\tShow this help\n");
	print("cd\tChange directory\n");
	print("clear\tClear screen\n");
	print("halt\tShutdown system\n");
	print("break\tTrigger debugger break\n");
	print("\nBasic commands:\n");
	print("info\tShow system information\n");
	print("echo\tEcho back characters\n");
	print("ls\tList elements in directory\n");
	print("cat\tConcatenate files\n");
	print("pwd\tShow working directory\n");
	print("ps\tProcess list or detail\n");
	print("kill\tTerminate process\n");
	print("date\tShow date and time\n");
	print("file\tOpen and operate file\n");
}

void terminal::dispatch(void){
	char buffer[0x100];
	dword size = cui.get(buffer,sizeof(buffer) - 1);
	cui.put('\n');
	buffer[size] = 0;
	if (ps){
		buffer[size++] = '\n';
		dword res = stream_write(in_pipe,buffer,&size);
		assert(0 == res);
		in_pipe_dirty = true;
		return;
	}
	do{
		bool su = false;
		bool bg = false;

		auto cmd = buffer;
		auto tail = cmd + size;
		auto pos = tail;
		//parse modifier
		while(cmd != tail){
			pos = find_first_of(cmd,tail,' ');
			if (pos == cmd){
				++cmd;
				continue;
			}
			size = pos - cmd;
			if (size != 2)
				break;
			if (2 == match(cmd,"su",2,equal_to<char>())){
				su = true;
				cmd = pos;
				continue;
			}
			if (2 == match(cmd,"bg",2,equal_to<char>())){
				bg = true;
				cmd = pos;
				continue;
			}
			break;
		}
		if (cmd == tail)
			break;
		
		if (size == 2 && match<const char*>(cmd,"cd",2,equal_to<char>()) == 2){
			cmd += 2;
			while(cmd != tail){
				if (*cmd != ' ')
					break;
				++cmd;
			}
			if (cmd != tail && !wd.set(cmd,tail - cmd)){
				print("No such directory");
			}
			break;
			
		}
		if (size == 6 && match<const char*>(cmd,"set_rw",6,equal_to<char>()) == 6){
			cmd += 6;
			pos = find_first_of(cmd,tail,'-');
			size = tail - pos;
			bool force = (size == 6 && match<const char*>(pos,"-force",6,equal_to<char>()) == 6);
			if (SUCCESS == osctl(set_rw,force ? cmd : nullptr,&size)){
				print("Disk now writable");
			}
			else{
				print("Failed setting writable");
			}
			break;
		}
		if (size == 5 && match<const char*>(cmd,"clear",5,equal_to<char>()) == 5){
			cui.clear();
			break;
		}
		if (size == 4 && match<const char*>(cmd,"help",4,equal_to<char>()) == 4){
			show_help();
			break;
		}
		if (size == 6 && match<const char*>(cmd,"reload",6,equal_to<char>()) == 6){
			cui.set_focus(false);
			cui.set_focus(true);
			break;
		}
		if (size == 4 && match<const char*>(cmd,"halt",4,equal_to<char>()) == 4){
			osctl(halt,nullptr,&size);
			abort();
		}
		if (size == 5 && match<const char*>(cmd,"abort",5,equal_to<char>()) == 5){
			abort();
		}
		if (size == 5 && match<const char*>(cmd,"break",5,equal_to<char>()) == 5){
			osctl(dbgbreak,nullptr,&size);
			break;
		}

		STARTUP_INFO info = {0};
		info.commandline = cmd;
		info.cmd_length = tail - cmd;
		info.work_dir = wd.get();
		info.wd_length = wd.size();
		info.flags = su ? SHELL : NORMAL;
		info.std_handle[0] = bg ? 0 : in_pipe;
		info.std_handle[1] = out_pipe;
		info.std_handle[2] = out_pipe;

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
			cmd[size] = 0;

			print("command \'");
			print(cmd);
			print("\' not found");
		}

	}while(false);
	show_shell();
}

void terminal::begin_paint(void){
	//fprintf(stderr,"begin_paint");
	auto res = wait_for(lock,0,0);
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
	assert(0 == wait_for(lock,0,1));
	while(*str){
		cui.put(*str++);
	}
}

void terminal::show_shell(void){
	if (!cui.is_newline())
		cui.put('\n');
	print(wd.get());
	print(" $ ");
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

	if (clock_pos){
		rectangle rect{clock_pos,resolution[1] - sys_fnt.line_height(),resolution[0],resolution[1]};
		display_fill(0,&rect);
	}
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

void update_status(void){
	auto prev_len = status_bar->length();
	status_bar->clear();
	status_bar->put("Terminal ");
	auto number = active_index + 1;
	status_bar->put('0' + number/10);
	status_bar->put('0' + number%10);
	
	char str[0x40];
	OS_INFO info;
	dword sz = sizeof(info);
	if (SUCCESS == os_info(&info,&sz)){
		char total_unit = (info.total_memory > 0x100000) ? 'M' : 'K';
		unsigned total_divider = (total_unit == 'M') ? 0x100000 : 0x400;
		char used_unit = (info.used_memory > 0x100000) ? 'M' : 'K';
		unsigned used_divider = (used_unit == 'M') ? 0x100000 : 0x400;
		sz = 1 + snprintf(str,sizeof(str),\
			"\t%u Tasks    Mem %llu%c/%llu%c",\
			(info.process_count >= 2) ? (info.process_count - 2) : 0,\
			info.used_memory/used_divider, used_unit,\
			info.total_memory/total_divider, total_unit
		);
		status_bar->put(str);
	}
	auto cur_len = status_bar->length();
	if (cur_len < prev_len){
		rectangle rect{prev_len,resolution[1] - sys_fnt.line_height(),cur_len,resolution[1]};
		display_fill(0,&rect);
	}
	status_bar->render(0,resolution[1] - sys_fnt.line_height());
}

int main(int argc,char** argv){
	fprintf(stderr,"%s\n",argv[0]);
	{
		OS_INFO info;
		dword len = sizeof(info);
		if (SUCCESS != os_info(&info,&len))
			return -1;

		resolution[0] = info.resolution_x;
		resolution[1] = info.resolution_y;
		back_buf = new back_buffer(resolution[0],resolution[1] - sys_fnt.line_height());
		wall_clock = new label(8*sys_fnt.max_width());
		status_bar = new label(resolution[0] / 4 * 3);
		term[0] = new terminal(*back_buf);
	}
	while(true){
		byte buffer[0x10];
		dword size = 0x10;
		dword res;
		res = stream_read(STDIN,buffer,&size);
		assert(0 == res);
		if (size == 0){
			res = wait_for(STDIN,0,0);
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
				update_status();
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
						term[active_index] = new terminal(*back_buf);
						painting = false;
					}
					update_status();
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