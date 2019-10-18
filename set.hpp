#pragma once
#include "types.hpp"
#include "assert.hpp"
#include "util.hpp"

namespace UOS{

template<typename T,template CMP = UOS::less<T> >
class set{
	
	struct node{
		qword depth:48;
		qword type_l:8;
		qword type_r:8;
		node* left;
		node* right;
		T payload;
		
	};
	
	
	
	
	
	
	
};



}