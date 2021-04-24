#pragma once
#include "types.h"
#include "container.hpp"
#include "util.hpp"

namespace UOS{
	template<typename T,typename C = dword>
	class vector{
		T* buffer = nullptr;
		C count = 0;
		C cap = 0;

	public:
		typedef T* iterator;
		typedef T const* const_iterator;
		vector(void) = default;
		template<typename ... Arg>
		vector(C cnt,Arg&& ... args){
			reserve(cnt);
			while(cnt--)
				push_back(forward<Arg>(args)...);
		}
		vector(const vector& other){
			reserve(other.count);
			for (const auto& ele : other){
				push_back(ele);
			}
			assert(count == other.count);
		}
		vector(vector&& other){
			swap(other);
		}
		~vector(void){
			clear();
		}
		C size(void) const{
			assert(count <= cap);
			return count;
		}
		C capacity(void) const{
			assert(count <= cap);
			return cap;
		}
		bool empty(void) const{
			assert(count <= cap);
			return count == 0;
		}
		void clear(void){
			assert(count <= cap);
			while(count)
				pop_back();
			operator delete(buffer,cap*sizeof(T));
			cap = 0;
			buffer = nullptr;
		}
		void swap(vector& other){
			using UOS::swap;
			swap(buffer,other.buffer);
			swap(count,other.count);
			swap(cap,other.cap);
		}
		void reserve(C new_size){
			assert(count <= cap);
			if (new_size <= cap)
				return;
			T* new_buffer = (T*)operator new(new_size*sizeof(T));
			for (C i = 0;i < count;++i){
				new (new_buffer + i) T(move(buffer[i]));
				buffer[i].~T();
			}
			operator delete(buffer,cap*sizeof(T));
			buffer = new_buffer;
			cap = new_size;
		}
		T* data(void){
			return buffer;
		}
		const T* data(void) const{
			return buffer;
		}
		iterator begin(void){
			return buffer;
		}
		const_iterator begin(void) const{
			return buffer;
		}
		iterator end(void){
			return buffer + count;
		}
		const_iterator end(void) const{
			return buffer + count;
		}
		T& at(C index){
			if (index < count)
				return buffer[index];
			THROW("index %x out of range @ %p",index,this);
		}
		const T& at(C index) const{
			if (index < count)
				return buffer[index];
			THROW("index %x out of range @ %p",index,this);
		}
		T& operator[](C index){
			return at(index);
		}
		const T& operator[](C index) const{
			return at(index);
		}
		T& front(void){
			if (count)
				return buffer[0];
			THROW("access empty vector @ %p",this);
		}
		const T& front(void) const{
			if (count)
				return buffer[0];
			THROW("access empty vector @ %p",this);
		}
		T& back(void){
			if (count)
				return buffer[count - 1];
			THROW("access empty vector @ %p",this);
		}
		const T& back(void) const{
			if (count)
				return buffer[count - 1];
			THROW("access empty vector @ %p",this);
		}
		template<typename ... Arg>
		void push_back(Arg&& ... args){
			if (count == cap)
				reserve(count ? 2*cap : max<C>(0x40/sizeof(T),2));
			if (count >= cap)
				THROW("vector expand failed @ %p",this);
			new (buffer + count) T(forward<Arg>(args)...);
			++count;
		}
		void pop_back(void){
			assert(count <= cap);
			if (!count)
				THROW("access empty vector @ %p",this);
			buffer[--count].~T();
		}
	};
}