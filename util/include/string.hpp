#pragma once
#include "types.h"
#include "container.hpp"
#include "util.hpp"
#include "vector.hpp"
#include "hash.hpp"
#include "assert.hpp"


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
			if (str == nullptr){
				buffer.push_back((C)0);
			}
			else{
				do{
					buffer.push_back(*str);
				}while(*str++);
			}
			assert(buffer.back() == 0);
		}
		template<typename It>
		basic_string(It head,It tail){
			while(head != tail){
				buffer.push_back(*head);
				++head;
			}
			buffer.push_back((C)0);
		}
		basic_string(const basic_string& other) : buffer(other.buffer){}
		basic_string(basic_string&& other) : buffer(move(other.buffer)){
			assert(other.buffer.empty());
			other.buffer.push_back((C)0);
		}
		basic_string& operator=(const basic_string& other){
			clear();
			for (C ch : other){
				buffer.push_back(ch);
			}
			buffer.push_back((C)0);
			assert(size() == other.size());
			return *this;
		}
		basic_string& operator=(basic_string&& other){
			clear();
			buffer.swap(other.buffer);
			assert(other.size() == 0);
			return *this;
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
			THROW("index %x out of range @ %p",index,this);
		}
		const C& at(size_t index) const{
			assert(buffer.size());
			if (index < buffer.size() - 1)
				return buffer.at(index);
			THROW("index %x out of range @ %p",index,this);
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
			THROW("access empty string @ %p",this);
		}
		const C& front(void) const{
			assert(buffer.size());
			if (buffer.size() > 1)
				return buffer.front();
			THROW("access empty string @ %p",this);
		}
		C& back(void){
			assert(buffer.size());
			if (buffer.size() > 1)
				return at(buffer.size() - 2);
			THROW("access empty string @ %p",this);
		}
		const C& back(void) const{
			assert(buffer.size());
			if (buffer.size() > 1)
				return at(buffer.size() - 2);
			THROW("access empty string @ %p",this);
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
			THROW("access empty string @ %p",this);
		}
		bool operator==(const C* str) const{
			if (str == nullptr)
				return false;
			for (auto it = begin();it != end();++it,++str){
				if (*str == 0)
					return false;
				if (*it != *str)
					return false;
			}
			return (*str == 0);
		}
		template<typename T>
		bool operator==(const T& str) const{
			if (size() != str.size())
				return false;
			return size() == match(begin(),str.begin(),size());
		}
		template<typename T>
		bool operator!=(const T& val) const{
			return ! operator==(val);
		}
		const_iterator find_first_of(C ch) const{
			for (auto it = begin();it != end();++it){
				if (*it == ch)
					return it;
			}
			return end();
		}
		const_iterator find_last_of(C ch) const{
			auto it = end();
			if (it != begin()){
				assert(!empty());
				do{
					--it;
					if (*it == ch)
						return it;
				}while(it != begin());
			}
			else{
				assert(empty());
			}
			return end();
		}
		basic_string substr(const_iterator head,const_iterator tail) const{
			return basic_string(head,tail);
		}
	};
	template<typename T>
	struct hash<basic_string<T> >
	{
		qword operator()(const basic_string<T>& str){
			return fasthash64(str.c_str(),sizeof(T)*str.size(),0);
		}
	};
	typedef basic_string<char> string;
}