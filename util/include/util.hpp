#pragma once
#include "types.hpp"

#define BITMASK(i) ( ( (qword)1<<(qword)(i) ) - 1 )
#define BIT(i) ( (qword)1 << ( (qword)(i)  ) )


namespace UOS{
	template< typename T > struct remove_reference      {typedef T type;};
	template< typename T > struct remove_reference<T&>  {typedef T type;};
	template< typename T > struct remove_reference<T&&> {typedef T type;};
	
	template< typename T >
	typename remove_reference<T>::type&& move( T&& t ){
		return static_cast<typename remove_reference<T>::type&&>(t);
	}
	
	template< class T >
	T* addressof(T& arg) 
	{
		return reinterpret_cast<T*>(
				   &const_cast<char&>(
					  reinterpret_cast<const volatile char&>(arg)));
	}
	
	template<typename T>
	class reference_wrapper{	
		T* ptr;
	public:
		reference_wrapper(T& val) : ptr(addressof(val)){}
		reference_wrapper(T&&) = delete;
		operator T&(void) const{
			return *ptr;
		}
		T& get(void) const{
			return *ptr;
		}
		
	};
	
	
	template<typename T>
	reference_wrapper<T> ref(T& t){
		return reference_wrapper<T>(t);
	}
	template<typename T>
	reference_wrapper<T> ref(reference_wrapper<T> val){
		return reference_wrapper<T>(val.get());
	}
	template<typename T>
	void ref(const T&&) = delete;
	
	template<typename T>
	reference_wrapper<const T> cref(const T& t){
		return reference_wrapper<const T>(t);
	}
	template<typename T>
	reference_wrapper<const T> cref(reference_wrapper<T> val){
		return reference_wrapper<const T>(val.get());
	}
	template<typename T>
	void cref(const T&&) = delete;
	
	
	//qword rdseed(void);
	//qword rdrand(void);
	
	
	template<typename A,typename B>
	struct pair{
		A first;
		B second;
		inline pair(const A& a,const B& b) : first(a),second(b){}
		inline pair(A&& a,B&& b) : first(move(a)),second(move(b)){}
	};

	
	//template<typename A,typename B>
	//inline pair<A, B> make_pair(const A& a, const B& b) {
	//	return pair<A, B>(a, b);
	//}
	template<typename A, typename B>
	inline pair<A, B> make_pair(A&& a, B&& b) {
		return pair<A, B>(move(a), move(b));
	}

	template<typename T>
	inline void swap(T& a,T& b){
		T t(move(a));
		a=move(b);
		b=move(t);
	}
	
	//template<typename T>
	//inline T& min(T& a,T& b){
	//	return (a<b) ? a : b;
	//}
	
	template<typename T>
	inline const T& min(const T& a,const T& b){
		return (a<b) ? a : b;
	}
	
	//template<typename T>
	//inline T& max(T& a,T& b){
	//	return (a>b) ? a : b;
	//}
	
	template<typename T>
	inline const T& max(const T& a,const T& b){
		return (a>b) ? a : b;
	}
	
	template<typename T>
	struct equal_to {
		bool operator()(const T& a, const T& b) const{
			return a == b;
		}
	};

	template<typename T>
	struct not_equal_to {
		bool operator()(const T& a, const T& b) const{
			return a != b;
		}
	};
	template<typename T>
	struct less {
		bool operator()(const T& a, const T& b) const{
			return a < b;
		}
	};
	
	template<typename T>
	size_t match(T a,T b,size_t cnt){
		size_t i=0;
		for (;i<cnt;i++){
			if (*a==*b)
				;
			else
				break;
			++a;
			++b;
		}
		return i;
	}

	template<typename It,typename T>
	It find(It head, It tail, const T& cmp) {
		while (head != tail) {
			if (*head == cmp)
				return head;
			++head;
		}
		return tail;
	}

	
	template<typename T>
	T align_down(T value,size_t align){
		return value & (T)~(align-1);
	}
	
	template<typename T>
	T align_up(T value,size_t align){
		return align_down((T)(value + align - 1),align);
	}
	
	template<typename T>
	class optional{		//simple and *buggy* implementation of C++17 std::optional
		alignas(T) byte buffer[sizeof(T)];
		bool valid;
	public:
		optional(void) : valid(false);
		optional(T&& other) {
			construct(move(other));
		}
		optional(const optional&) = delete;
		optional(optional&& other) : valid(other.valid){
			if (other.valid){
				construct(*other);
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
			if (!valid){
				new(buffer) T(forward<Arg>(args));
				valid = true;
			}
		}

		operator T*(void){
			return valid ? static_cast<T*>(buffer) : nullptr;
		}
		operator const T*(void) const{
			return valid ? static_cast<const T*>(buffer) : nullptr;
		}
		T* operator->(void){
			return operator T *();
		}
		const T* operator->(void) const{
			return operator const T *();
		}
		
	private:
		void construct(T&& other){
			if (!valid){
				new(buffer) T(move(other));
				valid = true;
			}
		}
		void destruct(void){
			if (valid){
				static_cast<T*>(buffer)->~T();
				valid = false;
			}
		}
	};
};
