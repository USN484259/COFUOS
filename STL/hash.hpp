#pragma once
#include "common.hpp"




namespace UOS{

	qword fasthash(const void*, size_t);


	template<typename T>
	qword hash(const T& obj) {
		return fasthash(addressof(obj), sizeof(const T));
	}
}


	/*
	qword hash(unsigned char);
	qword hash(signed char);
	qword hash(short);
	qword hash(unsigned short);
	qword hash(int);
	qword hash(unsigned int);
	qword hash(long);
	qword hash(unsigned long);
	qword hash(long long);
	qword hash(unsigned long long);
	qword hash(float);
	qword hash(double);
	qword hash(long double);

	template<typename T>
	qword hash(const T* ptr) {
		return fasthash(&ptr, sizeof(const T*));
	}
	*/

	/*
	template<typename T>
	struct hash {
		qword operator()(const T& obj) const{
			return obj.hash();
		}
	};
	template<typename T>
	struct hash<T*> {
		qword operator()(const T* ptr) const{
			return fasthash(&ptr, sizeof(const T*));
		}
	};
	
	template<> struct hash<unsigned char>;
	template<> struct hash<signed char>;
	template<> struct hash<short>;
	template<> struct hash<unsigned short>;
	template<> struct hash<int>;
	template<> struct hash<unsigned int>;
	template<> struct hash<long>;
	template<> struct hash<unsigned long>;
	template<> struct hash<long long>;
	template<> struct hash<unsigned long long>;
	template<> struct hash<size_t>;
	template<> struct hash<float>;
	template<> struct hash<double>;
	template<> struct hash<long double>;

	*/
