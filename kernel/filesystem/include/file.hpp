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
		qword offset = 0;
		void* buffer = nullptr;
		dword length = 0;
		volatile word iostate = 0;
		volatile word command = 0;

		enum : byte {COMMAND_READ = 1,COMMAND_WRITE = 2,COMMAND_LIST = 3};
		friend class exfat;
	protected:
		//ins acquired before calling
		file(file_instance* ins,process* ps);
		file(const file&) = delete;
		// cur should be acquired, relaxed inside
		static file_instance* imp_open(folder_instance* cur,const span<char>& path,byte mode);
	public:
		virtual ~file(void);
		static file* open(const span<char>& path,byte mode);
		static folder_instance* open_wd(folder_instance* cwd,const span<char>& path);
		file* duplicate(process* ps) override;

		OBJTYPE type(void) const override {
			return OBJ_FILE;
		}
		byte state(void) const override {
			return iostate;
		}
		bool relax(void) override;
		bool check(void) override;
		REASON wait(qword us = 0,wait_callback = nullptr) override;
		dword result(void) const override;
		dword read(void* dst,dword length) override;
		dword write(void const* sor,dword length) override;
		virtual bool seek(qword off);
		virtual qword tell(void) const{
			return offset;
		}
		qword size(void) const;
		string get_path(void) const;
	};
}