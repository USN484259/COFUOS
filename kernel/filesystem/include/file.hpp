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
		friend class exfat;
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
}