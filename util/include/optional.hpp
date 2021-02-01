#pragma once
#include "types.hpp"

namespace UOS{

	template<typename T>
	class optional{		//simple and *buggy* implementation of C++17 std::optional
		alignas(T) byte buffer[sizeof(T)];
		bool valid = false;
	public:
		optional(void) = default;
		optional(T&& other) {
			construct(move(other));
		}
		optional(const optional&) = delete;
		optional(optional&& other){
			if (other.valid){
				construct(other.get());
				other.destruct();
			}
		}
		~optional(void){
			if (valid)
				destruct();
		}
		operator bool(void) const{
			return valid;
		}

		void assign(T&& other){
			construct(move(other));
		}
		template<typename ... Arg>
		void emplace(Arg&& ... args){
			if (valid)
				destruct();
			
			new(buffer) T(forward<Arg>(args)...);
			valid = true;
		}

		T* operator->(void){
			return (T*)buffer;
		}
		T const* operator->(void) const{
			return (T const*)buffer;
		}
		T& get(void){
			return * operator->();
		}
		const T& get(void) const{
			return * operator->();
		}
		
	private:
		void construct(T&& other){
			if (valid)
				destruct();

			new(buffer) T(move(other));
			valid = true;
		}
		void destruct(void){
			if (valid){
				operator->() -> ~T();
				valid = false;
			}
		}
	};
}