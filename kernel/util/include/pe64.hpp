#pragma once
#include "types.h"
#include "util.hpp"
namespace UOS{
	

	class PE64{
	public:
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

	public:
		struct directory{
			dword offset;
			dword size;
		};

		struct section{
			char name[8];
			dword size;
			dword offset;
			dword datasize;
			dword fileoffset;
			dword reserved[3];
			dword attrib;
		};
	private:
		
	public:
		PE64(void) = delete;
		//WARNING: not safe if header is incomplete
		directory const* get_directory(unsigned index) const;
		//WARNING: not safe if header is incomplete
		section const* get_section(unsigned index) const;
		//extract basic info from (possilby incomplete) PE header
		static PE64 const* construct(void const* module_base,size_t length);
	};
	extern PE64 const* pe_kernel;
}