#include "exception.hpp"


using namespace UOS;


exception::exception(const char* s) : msg(s){}

exception::exception(void) : msg("unknown"){}

exception::~exception(void){}

const char* exception::what(void) const{
	return msg;
}

exception& exception::operator=(const exception& sor){
	msg=sor.msg;
	return *this;
}

out_of_range::out_of_range(void) : exception("out of range"){}