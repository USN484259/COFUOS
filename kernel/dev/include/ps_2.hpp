#pragma once
#include "types.hpp"
#include "sync/include/spin_lock.hpp"
#include "safe_queue.hpp"

namespace UOS{
	class PS_2{
	public:
		typedef void (*CALLBACK)(byte keycode,dword param,void* ud);
	private:
		//spin_lock lock;
		enum STATE : byte {BAD = 0, NONE, INIT, ID_ACK, ID, ID_K, MODE_M, ID_M, KEYBD_I, MOUSE_I, KEYBD, MOUSE};
		struct CHANNEL{
			STATE state = BAD;
			byte data[7];
			safe_queue<byte,0x40> queue;
		};
		static_assert(sizeof(CHANNEL) == 8 + sizeof(safe_queue<byte,0x40>),"CHANNEL size mismatch");
		CHANNEL channels[2];
		size_t timestamp = 0;
		CALLBACK callback = nullptr;
		void* userdata = nullptr;

		static void on_irq(byte,void*);
		void command(byte);
		byte read(void);
		void write(byte);
		void step_keybd(CHANNEL&);
		void step_mouse(CHANNEL&);
		byte translate(byte code,byte stat);
	public:
		PS_2(void);
		void step(size_t);
		void set(CALLBACK callback,void* data = nullptr);
	};
	extern PS_2 ps2_device;
}