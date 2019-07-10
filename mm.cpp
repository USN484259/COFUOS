#include "mm.hpp"
#include "ps.hpp"
#include "sysinfo.hpp"
#include "constant.hpp"
#include "util.hpp"
#include "assert.hpp"
#include "mp.hpp"
#include "lock.hpp"
#include "atomic.hpp"

using namespace UOS;


PM::type PM::operator| (const PM::type& a,const PM::type& b){
	return (PM::type)((qword)a | b);
}

bool PM::spy(void* dst,qword base,size_t len){
	//TODO
	
	return false;
	
}

void* PM::allocate(type t){
	volatile word* base=(word*)sysinfo->PMM_wmp_vbase;
	size_t cnt=sysinfo->PMM_wmp_page*PAGE_SIZE/2;
	
	word id=PS::id();
	//TODO currently zero page not supported
	while(cnt--){
		word cur=*base;
		if (cur & 0x8000){
			//if (cur==_cmpxchgw(base,cur,PS::id())){
			if (cur == cmpxchg<word>(base,id,cur)){
				void* ret=(void*)((qword)(base-(word*)sysinfo->PMM_wmp_vbase)*PAGE_SIZE);
				if (t & zero_page)
					VM::window(ret).zero();
				return ret;
			}
		}

		base++;
	}
	if (t & must_succeed)
		;
	else
		return nullptr;
	
	//TODO xchg pages if possible
	BugCheck(bad_alloc,0,(qword)t);

}

void PM::release(void* p){
	assert(0,(qword)p & 0xFFF);
	size_t off=(qword)p >> 12;
	assertless (off,sysinfo->PMM_wmp_page*2048);
	
	word id=PS::id();
	
	//self=_cmpxchgw((word*)sysinfo->PMM_wmp_vbase+off,self,(word)0x8000);
	word tmp = cmpxchg<word>((word*)sysinfo->PMM_wmp_vbase+off,0x8000,id);
	assert(id,tmp);
}



bool VM::spy(void* dst,qword base,size_t len){
	//TODO
	
	return false;
	
}


VM::window::window(void* pm){
	vbase=(byte*)sys->reserve(nullptr,1);
	sys->map(vbase,pm,PAGE_WRITE | PAGE_NX);
}

VM::window::~window(void){
	sys->release(vbase,1);
}

void VM::window::zero(void){
	zeromemory(vbase,0x1000);
}


byte* VM::VMG::base(void)const volatile{
	return (byte*)(highaddr*HIGHADDR(0)+index*0x40000000);

}

qword* VM::VMG::table(void)const volatile{
	return (qword*)(base()+0x1000*(offset*0x10+1));
	
}

byte* VM::VMG::bitmap(void)const volatile{
	return base()+0x1000*(offset*0x10+2);
}

void VM::VMG::lock(void)volatile{
	volatile byte* lck=(volatile byte*)this + 7;
	
	while(true){
		volatile byte expected=*lck & ~0x20;
		
		//if (expected == _cmpxchgb(lck,expected,expected | 0x20) )
		if (expected == cmpxchg<byte>(lck,expected | 0x20,expected))
			break;
	
	}
	assert(1,sync);
	
}

void VM::VMG::unlock(void)volatile{
	assert(1,sync);
	
	volatile byte* lck=(volatile byte*)this + 7;
	
	byte expected= *lck | 0x20;
	//expected=_cmpxchgb(lck,expected,expected & ~0x20);
	expected = cmpxchg<byte>(lck,expected & ~0x20,expected);
	
	assertinv(0,expected & 0x20);

}

void VM::VMG::construct(void){
#ifdef _DEBUG
	static bool init=false;
	assert(false,init);
#endif
	sys->index=0;
	sys->highaddr=1;
	sys->offset=1;
	qword* pdt=(qword*)HIGHADDR(PDT0_PBASE);
	assert(HIGHADDR(sys->pmpdt<<12),(qword)pdt);
	
	qword* pt=(qword*)HIGHADDR(PT0_PBASE);
	
	pt[0x11]=(qword)pdt | 3;

	byte* base=sys->base();
	
	byte* mapped_addr=base+0x10000;

	for (size_t i=0;i<8;i++){
	
		using namespace PM;
		void* p=allocate(must_succeed);
		pt[0x10]=(qword)p | 3;
		__invlpg(mapped_addr);
		zeromemory(mapped_addr,PAGE_SIZE);
		
		if (0==i)
			*(dword*)mapped_addr = 0x03FFFFFF;
		
		pt[0x12+i]=(qword)p | 3;
	}
	
	pt[0x10]=0;
	__invlpg(mapped_addr);
#ifdef _DEBUG
	init=true;
#endif
}


