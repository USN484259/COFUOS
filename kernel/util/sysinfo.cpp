#include "sysinfo.hpp"

using namespace UOS;

void system_feature::set(system_feature::FEATURE index){
	state |= (qword)1 << index;
}

void system_feature::clear(system_feature::FEATURE index){
	state &= (qword)~(1 << index);
}

bool system_feature::get(system_feature::FEATURE index) const{
	return (state & ((qword)1 << index));
}