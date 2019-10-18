#pragma once
#include "types.hpp"
#include "util.hpp"
#include "assert.hpp"

namespace UOS{
	template<typename T>
	class queue{
		word count,limit;
		word head,tail;
		T* buffer;
		
	public:
		queue(void) : count(0),limit(0),head(0),tail(0),buffer(nullptr){}
		queue(size_t maxcount) : count(0),limit(maxcount),head(0),tail(0),buffer( (T*)new byte[maxcount * sizeof(T)] ){}
		queue(const queue&)=delete;
		queue(queue&&)=delete;
		queue& operator=(const queue&)=delete;
		queue& operator=(queue&&)=delete;
		
		~queue(void){
			release();
		}
		
		void release(void){
			clear();
			assert(0,count);
			limit=0;
			delete[] buffer;
			buffer=nullptr;
		}
		
		void reserve(size_t maxcount){
			assert(nullptr,buffer);
			assert(0,limit);
			buffer=(T*)new byte[maxcount * sizeof(T)];
			limit=maxcount;
		}
		
		
		void swap(queue& obj){
			swap(limit,obj.limit);
			swap(count,obj.count);
			swap(head,obj.head);
			swap(tail,obj.tail);
			swap(buffer,obj.buffer);
		}
		
		size_t size(void) const{
			assertless(count,limit+1);
			return count;
		}
		
		size_t capacity(void) const{
			assertless(count,limit+1);
			return limit;
		}
		
		void clear(void){
			while(size())
				pop_back();
			
		}
		
		void push_back(T&& val){
			assertinv(nullptr,buffer);
			assertless(count,limit);
			assertless(tail,limit);
			tail=(tail+1)%limit;
			new(buffer+tail) T(val);
			++count;
		}
		
		void push_front(T&& val){
			assertinv(nullptr,buffer);
			assertless(count,limit);
			assertless(head,limit);
			head=(word)( (head+limit-1)%limit );
			new(buffer+head) T(val);
			++count;
		}
		
		void pop_back(void){
			assertinv(nullptr,buffer);
			assertinv(0,count);
			assertless(count,limit+1);
			assertless(tail,limit);
			(buffer+tail)->~T();
			
			tail=(word)( (tail+limit-1)%limit );
			if (0 == --count){
				assert(head,tail);
				head=tail=0;
			}
			
		}
		
		void pop_front(void){
			assertinv(nullptr,buffer);
			assertinv(0,count);
			assertless(count,limit+1);
			assertless(head,limit);
			(buffer+head)->~T();
			
			head=(head+1)%limit;
			if (0 == --count){
				assert(head,tail);
				head=tail=0;
			}
		}
		
		T& back(void){
			assertinv(nullptr,buffer);
			assertless(tail,limit);
			return buffer[tail];
		}
		
		T& front(void){
			assertinv(nullptr,buffer);
			assertless(head,limit);
			return buffer[head];
		}

		const T& back(void) const{
			assertinv(nullptr,buffer);
			assertless(tail,limit);
			return buffer[tail];
		}
		
		const T& front(void) const{
			assertinv(nullptr,buffer);
			assertless(head,limit);
			return buffer[head];
		}
		
		T& operator[](size_t index){
			assertinv(nullptr,buffer);
			assertless(count,limit+1);
			assertless(index,count);
			return buffer[(head+index)%limit];
		}

		const T& operator[](size_t index) const{
			assertinv(nullptr,buffer);
			assertless(count,limit+1);
			assertless(index,count);
			return buffer[(head+index)%limit];
		}

	};




}