VM::VMG::VMG(bool k,word id) : present((MP_assert(true),0)),writable(1),user(1),writethrough(0),cachedisable(0),accessed(0),highaddr(k),largepage(0),offset(0),pmpdt(0),index((assertless(id,512),id)),sync(0),xcutedisable(0) {
	using namespace PM;
	void* d=allocate(must_succeed);
	void* t=allocate(must_succeed);
	{
		VM::window w(d);
		w.zero();
		w.at<qword>(0) = (qword)t | 0x03;
		
	}
	{
		
		VM::window w(t);
		w.zero();
		w.at<qword>(1) = (qword)d | PAGE_PRESENT | PAGE_WRITE | PAGE_WT | PAGE_NX | (highaddr?PAGE_GLOBAL:0);
		
		void* p=allocate(must_succeed);
		w.at<qword>(2) = (qword)p | PAGE_PRESENT | PAGE_WRITE | PAGE_WT | PAGE_NX | (highaddr?PAGE_GLOBAL:0);
		{
			VM::window wp(p);
			wp.zero();
			wp.at<word>(0) = (word)0x3FF;
		}
		
		for (size_t i=3;i<10;i++){
			p = allocate(zero_page | must_succeed);
			w.at<qword>(i) = (qword)p | PAGE_PRESENT | PAGE_WRITE | PAGE_WT | PAGE_NX | (highaddr?PAGE_GLOBAL:0);
		}
		
	}
	pmpdt=(qword)d >> 12;
	present=1;
}



bool VM::VMG::bitscan(size_t& req,size_t bitcount)volatile{
	assert(1,sync);
	byte* bmp=bitmap();
	size_t res = req ? req : 0;
	size_t cnt = bitcount;
	size_t i = res/8;
	assertinv(0,res%8);
	for (;i<0x8000;i++){
		if (cnt<8){
			byte mask=BITMASK(cnt);
			
			if (bmp[i] & mask)
				;
			else{
				cnt=0;
				break;
			}
			
			if (bitcount==cnt){
				for (size_t j=1;j<cnt;j++){
					mask<<=1;
					res++;
					if (bmp[i] & mask)
						;
					else{
						cnt=0;
						break;
					}
				}
				if (0==cnt)
					break;
				
			}
		}
		else{
			if (0==bmp[i]){
				if (cnt-=8)
					continue;
				else
					break;
			}
		}
		if (req)
			return false;
		
		cnt=bitcount;
		res=(i+1)*8;
	}
	if (cnt)
		return false;
	
	for (i=0;i<bitcount;i++){
		bmp[i+res/8]=0xFF;
	}
	
	bmp[i+res/8] |= BITMASK(bitcount%8)<<(res%8);
	
	req=res;
	
	return true;
	
}

void VM::VMG::bitclear(size_t off,size_t bitcount)volatile{
	assert(1,sync);
	assertless(off+bitcount , 0x8000*8);
	
	byte* bmp=bitmap();
	
	if (off & 7){	//misaligned heading
		byte mask=~BITMASK(off & 7);
		
		assert(mask,bmp[off/8] & mask);
		
		bmp[off/8] &= ~mask;
		
	}
	assert(0,off & 7);
	while(bitcount>=8){
		assert(0xFF,bmp[off/8]);
		
		bmp[off/8]=0;
		off+=8;
		bitcount-=8;
		
	}
	
	
	if (bitcount){	//misaligned tailing
		byte mask=BITMASK(bitcount);
		
		assert(mask,bmp[off/8] & mask);
		
		bmp[off/8] &= ~mask;
		
	}
	
	
}


qword VM::VMG::PTE_set(volatile qword* dst,void* pm,qword attrib)volatile{
	assert(1,sync);
	MP_assert(true);
	
	assert(0,attrib & ~0x800000000000019F);
	assert(0,(qword)pm & 0xFFF0000000000FFF);
	
	//return _xchgq(dst,(qword)pm | attrib);
	return xchg<qword>(dst,(qword)pm | attrib);
}



