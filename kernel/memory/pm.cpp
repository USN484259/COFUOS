#include "pm.hpp"
//#include "ps.hpp"
#include "sysinfo.hpp"
#include "constant.hpp"
#include "util.hpp"
#include "bits.hpp"
#include "assert.hpp"
//#include "mp.hpp"
#include "lock_guard.hpp"
#include "atomic.hpp"
#include "vm.hpp"

using namespace UOS;

/*
PM::allocate_type UOS::operator| (const PM::allocate_type& a,const PM::allocate_type& b){
	return (PM::allocate_type)(a | b);
}
*/

PM::PM(void) : chunk(nullptr),layout(nullptr){}

PM::chunk_info::chunk_info(void) : index(0),avl_count(0),avl_head(0),present(0){}



void PM::construct(const void* scan){
#ifdef _DEBUG
	static bool init=false;
	assert(false == init);
#endif

	
	
	const PMMSCAN* it=(const PMMSCAN*)scan;
	qword* qmp=(qword*)sysinfo->PMM_qmp_vbase;
	
	while(it->type){
		if (it->base < 0x200000 && 1!=it->type){
			qword cur=it->base;
			qword lim=it->base+it->length;
			
			while(cur < lim){
				
				assert(cur < 0x200000);
				
				assert(0 == qmp[cur >> 12]);
				
				cur=(cur+PAGE_SIZE) & ~0x0FFF;
			}
			
		}
		
		
		++it;
	}
	
	assert(sizeof(PMMSCAN)*(it-(const PMMSCAN*)scan+1) < 0x1000);
	
	pmm.layout=new PMMSCAN[it-(const PMMSCAN*)scan+1];

	memcpy(pmm.layout,scan,sizeof(PMMSCAN)*(it-(const PMMSCAN*)scan+1) );
	
	
	pmm.chunk=new chunk_info[sysinfo->PMM_qmp_page];
	
	for (word i=0;i<sysinfo->PMM_qmp_page;i++)
		pmm.chunk[i].index=i;
	
	
	//construct first chunk
	
	qword* avl_prev=nullptr;
	
	for (unsigned i=0;i<PAGE_SIZE/sizeof(qword);++i){
		if ( qmp[i] & BIT(63) ){
			assert(BIT(63) == qmp[i]);
			
			if (avl_prev){
				assert(BIT(63) == qmp[i]);
				*avl_prev = (qword)(qmp+i);
			}
			else{
				pmm.chunk->avl_head=i;
			}
			avl_prev=qmp+i;
			pmm.chunk->avl_count++;
		}
	}

	pmm.chunk->present=1;
	
#ifdef _DEBUG
	init=true;
#endif
	
}



bool PM::spy(void* d,qword base,size_t len){
	byte* dst=(byte*)d;
	
	qword cur=base & ~PAGE_MASK;
	
	if (cur != base){
		//byte* buffer=new byte[PAGE_SIZE];
		byte* buffer=new byte[PAGE_SIZE];
		VM::window w((void*)cur);
		w.read(buffer,0,PAGE_SIZE);
		size_t off=base-cur;
		memcpy(dst,buffer+off,min(PAGE_SIZE-off,len));
		cur+=PAGE_SIZE-off;
		dst+=PAGE_SIZE-off;
		
		delete[] buffer;
		
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



void* PM::legacy_allocate(volatile void* va){

	assert(nullptr == chunk);
	assert(nullptr != va);
	volatile qword* qmp=(qword*)sysinfo->PMM_qmp_vbase;
	for (unsigned i=0;i<PAGE_SIZE/sizeof(qword);i++){
		qword cur=*(qmp+i);
		if (cur & BIT(63)){
			qword marked = BITMASK(36) & ( (qword)va >> 12 );
			if (cur == cmpxchg<qword>(qmp+i,marked,cur)){
				return (void*)((qword)i*PAGE_SIZE);
				
			}
			
		}
	}
	BugCheck(bad_alloc,qmp);
	
}

void PM::legacy_release(volatile void* ptr){
	//shall not used
	BugCheck(not_implemented,ptr);
	assert(nullptr == chunk);
	assert(0 == (qword)ptr & 0x0FFF);
	size_t off=(qword)ptr >> 12;
	assert(off < PAGE_SIZE / sizeof(qword));
	
	qword marked = xchg<qword>((qword*)sysinfo->PMM_qmp_vbase + off,BIT(63));
	assert(0 != marked & BITMASK(36));
	assert(0 == marked & ~BITMASK(36));
	
}




void* PM::allocate(volatile void* va,dword t){
	assert(nullptr != va);
	if (nullptr == chunk){
		assert(0 != t & must_succeed);
		assert(0 == t & zero_page);
		return legacy_allocate(va);
	}
	
	BugCheck(not_implemented,va);
	
	/*
	volatile word* base=(word*)sysinfo->PMM_qmp_vbase;
	size_t cnt=sysinfo->PMM_qmp_page*PAGE_SIZE/sizeof(word);
	
	word id=PS::id();
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
	
	//TODO swap pages if possible
	BugCheck(bad_alloc,0,(qword)t);
	*/
}



void PM::release(volatile void* ptr){
	assert(0 == (qword)ptr & 0x0FFF);
	if (nullptr==chunk){
		return legacy_release(ptr);
	}
	
	BugCheck(not_implemented,(qword)ptr);
	
	/*
	
	size_t off=(qword)p >> 12;
	assertless (off,sysinfo->PMM_wmp_page*PAGE_SIZE/sizeof(word));
	
	word id=PS::id();
	
	//self=_cmpxchgw((word*)sysinfo->PMM_wmp_vbase+off,self,(word)0x8000);
	word tmp = cmpxchg<word>((word*)sysinfo->PMM_wmp_vbase+off,0x8000,id);
	assert(id,tmp);
	*/
}


