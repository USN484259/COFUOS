#include "types.hpp"
#include "IF_guard.hpp"

using namespace UOS;

IF_guard::IF_guard(void) : state (__readeflags() & 0x0200) {
	_disable();
}

IF_guard::~IF_guard(void){
	if (state)
		_enable();
}
