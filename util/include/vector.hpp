#pragma once
#include "types.hpp"
#include "container.hpp"
#include "util.hpp"
#include "constant.hpp"
#include "lang.hpp"
#include "assert.hpp"

namespace UOS{
	template<typename T>
	class vector{
		T* buffer = nullptr;
		size_t count = 0;
		size_t cap = 0;

	public:
		typedef T* iterator;
		typedef T const* const_iterator;
		vector(void) = default;
		template<typename ... Arg>
		vector(size_t cnt,Arg&& ... args){
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
		size_t size(void) const{
			assert(count <= cap);
			return count;
		}
		size_t capacity(void) const{
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
			cap = 0;
			delete[] (byte*)buffer;
			buffer = nullptr;
		}
		void swap(vector& other){
			using UOS::swap;
			swap(buffer,other.buffer);
			swap(count,other.count);
			swap(cap,other.cap);
		}
		void reserve(size_t new_size){
			assert(count <= cap);
			if (new_size <= cap)
				return;
			T* new_buffer = (T*)new byte[new_size*sizeof(T)];
			for (size_t i = 0;i < count;++i){
				new (new_buffer + i) T(move(buffer[i]));
				buffer[i].~T();
			}
			delete[] (byte*)buffer;
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
		T& at(size_t index){
			if (index < count)
				return buffer[index];
			THROW(this);
		}
		const T& at(size_t index) const{
			if (index < count)
				return buffer[index];
			THROW(this);
		}
		T& operator[](size_t index){
			return at(index);
		}
		const T& operator[](size_t index) const{
			return at(index);
		}
		T& front(void){
			if (count)
				return buffer[0];
			THROW(this);
		}
		const T& front(void) const{
			if (count)
				return buffer[0];
			THROW(this);
		}
		T& back(void){
			if (count)
				return buffer[count - 1];
			THROW(this);
		}
		const T& back(void) const{
			if (count)
				return buffer[count - 1];
			THROW(this);
		}
		template<typename ... Arg>
		void push_back(Arg&& ... args){
			if (count == cap)
				reserve(count ? 2*cap : max((size_t)0x40/sizeof(T),(size_t)2));
			if (count >= cap)
				THROW(this);
			new (buffer + count) T(forward<Arg>(args)...);
			++count;
		}
		void pop_back(void){
			assert(count <= cap);
			if (!count)
				THROW(this);
			buffer[--count].~T();
		}
	};
}