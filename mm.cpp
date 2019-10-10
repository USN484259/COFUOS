#include "mm.hpp"
#include "ps.hpp"
#include "sysinfo.hpp"
#include "constant.hpp"
#include "util.hpp"
#include "bits.hpp"
#include "assert.hpp"
#include "mp.hpp"
#include "lock.hpp"
#include "atomic.hpp"

using namespace UOS;


PM::type PM::operator| (const PM::type& a,const PM::type& b){
	return (PM::type)((qword)a | b);
}

#pragma check_stack(off)
bool PM::spy(void* d,qword base,size_t len){
	byte* dst=(byte*)d;
	
	qword cur=base & ~PAGE_MASK;
	
	if (cur != base){
		//byte* buffer=new byte[PAGE_SIZE];
		byte buffer[PAGE_SIZE];
		VM::window w((void*)cur);
		w.read(buffer,0,PAGE_SIZE);
		size_t off=base-cur;
		memcpy(dst,buffer+off,min(PAGE_SIZE-off,len));
		cur+=PAGE_SIZE-off;
		dst+=PAGE_SIZE-off;
		
		//delete[] buffer;
		
		if (len>PAGE_SIZE-off)
			len-=PAGE_SIZE-off;
		else
			return true;
	}
	
	while(true){
		VM::window w((void*)cur);
		w.read(dst,0,min(len,PAGE_SIZE));
		cur+=PAGE_SIZE;
		dst+=PAGE_SIZE;
		if (len>PAGE_SIZE)
			len-=PAGE_SIZE;
		else
			break;
	}
	
	return true;
	
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
	
	memcpy(dst,(const void*)base,len);
	
	return true;
	
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

size_t VM::window::read(void* dst,size_t off,size_t len) const{
	if (off>=PAGE_SIZE)
		return 0;
	size_t read_length=min(PAGE_SIZE-off,len);
	memcpy(dst,vbase+off,read_length);
	return read_length;
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

#pragma warning(push)
#pragma warning(disable:4365)

void VM::VMG::lock(void)volatile{
	volatile byte* lck=(volatile byte*)this + 7;	//directly operate on sync bit
	
	while(true){
		volatile byte expected=*lck & (byte)~0x20;
		
		//if (expected == _cmpxchgb(lck,expected,expected | 0x20) )
		if (expected == cmpxchg<byte>(lck,expected | (byte)0x20,expected))
			break;
		_mm_pause();
	}
	assert(1,sync);
	
}

void VM::VMG::unlock(void)volatile{
	assert(1,sync);
	
	volatile byte* lck=(volatile byte*)this + 7;	//directly operate on sync bit
	
	byte expected= *lck | (byte)0x20;
	//expected=_cmpxchgb(lck,expected,expected & ~0x20);
	expected = cmpxchg<byte>(lck,expected & (byte)~0x20,expected);
	
	assertinv(0,expected & 0x20);

}
#pragma warning(pop)

void VM::VMG::construct(void){
#ifdef _DEBUG
	static bool init=false;
	assert(false,init);
#endif
	sys->index=0;		//first GB in sys area
	sys->highaddr=1;
	sys->offset=1;		//VMG info starts at 16-th page
	qword* pdt=(qword*)HIGHADDR(PDT0_PBASE);	//PDT of first sys area (this area)
	//VMG of sys area is placed at PDPT_HIGH_PBASE,whose first entrance is initialized during krnldr
	assert(HIGHADDR(sys->pmpdt<<12),(qword)pdt);
	
	qword* pt=(qword*)HIGHADDR(PT0_PBASE);
	
	//VMG info starts at 16-th page
	//pt[0x10]	GAP
	pt[0x11]=(qword)pdt | 3;	//PDT area

	byte* base=sys->base();		//LA of this GB
	
	byte* mapped_addr=base+0x10000;		// + 16 page offset,addr of GAP page
	
	//VM::window not available,do it manually
	//TRICK: use GAP page to map PA (simulate VM::window)

	for (size_t i=0;i<8;i++){
	
		using namespace PM;
		void* p=allocate(must_succeed);
		pt[0x10]=(qword)p | 3;	//map physical page to GAP page
		__invlpg(mapped_addr);
		zeromemory(mapped_addr,PAGE_SIZE);
		
		if (0==i)
			*(dword*)mapped_addr = 0x03FFFFFF;	//mask out used pages in bitmap (26 pages)
		
		pt[0x12+i]=(qword)p | 3;
	}
	
	pt[0x10]=0;		//clear GAP page
	__invlpg(mapped_addr);
#ifdef _DEBUG
	init=true;
#endif
}

//general constructor for 1G user or sys area
VM::VMG::VMG(bool k,word id) : present((MP_assert(true),0)),writable(1),user(1),writethrough(0),cachedisable(0),accessed(0),highaddr(k?1:0),largepage(0),offset(0),pmpdt(0),index((assertless(id,(word)512),id)),sync(0),xcutedisable(0) {
	using namespace PM;
	void* d=allocate(must_succeed);		//PDT page
	void* t=allocate(must_succeed);		//PT page
	{	//zeroing PDT and assign first PT
		VM::window w(d);
		w.zero();
		w.at<qword>(0) = (qword)t | 0x03;
		
	}	//now first 2M of this G can be mapped
	{
		//zeroing PT and put VMG info in
		VM::window w(t);
		w.zero();
		//w.at<qword>(0)	GAP
		
		w.at<qword>(1) = (qword)d | PAGE_PRESENT | PAGE_WRITE | PAGE_WT | PAGE_NX | (highaddr?PAGE_GLOBAL:0);	//PDT
		
		//first page of bitmap,mask out used area
		void* p=allocate(must_succeed);
		w.at<qword>(2) = (qword)p | PAGE_PRESENT | PAGE_WRITE | PAGE_WT | PAGE_NX | (highaddr?PAGE_GLOBAL:0);	//bitmap[0]
		{
			VM::window wp(p);
			wp.zero();
			wp.at<word>(0) = (word)0x3FF;	//used 10 pages
		}
		
		//fill the rest of bitmap
		for (size_t i=3;i<10;i++){
			p = allocate(zero_page | must_succeed);
			w.at<qword>(i) = (qword)p | PAGE_PRESENT | PAGE_WRITE | PAGE_WT | PAGE_NX | (highaddr?PAGE_GLOBAL:0);
		}
		
	}
	pmpdt=(qword)d >> 12;	//put the 1G page into PDPT
	present=1;	//activate it
}


bool VM::VMG::bitset(size_t off,size_t bitcount) volatile{
	assert(1,sync);
	assertless(off+bitcount , 0x8000*8);
	
	bits bmp(bitmap(),0x8000);
	
	
	for (size_t i=0;i<bitcount;i++){
		if (bmp.at(off+i))
			return false;
	}
	
	bmp.set(off,bitcount,1);
	
	return true;
	
	
}


bool VM::VMG::bitscan(size_t& res,size_t bitcount)volatile{
	assert(1,sync);
	
	bits bmp(bitmap(),0x8000);
	
	res=0;
	
	while(res<0x8000*8-bitcount){
		size_t cnt=0;
		while(cnt<bitcount){
			if (bmp.at(res+cnt))
				break;
			++cnt;
		}
		if (cnt==bitcount){	//found
			bmp.set(res,bitcount,1);
			return true;
		}
		
		//reset and continue
		res+=cnt+1;
		
		
	}
	
	return false;
	
	/*
	
	byte* bmp=bitmap();
	size_t res = req;	//req ? req : 0;
	size_t cnt = bitcount;
	size_t i = res/8;		//current byte
	//assertinv(0,res%8);		//???
	for (;i<0x8000;i++){
		if (cnt<8){		//single byte
			byte mask=BITMASK(cnt);
			
			if (bmp[i] & mask)
				;
			else{	//found
				cnt=0;
				break;
			}
			
			if (bitcount==cnt){		//less than 8 pages,try to find result at tail of a byte
				//for (size_t j=1;j<cnt;j++){
				for (size_t j=0;j<8-cnt;j++){	//n-bit mask needs 8-n times to move through a byte
					mask<<=1;
					res++;
					if (bmp[i] & mask)
						;
					else{	//found
						cnt=0;
						break;
					}
				}
				if (0==cnt)
					break;
				
			}
			
			//else bitcount more than 8,no,re-find
		}
		else{	//more than 8 pages remaining,find a '0' byte
			if (0==bmp[i]){
				if (cnt-=8)
					continue;
				else
					break;
			}
		}
		if (req)	//fixed address not available
			return false;
		
		//reset and continue to find
		cnt=bitcount;
		res=(i+1)*8;
	}
	if (cnt)	//not found
		return false;
	
	for (i=0;i<bitcount;i++){
		bmp[i+res/8]=0xFF;
	}
	
	bmp[i+res/8] |= BITMASK(bitcount%8)<<(res%8);
	
	req=res;
	
	return true;
	*/
}

void VM::VMG::bitclear(size_t off,size_t bitcount)volatile{
	assert(1,sync);
	assertless(off+bitcount , 0x8000*8);
	
	bits bmp(bitmap(),0x8000);
	
#ifdef _DEBUG
	for (size_t i=0;i<bitcount;i++){
		assertinv(0,bmp.at(off+i));
	}
#endif

	bmp.set(off,bitcount,0);

	
	/*
	
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
	*/
	
}


qword VM::VMG::PTE_set(volatile qword& dst,void* pm,qword attrib)volatile{
	assert(1,sync);
	MP_assert(true);
	
	assert(0,attrib & ~0x800000000000019F);
	assert(0,(qword)pm & 0xFFF0000000000FFF);
	
	//return _xchgq(dst,(qword)pm | attrib);
	return xchg<qword>(&dst,(qword)pm | attrib);
}



void* VM::VMG::reserve(void* fixbase,size_t pagecount)volatile{
	assertinv(0,pagecount);
	//assert(0,attrib & ~0x80000000000011E);
	page_assert(fixbase);
	
	
	lock_guard<volatile VMG> lck(*this);
	
	size_t off;
	
	if (fixbase){	//if given address available
		off=((qword)fixbase >> 12) & BITMASK(18);
		if (!bitset(off,pagecount))
			return nullptr;
	}
	else{		//find a VM area
		if (!bitscan(off,pagecount))
			return nullptr;
	}
	assertless(off+pagecount,8*0x8000);
	
	size_t cur=off;		//#-th page in this GB
	volatile qword* pdt=table();
	while(pagecount){
		if (pdt[cur/0x200] & 1)	//PT available
			;
		else{	//new PT
			assert(0,pdt[cur/0x200]);	//shall be empty slot
			using namespace PM;
			void* pm=allocate(zero_page | must_succeed);
			PTE_set(pdt[cur/0x200],pm,PAGE_PRESENT | PAGE_WRITE | PAGE_WT | PAGE_NX | (highaddr?PAGE_GLOBAL:0) );
			
		}
		
		assertinv(0,pdt[cur/200] & 0x000FFFFFFFFFF000);		//shall have PT's PM
		window w((void*)(pdt[cur/200] & 0x000FFFFFFFFFF000));	//map PT
		
		size_t border=(cur+0x200) & ~0x1FF;		//upper bound page # of current PT

		while (pagecount && cur<border){
			qword oldval = PTE_set(w.at<qword>(cur % 0x200),nullptr,PAGE_WRITE);	//not present
			
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
			
			PTE_set(w.at<qword>(off % 0x200),allocate(must_succeed),attrib);
			
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
	
	PTE_set(w.at<qword>(off % 0x200),pm,attrib);
}