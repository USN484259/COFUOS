#pragma once
#include "types.h"
#include "container.hpp"
#include "hash.hpp"

namespace UOS{
	template<typename T>
	class span{
		const T* ptr;
		size_t len;
	public:
		typedef T* iterator;
		typedef const T* const_iterator;

		template<typename It>
		span(It head,It tail) : ptr(&*head), len(tail - head) {}
		template<typename It>
		span(It head,size_t length) : ptr(&*head), len(length) {}
		template<typename Ct>
		span(const Ct& container) : ptr(container.begin()), len(container.size()) {}
		
		size_t size(void) const{
			return len;
		}
		bool empty(void) const{
			return len == 0;
		}
		iterator begin(void){
			return ptr;
		}
		const_iterator begin(void) const{
			return ptr;
		}
		iterator end(void){
			return ptr + len;
		}
		const_iterator end(void) const{
			return ptr + len;
		}
		T* data(void){
			return ptr;
		}
		const T* data(void) const{
			return ptr;
		}
		T& operator[](size_t index){
			return ptr[index];
		}
		const T& operator[](size_t index) const{
			return ptr[index];
		}
	};
	template<typename T>
	struct hash<span<T> >{
		qword operator()(const span<T>& obj){
			return fasthash64(obj.data(),sizeof(T)*obj.size(),0);
		}
	};
}