void* VM::VMG::reserve(void* fixbase,size_t pagecount)volatile{
	assertinv(0,pagecount);
	//assert(0,attrib & ~0x80000000000011E);
	page_assert(fixbase);
	
	size_t off=fixbase ? ((qword)fixbase >> 12) & BITMASK(18) : 0;
	
	lock_guard<volatile VMG> lck(*this);
	
	if (!bitscan(off,pagecount))
		return nullptr;
	
	assertless(off+pagecount,8*0x8000);
	
	size_t cur=off;
	volatile qword* pdt=table();
	while(pagecount){
		if (pdt[cur/0x200] & 1)
			;
		else{	//new PT
			assert(0,pdt[cur/0x200]);
			using namespace PM;
			void* pm=allocate(zero_page | must_succeed);
			PTE_set(pdt+(cur/0x200),pm,PAGE_PRESENT | PAGE_WRITE | PAGE_WT | PAGE_NX | (highaddr?PAGE_GLOBAL:0) );
			
		}
		
		assertinv(0,pdt[cur/200] & 0x000FFFFFFFFFF000);
		window w((void*)(pdt[cur/200] & 0x000FFFFFFFFFF000));
		
		size_t border=(cur+0x200) & ~0x1FF;

		while (pagecount && cur<border){
			qword oldval = PTE_set(&w.at<qword>(cur % 0x200),nullptr,PAGE_WRITE);
			
			assert(0,oldval);
			
			pagecount--,cur++;
		}
		
	}
	
	
	return (void*)(base()+0x1000*off);
}

void VM::VMG::release(void* base,size_t pagecount)volatile{
	assertinv(0,pagecount);
	page_assert(base);
	assertinv(nullptr,base);
	
	lock_guard<volatile VMG> lck(*this);
	
	size_t off=((qword)base >> 12) & BITMASK(18);

	bitclear(off,pagecount);
	
	volatile qword* pdt=table();
	while(pagecount){
		assertinv(0,pdt[off/200] & 1);
		assertinv(0,pdt[off/200] & 0x000FFFFFFFFFF000);
		
		window w((void*)(pdt[off/200] & 0x000FFFFFFFFFF000));
		
		size_t border = (off+0x200) & ~0x1FF;
		
		while(pagecount && off < border){
			qword cur=xchg<qword>(&w.at<qword>(off % 0x200),0);
			assertinv(0,cur & 1);
			
			if (cur & 0x4000000000000000){	//shall release PM
				PM::release((void*)(cur & 0x000FFFFFFFFFF000));
			}
			
			
			pagecount--,off++;
		}
		
	}
	
}

void VM::VMG::commit(void* base,size_t pagecount,qword attrib)volatile{
	assertinv(0,pagecount);
	page_assert(base);
	assertinv(nullptr,base);
	
	//assert(0,attrib & 0x800000000000011F);
	attrib = (attrib & 0x800000000000011F) | PAGE_PRESENT | PAGE_COMMIT;
	if (highaddr)
		attrib &= ~PAGE_USER;
	
	lock_guard<volatile VMG> lck(*this);
	
	size_t off=((qword)base >> 12) & BITMASK(18);

	volatile qword* pdt=table();
	while(pagecount){
		assertinv(0,pdt[off/200] & 1);
		assertinv(0,pdt[off/200] & 0x000FFFFFFFFFF000);
		
		window w((void*)(pdt[off/200] & 0x000FFFFFFFFFF000));
		
		size_t border = (off+0x200) & ~0x1FF;
		
		while(pagecount && off < border){
			qword cur = w.at<qword>(off % 0x200);
			
			assertinv(0,cur);
			assert(0,cur & 1);
			
			using namespace PM;
			
			PTE_set(&w.at<qword>(off % 0x200),allocate(must_succeed),attrib);
			
			pagecount--,off++;
		}
	}
}

void VM::VMG::map(void* vbase,void* pm,qword attrib)volatile{
	page_assert(vbase);
	page_assert(pm);
	assertinv(nullptr,vbase);
	assertinv(nullptr,pm);
	
	attrib = (attrib & 0x800000000000011F) | 1;
	if (highaddr)
		attrib &= ~PAGE_USER;

	lock_guard<volatile VMG> lck(*this);
	size_t off=((qword)vbase >> 12) & BITMASK(18);
	volatile qword* pdt=table();
	
	assertinv(0,pdt[off/200] & 1);
	assertinv(0,pdt[off/200] & 0x000FFFFFFFFFF000);
	
	window w((void*)(pdt[off/200] & 0x000FFFFFFFFFF000));
	
	
	qword cur = w.at<qword>(off % 0x200);
	
	assertinv(0,cur);
	assert(0,cur & 1);
	
	PTE_set(&w.at<qword>(off % 0x200),pm,attrib);
}