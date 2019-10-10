#include "heap.hpp"
#include "util.hpp"
#include "assert.hpp"

using namespace UOS;


void* operator new(size_t len){
	return syspool.allocate(len);

}

void* operator new(size_t,void* pos){
	return pos;
}


void operator delete(void* p,size_t len){
	syspool.release(p,len);
}

void* operator new[](size_t len){
	dword* p=(dword*)operator new(len+sizeof(dword));
	*p=(dword)len;	//place size in the first dword of block
	return p+1;	//return the rest of block
}

void operator delete[](void* p){
	dword* b=(dword*)p;
	--b;	//point back to the block head
	operator delete(b,*b);	//block size at head
}

const heap::BLOCK heap::bitoff=5;
const heap::BLOCK heap::nomem=16;


heap::heap(void) : pool{nullptr} {}
heap::heap(void* base,size_t len) : pool{nullptr} {
	expand(base,len);
}


inline size_t heap::align_mask(BLOCK index){
	return ((size_t)1<<(index+bitoff))-1;
}

inline size_t heap::align_size(BLOCK index){
	return (qword)1<<(index+bitoff);
}

heap::BLOCK heap::category(size_t req){
	assertinv(0,req);
	BLOCK res=nomem;

	do{
		if (req & ~align_mask(res))
			return res;
	}while(res--);
	
	return 0;	//b32
	
}



void* heap::get(BLOCK index){
	if (index>=nomem)
		return nullptr;
	node* cur;
	if (pool[index]){
		cur=pool[index];
		assertinv(nullptr,cur->prev);
		assert(0,reinterpret_cast<size_t>(cur) & align_mask(index));
		pool[index]=cur->next;
		if (pool[index])
			pool[index]->prev=nullptr;
		#ifdef _DEBUG
		cur->prev=cur->next=nullptr;
		#endif
		return cur;
	}
	cur=static_cast<node*>(get(index+1));
	if (!cur)
		return nullptr;
	cur->next=pool[index];
	cur->prev=nullptr;
	if (pool[index])
		pool[index]->prev=cur;
	pool[index]=cur;
	
	return ((byte*)cur)+align_size(index);
	
	
}

void heap::put(void* base,BLOCK index){
	assert(0,reinterpret_cast<size_t>(base) & align_mask(index));
	
	node* cur=pool[index];
	while(cur){
		if ( ( reinterpret_cast<qword>(cur) ^ reinterpret_cast<qword>(base) ) == align_size(index)){
			put(min(cur,static_cast<node*>(base)),index+1);
			return;
		}
		cur=cur->next;
	}
	cur=static_cast<node*>(base);
	cur->prev=nullptr;
	cur->next=pool[index];
	pool[index]=cur;
}



bool heap::expand(void* base,size_t len){
	if (base && len)
		;
	else
		return false;
	
	lock_guard<mutex> lck(m);
	//assume page alignment
	
	assert(0,reinterpret_cast<size_t>(base) & align_mask(15));	//m1
	
	while(len>=align_size(0)){	//b32
		BLOCK cur=category(len);
		if (cur==nomem)
			cur--;
		
		put(base,cur);
		len-=align_size(cur);
		base=((byte*)base)+align_size(cur);
		
	}
	return true;	
	
}

void* heap::allocate(size_t req){
	if (req > align_size(15))	//m1
		return nullptr;
	
	lock_guard<mutex> lck(m);
		
	BLOCK cur=category(req);
	if (req & align_mask(cur))
		cur++;
	assertless(cur,nomem);
	return get(cur);
	
}

void heap::release(void* base,size_t req){
	//assert(req <= align_size(15));	//m1
	assertless(req,align_size(15)+1);
	
	lock_guard<mutex> lck(m);
	
	BLOCK cur=category(req);
	if (req & align_mask(cur))
		cur++;
	assertless(cur,nomem);
	assert(0,reinterpret_cast<size_t>(base) & align_mask(cur));
	put(base,cur);

}