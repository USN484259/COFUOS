#pragma once
#include "types.hpp"

namespace UOS{
	

	class PE64{
		//const byte* const img_base;
		
		
		struct _info{
			dword signature;
			word machine;
			word section_count;
			dword time_stamp;
			dword symbol_info[2];
			word size_optheader;
			word img_type;
			
			word magic;
			word linker_version;
			dword reserved1[3];
			dword entrance;
			dword codebase;
			qword imgbase;
			dword align_section;
			dword align_file;
			dword versions[4];
			
			dword imgsize;
			dword header_size;
			dword checksum;
			word subsystem;
			word attrib;
			qword stk_reserve;
			qword stk_commit;
			qword heap_reserve;
			qword heap_commit;
			dword reserved2;
			dword dir_count;
			struct{
				dword offset;
				dword size;
			}directory[0x10];
			
		}const* const pe_info;
		
		struct _section{
			char name[8];
			dword size;
			dword offset;
			dword datasize;
			dword fileoffset;
			dword reserved[3];
			dword attrib;
		}const* const pe_section;
		
		friend class section_iterator;
		
	public:
		class section_iterator{
			const PE64& pe;
			size_t it;
		
			friend class PE64;
			
			section_iterator(const PE64&);
			
			const _section* entrance(void) const;
			
		public:
			bool next(void);
			
			const char* name(void) const;
			qword base(void) const;
			dword size(void) const;
			
			
			
		};
	
	public:
		PE64(const void*);
		const void* base(void) const;
		dword header_size(void) const;
		//const void* section(const char*) const;
		section_iterator section(void) const;
		pair<dword,dword> stack(void) const;
	};
	
	//void* peGetSection(const void* imgBase,const char* name);
}