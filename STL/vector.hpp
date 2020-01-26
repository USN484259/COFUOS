#pragma once
// #include "types.hpp"
// #include "constant.hpp"
// #include "util.hpp"
// #include "assert.hpp"
#include "common.hpp"

namespace UOS{
	template<typename T>
	class vector{
		size_t count,limit;
		T* buffer;

		void expand(size_t req){
			if (count+req <= limit)
				return;
			reserve(count ? 2*count : 16);
		}

		
		void r_shift(size_t from,size_t off = 1){
			assert(count+off <= limit);
			for (auto i=count;i>from;){
				--i;	
				
				if (i+off >=count)
					new(buffer+i+off) T(move(buffer[i]));
				else
					buffer[i+off] = move(buffer[i]);
			}

		}
		
		void l_shift(size_t from,size_t off = 1){
			assert(off <= count);
			for (auto i=from;i<count;++i){
				if (i+off < count)
					buffer[i] = move(buffer[i+off]);
				else
					buffer[i].~T();
			}
		}
		
	public:

		//typedef T* iterator;
		//typedef const T* const_iterator;

		using iterator = T*;
		using const_iterator = const T*;

	
	public:
		vector(void) : count(0),limit(0),buffer(nullptr){}
		vector(size_t cnt, const T& obj) : count(0), limit(cnt), buffer((T*)new byte[sizeof(T) * cnt]) {
			while (cnt--) {
				push_back(obj);
			}
		}
		vector(const vector& obj) : count(obj.count),limit(obj.limit),buffer((T*)new byte[sizeof(T)*obj.limit]){
			for (auto i=0;i<count;++i){
				new(buffer+i) T(obj.buffer[i]);
			}
		}
		vector(vector&& obj) : count(obj.count),limit(obj.limit),buffer(obj.buffer){
			obj.buffer=nullptr;
			obj.count=obj.limit=0;
		}
		
		~vector(void){
			clear();
			delete[] (byte*)buffer;
		}
		vector& operator=(const vector& obj){
			clear();
			reserve(obj.limit);
			for (auto i=0;i<obj.count;++i){
				new(buffer+i) T(obj.buffer[i]);
			}
			count=obj.count;
			return *this;
		}
		vector& operator=(vector&& obj){
			clear();
			UOS::swap(buffer, obj.buffer);
			UOS::swap(limit, obj.limit);
			UOS::swap(count, obj.count);
			/*
			delete[] (byte*)buffer;
			buffer=obj.buffer;
			count=obj.count;
			limit=obj.limit;
			obj.buffer=nullptr;
			obj.count=obj.limit=0;
			*/
			return *this;
		}
		void swap(vector& obj){
			UOS::swap(buffer,obj.buffer);
			UOS::swap(count,obj.count);
			UOS::swap(limit,obj.limit);
		}
		
		size_t size(void) const{
			return count;
		}
		
		size_t capacity(void) const{
			return limit;
		}
		
		bool empty(void) const{
			return count==0;
		}
		
		void clear(void){
			for (auto i=0;i<count;++i)
				buffer[i].~T();
			count=0;
		}
		
		void reserve(size_t req){
			if (req<=limit){
				return;
			}
			
			T* new_buf=(T*)new byte[sizeof(T)*req];
			for (auto i=0;i<count;++i){
				new(new_buf+i) T(move(buffer[i]));
				buffer[i].~T();
			}
			
			delete[] (byte*)buffer;
			
			buffer=new_buf;
			limit=req;
		}

		bool shink(size_t req = 0){
			if (req >= limit)
				return false;
			size_t new_size = max<size_t>(req,count+(limit-count)/4);
			if ( new_size - count < min<size_t>(16,PAGE_SIZE / sizeof(T)) )
				return false;
			
			T* new_buf = (T*)new byte[sizeof(T)*new_size];
			for (auto i=0;i<count;++i){
				new(new_buf+i) T(move(buffer[i]));
				buffer[i].~T();
			}
			delete[] (byte*)buffer;
			buffer = new_buf;
			limit=new_size;
			return true;
			
		}
		
		T& operator[](size_t index){
			if (index < count)
				return buffer[index];
			error(out_of_range, this);
		}
		const T& operator[](size_t index) const{
			if (index < count)
				return buffer[index];
			error(out_of_range, this);
		}
		
		T& at(size_t index){
			return operator[](index);
		}
		const T& at(size_t index) const{
			return operator[](index);
		}
		
		T& front(void){
			if (count)
				return buffer[0];
			error(out_of_range, this);
		}
		const T& front(void) const{
			if (count)
				return buffer[0];
			error(out_of_range, this);
		}
		
		T& back(void){
			if (count)
				return buffer[count-1];
			error(out_of_range, this);
		}
		const T& back(void) const{
			if (count)
				return buffer[count - 1];
			error(out_of_range, this);
		}
		
		iterator begin(void){
			return buffer;
		}
		const_iterator cbegin(void) const{
			return buffer;
		}
		iterator end(void){
			return buffer+count;
		}
		const_iterator cend(void) const{
			return buffer+count;
		}

		iterator insert(const_iterator pos,const T& val){
			size_t off=pos-buffer;
			if (off > count)
				error(out_of_range,this);
			//assertless(off,count+1);
			expand(1);
			assert(count <= limit);
			if (off==count){
				new(buffer+off) T(val);
			}
			else{
				r_shift(off);
				buffer[off] = val;
			}
			++count;
			return buffer+off;		
		}
		iterator insert(const_iterator pos,T&& val){
			size_t off=pos-buffer;
			if (off > count)
				error(out_of_range,this);
			//assertless(off,count+1);
			expand(1);
			assert(count <= limit);
			if (off==count){
				new(buffer+off) T(move(val));
			}
			else{
				r_shift(off);
				buffer[off] = move(val);
			}
			++count;
			return buffer+off;		
		}
		void push_front(const T& val){
			insert(cbegin(),val);
		}
		void push_front(T&& val){
			insert(cbegin(),move(val));
		}
		void push_back(const T& val){
			insert(cend(),val);
		}
		void push_back(T&& val){
			insert(cend(),move(val));
		}
		
		iterator erase(const_iterator pos){
			//assertinv(0,count);
			size_t off=pos-buffer;
			if (!count || off > count)
				error(out_of_range,this);
			//assertless(off,count);
			l_shift(off);
			--count;
			return buffer+off;
		}
		void pop_front(void){
			erase(cbegin());
		}
		void pop_back(void){
			erase(cend()-1);
		}
		
	};






}