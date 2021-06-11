#pragma once
#include "types.h"
#include "container.hpp"
#include "hash.hpp"

namespace UOS{
	template<typename T,typename C = dword>
	class span{
	public:
		typedef typename UOS::remove_cv<T>::type value_type;
		typedef value_type* iterator;
		typedef const value_type* const_iterator;
	private:
		value_type* ptr = nullptr;
		C len = 0;

	public:
		span(void) = default;
		template<typename It>
		span(It head,It tail) : ptr((value_type*)&*head), len(tail - head) {}
		template<typename It>
		span(It head,C length) : ptr((value_type*)&*head), len(length) {}
		template<typename Ct>
		span(const Ct& container) : ptr((value_type*)container.begin()), len(container.size()) {}
		
		C size(void) const{
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
		T& operator[](C index){
			return ptr[index];
		}
		const T& operator[](C index) const{
			return ptr[index];
		}
	};
	template<typename T,typename C>
	struct hash<span<T,C> >{
		qword operator()(const span<T,C>& obj){
			return fasthash64(obj.data(),sizeof(T)*obj.size(),0);
		}
	};
}