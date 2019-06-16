#include "mp.hpp"
#include "sysinfo.hpp"
#include "lock.hpp"
#include "apic.hpp"

using namespace UOS;


MP::MP(void) : guard(0),owner(0xFFFF),command(0),count(0){}


void MP::lock(void){
	word id=apic->id();
	word res=_InterlockedCompareExchange16(&owner,id,0xFFFF);
	if (res==id){
		assertinv(0,guard);
		++guard;
		return;
	}
	if (res!=0xFFFF){
		do{
			_mm_pause();
		}while(_InterlockedCompareExchange16(&owner,id,0xFFFF) != 0xFFFF);
	}
	res=_InterlockedIncrement16(&guard);
	assert(1,res);
}		

void MP::unlock(void){
	assert(apic->id(),owner);
	assertinv(0,guard);
	if (0==_InterlockedDecrement16(&guard)){
		_InterlockedExchange16(&owner,0xFFFF);
	}
		
}

bool MP::lock_state(void) const{
	return guard?true:false;

}

void MP::sync(MP::CMD cmd,void* argu,size_t len){
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


void MP::reply(MP::CMD cmd){
	assertinv(0,count);
	assert(command,cmd);
	assertless(count,sysinfo->MP_cnt);
	_InterlockedIncrement16(&count);
}
