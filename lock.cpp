#include "lock.hpp"
#include "assert.hpp"
using namespace UOS;

extern "C" qword IF_set(qword);


IF_guard::IF_guard(void) : state(IF_set(0)){
	IF_assert(0);
}

IF_guard::~IF_guard(void){
	IF_set(state);
	IF_assert(state);
}

bool IF_guard::status(void) const{
	return state?true:false;
}

