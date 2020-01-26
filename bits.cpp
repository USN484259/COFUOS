#include "bits.hpp"
#include "assert.hpp"


using namespace UOS;

bits::bits(void* b,size_t l) : base((byte*)b),length(l){}

int bits::at(size_t off) const{
	assert(off < length*8);
	
	return base[off / 8] & ( 1 << (off % 8) );
	
}

void bits::at(size_t off,int state){
	assert(off < length*8);
	
	if (state){	//set
		base[off/8] |= ( 1 << (off % 8) );

	}
	else{	//clear
		base[off/8] &= ~( 1 << (off % 8) );
		
	}
	
}

void bits::set(size_t off,size_t len,int state){
	assert(off+len < length*8);
	while(len && off % 8){
		at(off,state);
		++off,--len;
	}
	while(len>=8){
		base[off/8]=state?(byte)0xFF:(byte)0;
		off+=8,len-=8;
	}
	
	while(len){
		at(off,state);
		++off,--len;
	}
	
}
