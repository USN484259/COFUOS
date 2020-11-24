#include "mp.hpp"
#include "sysinfo.hpp"
#include "lock.hpp"
#include "apic.hpp"
#include "atomic.hpp"

using namespace UOS;


MP::MP(void) : guard(0),owner(0xFFFF),command(0),count(0){}


void MP::lock(void)volatile{
	if (!this)
		return;
	word id=apic->id();
	word res=cmpxchg<word>(&owner,id,0xFFFF);
	if (res==id){
		assert(0 != guard);
		++guard;
		return;
	}
	if (res!=0xFFFF){
		do{
			_mm_pause();
		}while(cmpxchg<word>(&owner,id,0xFFFF) != 0xFFFF);
	}
	res=lockinc<word>(&guard);
	assert(1 == res);
}		

void MP::unlock(void)volatile{
	if (!this)
		return;
	assert(apic->id() == owner);
	assert(0 != guard);
	if (0==lockdec<word>(&guard)){
		xchg<word>(&owner,0xFFFF);
	}
		
}

bool MP::lock_state(void) const volatile{
	return (this && guard)?true:false;

}

void MP::sync(MP::CMD cmd,void* argu,size_t len)volatile{
	if (!this)
		return;
	lock_guard<volatile MP> lck(*this);
	assert(len < 0x0C00);
	word tmp=cmpxchg<word>(&count,1,0);
	assert(0 == tmp);
	if (argu && len)
		memcpy(this+1,argu,len);
	tmp = xchg<word>(&command,cmd);
	
	apic->mp_break();
	
	while(count != sysinfo->MP_cnt)
		_mm_pause();
	
	tmp=cmpxchg<word>(&count,0,sysinfo->MP_cnt);
	assert(sysinfo->MP_cnt == tmp);
	
}


void MP::reply(MP::CMD cmd)volatile{
	assert(0 != this);
	assert(0 != count);
	assert(command == cmd);
	assert(count < sysinfo->MP_cnt);
	lockinc<word>(&count);
}
