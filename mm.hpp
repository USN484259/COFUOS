#pragma once
#include "types.hpp"


namespace UOS{
	
	namespace PM{
		
		void* allocate(void);
		void release(void*);
	}
	
	
	class VMG{
		qword p:1;
		qword w:1;
		qword u:1;
		qword wt:1;
		qword cd:1;
		qword a:1;
		qword bmp_l:1;
		qword l:1;
		qword bmp_m:4;
		qword pdt:40;
		qword bmp_h:11;
		qword xd:1;
		

		
		word bmpraw(void) const;
		word bmpraw(word);
		qword bmp(void) const;
		qword bmp(qword);
	public:
		VMG(void);
		~VMG(void);
		qword reserve(size_t pagecount,qword attrib);
		void commit(qword base,size_t pagecount);
		void release(qword base,size_t pagecount);
		qword map(void* pm,qword attrib);
		void* unmap(qword vm);
		qword protect(qword base,size_t pagecount,qword attrib);	//attrib==0 means query
	};


}