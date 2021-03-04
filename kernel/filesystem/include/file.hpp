#pragma once
#include "types.hpp"
#include "process/include/waitable.hpp"
#include "string.hpp"

namespace UOS{
	class basic_file : public waitable{
	public:
		enum TYPE {UNKNOWN,FILE,DIRECTORY};
		enum ATTRIB : qword {READONLY = 1, HIDDEN = 2, SYSTEM = 4};
	private:
		struct hash{
			UOS::hash<string> h;
			qword operator()(const basic_file& obj){
				return h(obj.name);
			}
			qword operator()(const string& str){
				return h(str);
			}
		};
		struct equal{
			bool operator()(const basic_file& f,const string& str){
				return f.name == str;
			}
		};
		string name;
	public:
		inline basic_file(const char* str) : name(str) {}
		basic_file(const basic_file&) = delete;
		//TODO send 'kill' to file-manager when closing
		virtual ~basic_file(void) = default;
		inline const string& full_name(void) const{
			return name;
		}
		string file_name(void) const;
		string path_name(void) const;

		virtual TYPE type(void) const = 0;
		virtual qword attribute(void) const = 0;
		virtual size_t size(void) const = 0;
		virtual bool seek(size_t off) = 0;
		virtual size_t tell(void) const = 0;
		virtual size_t read(void* dst,size_t length) = 0;
		virtual size_t write(const void* sor,size_t length) = 0;
	};

	class file_stub : public basic_file{
		size_t offset = 0;
	public:
		file_stub(void);
		TYPE type(void) const override;
		qword attribute(void) const override;
		size_t size(void) const override;
		bool seek(size_t off) override;
		size_t tell(void) const override;
		size_t read(void* dst,size_t length) override;
		size_t write(const void* sor,size_t length) override;
	};
}