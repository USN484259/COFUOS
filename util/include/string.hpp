#pragma once
#include "types.hpp"
#include "util.hpp"
#include "vector.hpp"
#include "assert.hpp"

#ifndef THROW
#ifdef COFUOS
#include "bugcheck.hpp"
#define THROW(x) BugCheck(unhandled_exception,x)
#else
#define THROW throw
#endif
#endif

namespace UOS{
	template<typename C>
	class basic_string{
		vector<C> buffer;
	public:
		typedef C* iterator;
		typedef const C* const_iterator;
		basic_string(void){
			buffer.push_back((C)0);
		}
		basic_string(const char* str){
			do{
				buffer.push_back(*str);
			}while(*str++);
		}
		basic_string(const basic_string& other) : buffer(other.buffer){}
		basic_string(basic_string&& other) : buffer(move(other.buffer)){
			assert(buffer.empty());
			other.buffer.push_back((C)0);
		}

		size_t size(void) const{
			assert(buffer.size());
			return buffer.size() - 1;
		}
		bool empty(void) const{
			assert(buffer.size());
			return buffer.size() <= 1;
		}
		void clear(void){
			buffer.clear();
			buffer.push_back((C)0);
		}
		const C* c_str(void) const{
			assert(buffer.size());
			return buffer.data();
		}
		operator const C*(void) const{
			return c_str();
		}
		iterator begin(void){
			return buffer.begin();
		}
		const_iterator begin(void) const{
			return buffer.begin();
		}
		iterator end(void){
			assert(buffer.size());
			return buffer.end() - 1;
		}
		const_iterator end(void) const{
			assert(buffer.size());
			return buffer.end() - 1;
		}
		C& at(size_t index){
			assert(buffer.size());
			if (index < buffer.size() - 1)
				return buffer.at(index);
			THROW(this);
		}
		const C& at(size_t index) const{
			assert(buffer.size());
			if (index < buffer.size() - 1)
				return buffer.at(index);
			THROW(this);
		}
		C& operator[](size_t index){
			return at(index);
		}
		const C& operator[](size_t index) const{
			return at(index);
		}
		C& front(void){
			assert(buffer.size());
			if (buffer.size() > 1)
				return buffer.front();
			THROW(this);
		}
		const C& front(void) const{
			assert(buffer.size());
			if (buffer.size() > 1)
				return buffer.front();
			THROW(this);
		}
		C& back(void){
			assert(buffer.size());
			if (buffer.size() > 1)
				return at(buffer.size() - 2);
			THROW(this);
		}
		const C& back(void) const{
			assert(buffer.size());
			if (buffer.size() > 1)
				return at(buffer.size() - 2);
			THROW(this);
		}
		void push_back(C ch){
			assert(buffer.size() && buffer.back() == (C)0);
			buffer.pop_back();
			buffer.push_back(ch);
			buffer.push_back((C)0);
		}
		void pop_back(void){
			assert(buffer.size() && buffer.back() == (C)0);
			if (buffer.size() > 1){
				buffer.pop_back();
				buffer.pop_back();
				buffer.push_back((C)0);
				return;
			}
			THROW(this);
		}
	};
	typedef basic_string<char> string;
}