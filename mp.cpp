#include "mp.hpp"
#include "sysinfo.hpp"
#include "apic.hpp"

using namespace UOS;


void MP::lock(void){
	while(_InterlockedCompareExchange16(&command,0xFFFF,0))
		_mm_pause();
	
	assert(0,count);
	
	trigger();
	
	assert(0,command);
}

void MP::unlock(void){
	word tmp=_InterlockedCompareExchange16(&count,0,sysinfo->MP_cnt);
	assert(tmp,sysinfo->MP_cnt);
	
	_InterlockedExchange16(&command,0);
	
}

bool MP::lock_state(void) const{
	return command || count;

}


void MP::reply(void){
	assertinv(0,command);
	word tmp=_InterlockedIncrement16(&count);
	if (tmp==sysinfo->MP_cnt){
		tmp=_InterlockedExchange16(&command,0);
		assertinv(0,tmp);
	}
	else
		assertless(tmp,sysinfo->MP_cnt);
}

void MP::trigger(void){
	assertinv(0,command);
	_InterlockedExchange16(&count,1);
	
	apic->mp_break();
	
	while(command)
		_mm_pause();
	
	assert(sysinfo->MP_cnt,count);
}