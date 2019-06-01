#include "mm.hpp"
#include "ps.hpp"
#include "sysinfo.hpp"
#include "util.hpp"
#include "assert.hpp"

using namespace UOS;

extern SYSINFO* sysinfo;


void* PM::allocate(void){
	word* base=(word*)sysinfo->PMM_wmp_vbase;
	size_t cnt=sysinfo->PMM_wmp_page*2048;
	
	while(cnt--){
		word cur=*base;
		if (cur & 0x8000){
			if (cur==_cmpxchgw(base,cur,PS::id()))
				return (void*)((qword)(base-(word*)sysinfo->PMM_wmp_vbase)*0x1000);
		}

		base++;
	}
	return nullptr;
}

void PM::release(void* p){
	assert(0,(qword)p & 0xFFF);
	size_t off=(qword)p >> 12;
	assertless (off,sysinfo->PMM_wmp_page*2048);
	
	word self=PS::id();
	self=_cmpxchgw((word*)sysinfo->PMM_wmp_vbase+off,self,(word)0x8000);
	assert(self,PS::id());
}


VMG::VMG(void) : p(1),w(1),u(1),wt(0),cd(0),a(0),bmp_l(0),l(0),bmp_m(0),pdt(0),bmp_h(0),xd(0) {
	void* pm=PM::allocate();
	if (!pm)	//TODO exhg page if possible
		BugCheck(bad_alloc,reinterpret_cast<qword>(this),0);
	pdt=(qword)pm >> 12;
	{
		VM::window(pm);
		
		
		
		
	}
	
}


word VMG::bmpraw(void) const{
	return bmp_l | (bmp_m<<1) | (bmp_h << 5);
}

word VMG::bmpraw(word val){
	word old = bmp_l | (bmp_m<<1) | (bmp_h << 5);
	bmp_l=val & 1;
	bmp_m=(val>>1) & 0x0F;
	bmp_h=val>>5;
	return old;
}

qword VMG::bmp(void) const{
	return (qword)bmpraw() << (12+2);
	
}

qword VMG::bmp(qword val){
	assert(0,val & 0x3FFF);
	return (qword)bmpraw(val>>(12+2)) <<(12+2);
	
	
}

