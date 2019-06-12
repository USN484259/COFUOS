#include "mp.hpp"
#include "sysinfo.hpp"
#include "lock.hpp"
#include "apic.hpp"

using namespace UOS;


void MP::lock(void){
	while(_InterlockedCompareExchange16(&guard,1,0))
		_mm_pause();
	owner=apic->id();
}

void MP::unlock(void){
	assert(apic->id(),owner);
	word tmp=_InterlockedCompareExchange16(&guard,0,1);
	assert(tmp,1);
		
}

bool MP::lock_state(void) const{
	return guard?true:false;

}

void MP::sync(word cmd,void* argu,size_t len){
	lock_guard<MP> lck(*this);
	assertless(len,0x0C00);
	word tmp=_InterlockedCompareExchange16(&count,1,0);
	assert(0,tmp);
	if (argu && len)
		memcpy(this+1,argu,len);
	tmp = _InterlockedExchange16(&command,cmd);
	
	apic->mp_break();
	
	while(count != sysinfo->MP_cnt)
		_mm_pause();
	
	tmp=_InterlockedCompareExchange16(&count,0,sysinfo->MP_cnt);
	assert(sysinfo->MP_cnt,tmp);
	
}


void MP::reply(word cmd){
	assertinv(0,count);
	assert(command,cmd);
	assertless(count,sysinfo->MP_cnt);
	_InterlockedIncrement16(&count);
}
