#pragma once
#include "types.h"
#include "util.hpp"
#include "process/include/waitable.hpp"
#include "filesystem/include/instance.hpp"
#include "assert.hpp"

namespace UOS{
	class file : public stream{
		file_instance* const instance;
		process* const host;
		file* next = nullptr;
		byte iostate = 0;
		word command = 0;
		void* buffer = nullptr;
		dword length = 0;
		dword offset = 0;

		enum : byte {COMMAND_READ = 1,COMMAND_WRITE = 2};
		friend class FAT32;
	protected:
		//ins acquired before calling
		file(file_instance* ins,process* ps);
		file(const file&) = delete;
	public:
		virtual ~file(void);
		static file* open(const span<char>& path);
		static file* duplicate(file* f,process* p);

		OBJTYPE type(void) const override {
			return FILE;
		}
		IOSTATE state(void) const override {
			return (IOSTATE)iostate;
		}
		bool relax(void) override;
		bool check(void) override;
		REASON wait(qword us = 0,wait_callback = nullptr) override;
		dword result(void) const override;
		dword read(void* dst,dword length) override;
		dword write(void const* sor,dword length) override;
		virtual bool seek(size_t off);
		virtual size_t tell(void) const{
			return offset;
		}
	};

/*
	class basic_file : public stream{
	public:
		enum FILETYPE {UNKNOWN,FILE,DIRECTORY};
		enum ATTRIB : qword {READONLY = 1, HIDDEN = 2, SYSTEM = 4};
	private:
		struct hash{
			UOS::hash<literal> h;
			qword operator()(const basic_file& obj){
				return h(obj.name);
			}
			qword operator()(const literal& str){
				return h(str);
			}
		};
		struct equal{
			bool operator()(const basic_file& f,const literal& str){
				return f.name == str;
			}
		};
		literal const name;
	protected:
		basic_file(literal&& str) : name(move(str)) {}
		basic_file(const basic_file&) = delete;
	public:
		//TODO send 'erase' to file-manager when closing
		virtual ~basic_file(void) = default;
		OBJTYPE type(void) const override{
			return OBJTYPE::FILE;
		}
		IOSTATE state(void) const override{
			//TODO
			return (IOSTATE)0;
		}
		bool check(void) override{
			return false;
		}
		inline const literal& full_name(void) const{
			return name;
		}
		literal file_name(void) const;
		literal path_name(void) const;

		virtual FILETYPE file_type(void) const = 0;
		virtual qword attribute(void) const = 0;
		virtual size_t size(void) const = 0;
		virtual bool seek(size_t off) = 0;
		virtual size_t tell(void) const = 0;
	};

	class file_stub : public basic_file{
		byte* const base;
		dword const length;
		dword offset = 0;

		file_stub(literal&& filename,void*,dword);
	public:
		static file_stub* open(literal&& filename);
		FILETYPE file_type(void) const override;
		qword attribute(void) const override;
		size_t size(void) const override;
		bool seek(size_t off) override;
		size_t tell(void) const override;
		dword result(void) const override;
		dword read(void* dst,dword length) override;
		dword write(const void* sor,dword length) override;
	};
*/
}