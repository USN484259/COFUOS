#include "heap.hpp"
#include "util.hpp"
#include "assert.hpp"


using namespace UOS;

#ifndef _TEST

void* operator new(size_t len){
	if (!len)
		return nullptr;
	return syspool.allocate(len);

}

void* operator new(size_t,void* pos){
	return pos;
}


void operator delete(void* p,size_t len){
	if (!p)
		return;
	syspool.release(p,len);
}

void* operator new[](size_t len){
	dword* p=(dword*)operator new(len+sizeof(dword));
	*p=(dword)len;	//place size in the first dword of block
	return p+1;	//return the rest of block
}

void operator delete[](void* p){
	if (!p)
		return;
	dword* b=(dword*)p;
	--b;	//point back to the block head
	operator delete(b,*b);	//block size at head
}

#endif

//const heap::BLOCK heap::bitoff=5;
//const heap::BLOCK heap::nomem=16;


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

//get level of size ( size <= level )
//greater than 1M not supported
heap::BLOCK heap::category(size_t req){
	assertinv(0,req);
	if (req > align_size(nomem - 1))
		return nomem;

	BLOCK res = nomem - 1;
	do{
		if (req & ~align_mask(res)) {
			if (req & align_mask(res))
				res++;
			assertless(res, nomem);
			return res;
		}
	}while(res--);
	
	return 0;	//b32
	
}

//get level of given memory range
heap::BLOCK heap::category(void* p, size_t len) {
	BLOCK res = nomem;
	BLOCK cur = 0;
	qword base = reinterpret_cast<qword>(p);

	while (cur < nomem) {

		if (base & align_mask(cur))
			break;
		if (len < align_size(cur))
			break;

		res = cur;
		++cur;
	}

	return res;

}


void* heap::get(BLOCK index){
	assertless(index, nomem);
	node* cur = nullptr;
	if (pool[index]){
		cur=pool[index];
		assert(nullptr,cur->prev);
		assert(0,(reinterpret_cast<qword>(cur)) & align_mask(index));

		//pick the head node
		pool[index]=cur->next;
		if (pool[index])
			pool[index]->prev=nullptr;
#ifdef _DEBUG
		cur->prev=cur->next=nullptr;
#endif
		return cur;
	}
	if (index + 1 == nomem)
		return nullptr;

	cur = static_cast<node*>(get(index + 1));
	if (!cur)
		return nullptr;

	//cur = (node*)(res + align_size(index));

	//put back the latter block
	put(reinterpret_cast<byte*>(cur) + align_size(index), index);
	/*
	cur->next=pool[index];
	cur->prev=nullptr;
	if (pool[index])
		pool[index]->prev=cur;

	pool[index]=cur;
	*/
	return cur;
	
	
}

void heap::put(void* base,BLOCK index){
	assert(0,(reinterpret_cast<qword>(base)) & align_mask(index));
	
	node* block = static_cast<node*>(base);
	node* cur = pool[index];
	node* prev = nullptr;
	while (cur && cur < block) {
		assert(cur->prev, prev);
		prev = cur;
		cur = cur->next;
		//if ((reinterpret_cast<qword>(it) ^ reinterpret_cast<qword>(block)) == align_size(index)) {	//find a pair,migrate

		//}
	}
	assert((qword)(prev ? prev->next : cur), (qword)cur);
	assert((qword)(cur ? cur->prev : prev), (qword)prev);

	if (index + 1 < nomem) {
		node* friend_block = nullptr;
		if (prev && (reinterpret_cast<qword>(prev) ^ reinterpret_cast<qword>(block)) == align_size(index))
			friend_block = prev;
		if (cur && (reinterpret_cast<qword>(cur) ^ reinterpret_cast<qword>(block)) == align_size(index))
			friend_block = cur;

		if (friend_block){
			if (friend_block->next)
				friend_block->next->prev = friend_block->prev;
			if (friend_block->prev)
				friend_block->prev->next = friend_block->next;
			else {
				assert(friend_block, pool[index]);
				pool[index] = friend_block->next;
			}
			put(min(friend_block, block), index + 1);
			return;
		}
	}
	//now prev < block < cur
	block->prev = prev;
	block->next = cur;
	if (cur)
		cur->prev = block;
	if (prev)
		prev->next = block;
	else {
		assert(pool[index], cur);
		pool[index] = block;
	}
	/*
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
	*/
}



bool heap::expand(void* base,size_t len){
	if (base && len)
		;
	else
		return false;
	
	if (reinterpret_cast<qword>(base) & align_mask(0))	//not b32 aligned
		return false;

	lock_guard<mutex> lck(m);
	//assume page alignment
	
	//assert(0,reinterpret_cast<size_t>(base) & align_mask(15));	//m1
	
	byte* cur = static_cast<byte*>(base);
	while (len >= align_size(0)) {
		BLOCK level = category(cur,len);
		if (level == nomem)
			break;

		assertless(level, nomem);
		assertless(align_size(level), len + 1);
		assert(0, (reinterpret_cast<qword>(cur)) & align_mask(level));
		put(cur, level);
		cur += align_size(level);
		len -= align_size(level);
	}


	/*
	while(len>=align_size(0)){	//b32
		BLOCK cur=category(len);
		if (cur==nomem)
			cur--;
		
		put(base,cur);
		len-=align_size(cur);
		base=((byte*)base)+align_size(cur);
		
	}
	*/

	return true;	
}


size_t heap::capability(void) const {
	BLOCK res = nomem;
	for (BLOCK i = 0; i < nomem; i++) {
		if (pool[i])
			res = i;
	}
	return res == nomem ? 0 : align_size(res);
}


void* heap::allocate(size_t req){
	assertinv(0, req);
	if (req > align_size(15))	//m1
		return nullptr;

	BLOCK level = category(req);
	assertless(level, nomem);

	lock_guard<mutex> lck(m);

	return get(level);
	
}

void heap::release(void* base,size_t req){
	assertinv(nullptr, base);
	assertinv(0, req);
	//assertless(req, align_size(15) + 1);
	BLOCK level = category(req);
	assertless(level, nomem);

	lock_guard<mutex> lck(m);
	//assert(0,reinterpret_cast<size_t>(base) & align_mask(cur));
	put(base, level);

}