#pragma once
#include "types.h"
#include "container.hpp"
#include "util.hpp"
#include "hash.hpp"

namespace UOS{
	template<typename C,typename L = word>
	class basic_literal{
		static_assert(sizeof(C) <= sizeof(L),"invalid combination of basic_literal");
		void* buffer = nullptr;
	public:
		typedef const C* iterator;

		basic_literal(void) = default;
		basic_literal(const C* str){
			assign(str);
		}
		template<typename It>
		basic_literal(It head,It tail){
			assign(head,tail);
		}
		basic_literal(const basic_literal& other){
			assign(other.begin(),other.end());
		}
		basic_literal(basic_literal&& other){
			UOS::swap(buffer,other.buffer);
		}
		~basic_literal(void){
			clear();
		}
		basic_literal& operator=(const basic_literal& other){
			assign(other.begin(),other.end());
			return *this;
		}
		basic_literal& operator=(basic_literal&& other){
			clear();
			UOS::swap(buffer,other.buffer);
			return *this;
		}
		void* detach(void){
			auto ptr = buffer;
			buffer = nullptr;
			return ptr;
		}
		void attach(void* ptr){
			clear();
			buffer = ptr;
		}
		size_t size(void) const{
			if (buffer == 0)
				return 0;
			assert(*(L*)buffer > 1);
			return *(L*)buffer - 1;
		}
		bool empty(void) const{
			return (buffer == nullptr);
		}
		void clear(void){
			if (buffer){
				auto len = *(L*)buffer;
				operator delete(buffer,sizeof(L) + sizeof(C)*len);
				buffer = nullptr;
			}
		}
		const C* c_str(void) const{
			if (!buffer)
				return (const C*)"\0\0\0";
			return (const C*)((L const*)buffer + 1);
		}
		operator const C*(void) const{
			return c_str();
		}
		void assign(const C* str){
			clear();
			if (str == nullptr)
				return;
			auto tail = str;
			while(*tail){
				++tail;
			}
			assign(str,tail);
		}
		template<typename It>
		void assign(It head,It tail){
			clear();
			auto len = tail - head;
			if (len++ == 0)
				return;
			if (len != (L)len)
				THROW("literal size %x overflow @ %p",(qword)len,this);
			buffer = operator new(sizeof(L) + sizeof(C)*len);
			*(L*)buffer = len;
			auto ptr = (C*)((L*)buffer + 1);
			while(head != tail){
				*ptr++ = *head++;
			}
			*ptr = (C)0;
		}
		iterator begin(void) const{
			return (const C*)((L const*)buffer + 1);
		}
		iterator end(void) const{
			return (const C*)((L const*)buffer + 1) + size();
		}
		C at(size_t index) const{
			if (index >= size())
				THROW("index %x out of range @ %p",index,this);
			return *(begin() + index);
		}
		C operator[](size_t index) const{
			return at(index);
		}
		bool operator==(const C* str) const{
			if (str == nullptr)
				return empty();
			auto ptr = begin();
			while(ptr != end()){
				if (*str == (C)0)
					return false;
				if (*str != *ptr)
					return false;
				++ptr,++str;
			}
			return (*str == (C)0);
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

	};
	template<typename T,typename L>
	struct hash<basic_literal<T,L> >
	{
		qword operator()(const basic_literal<T,L>& lit){
			if (lit.empty())
				return 0;
			return fasthash64(lit.c_str(),sizeof(T)*lit.size(),0);
		}
	};
	typedef basic_literal<char> literal;
}