#pragma once
#include "types.hpp"
#include "sync/include/spin_lock.hpp"
#include "sync/include/lock_guard.hpp"
#include "intrinsics.hpp"
#include "assert.hpp"

namespace UOS{
	template<typename T,size_t SIZE>
	class safe_queue{
		spin_lock read_lock;
		spin_lock write_lock;
		T* const buffer;
		T* volatile head;
		T* volatile tail;
	public:
		safe_queue(void) : buffer(new T[SIZE]), head(buffer), tail(buffer) {}
		~safe_queue(void){
			assert(read_lock.try_lock());
			assert(write_lock.try_lock());
			delete[] buffer;
		}
		bool put(const T& val){
			lock_guard<spin_lock> guard(write_lock);
			assert(tail < buffer + SIZE);
			T* original_tail = tail;
			T* next = original_tail + 1;
			if (next == buffer + SIZE)
				next = buffer;
			if (next == head)
				return false;
			*original_tail = val;
			auto res = cmpxchg_ptr(&tail,next,original_tail);
			assert(res == original_tail);
			return true;
		}
		bool get(T& val){
			lock_guard<spin_lock> guard(read_lock);
			assert(head < buffer + SIZE);
			T* original_head = head;
			if (original_head == tail)
				return false;
			val = *original_head;
			T* next = original_head + 1;
			if (next == buffer + SIZE)
				next = buffer;
			auto res = cmpxchg_ptr(&head,next,original_head);
			assert(res == original_head);
			return true;
		}
		bool empty(void) const{
			return head == tail;
		}
		void clear(void){
			lock_guard<spin_lock> guard_w(write_lock);
			lock_guard<spin_lock> guard_r(read_lock);
			tail = head;
		}
	};
};