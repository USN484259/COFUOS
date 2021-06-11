#pragma once
#include "types.h"
#include "sync/include/rwlock.hpp"
#include "assert.hpp"
#include "literal.hpp"
#include "span.hpp"
#include "hash_set.hpp"
#include "exfat.hpp"

namespace UOS{
	class folder_instance;
	class file_instance{
		//friend class folder_instance;
		friend class instance_wrapper;
	protected:
		mutable rwlock objlock;
		dword ref_count = 1;
		folder_instance* parent;
		literal name;
		qword valid_size = 0;
		qword alloc_size = 0;
		dword rec_index = 0;
		word name_hash;
		byte attribute = 0;	//file attributes
		byte access = 0;	//share_read, share_write
		cluster_chain clusters;

	protected:
		struct instance_wrapper{
			file_instance* instance;

			instance_wrapper(file_instance* ins) : instance(ins){}
			instance_wrapper(const instance_wrapper&) = default;
			operator file_instance*(void){
				return instance;
			}
			file_instance* operator->(void){
				return instance;
			}
			struct hash{
				qword operator()(const instance_wrapper& obj){
					return obj.instance->name_hash;
				}
				template<typename T>
				qword operator()(const T& str){
					return exfat::name_hash(str);
				}
			};
			struct equal{
				template<typename T>
				bool operator()(const instance_wrapper& ins,const T& str){
					return exfat::name_equal(ins.instance->name,str);
				}
			};
		};
		void imp_get_path(string& str) const;
	public:
		// X locked after construction
		file_instance(exfat& fs,folder_instance* top,literal&& str,dword index,const exfat::record* file);
		// X locked before destruction
		virtual ~file_instance(void);
		
		virtual bool is_folder(void) const{
			assert(0 == (attribute & FOLDER));
			return false;
		}
		void acquire(void);
		void relax(void);

		inline void lock(rwlock::MODE mode){
			objlock.lock(mode);
		}
		inline bool try_lock(rwlock::MODE mode){
			return objlock.try_lock(mode);
		}
		inline void unlock(void){
			assert(objlock.is_locked());
			objlock.unlock();
		}
		inline bool is_locked(void) const{
			return objlock.is_locked();
		}

		inline qword get_size(void) const{
			assert(is_locked());
			return alloc_size;
		}
		inline qword get_valid_size(void) const{
			assert(is_locked());
			return valid_size;
		}
		inline const literal& get_name(void) const{
			assert(is_locked());
			return name;
		}
		inline byte get_attribute(void) const{
			assert(is_locked());
			return attribute;
		}
		inline folder_instance* get_parent(void) const{
			assert(is_locked());
			return parent;
		}

		qword get_lba(qword offset,bool expand = false);
		void set_size(qword vs,qword fs);

		void get_path(string& str) const;

		bool rename(const span<char>& str);
		bool resize(qword sz);
		bool move_to(folder_instance* target);
		bool remove(void);
	};
	class folder_instance : public file_instance{

		hash_set<instance_wrapper,instance_wrapper::hash,instance_wrapper::equal> file_table;

	public:
		class reader{
			struct record{
				byte data[0x20];
			};
			struct name_part{
				byte type;
				byte zero;
				word str[0x0F];
			};
			folder_instance& inst;
			vector<record> list;
			dword index;

		public:
			enum : byte { CHECK_HASH = 0x40 };
		public:
			reader(folder_instance& ins,dword i = 0) : inst(ins), index(i) {}
			// instance should be locked
			byte step(byte mode,word hash = 0);
			const exfat::record* get_record(void) const;
			dword get_index(void) const{
				return index;
			}
			literal get_name(void) const;

		};

		//void imp_flush(file_instance*);
	public:
		folder_instance(exfat& fs,folder_instance* top,literal&& str,dword index,const exfat::record* file);
		folder_instance(exfat& fs,const exfat::record* file);
		~folder_instance(void);

		bool is_folder(void) const override{
			assert(attribute & FOLDER);
			return true;
		}

		// instance auto acquired
		file_instance* open(const span<char>& str,byte mode);
		void close(file_instance*);
		//void as_root(dword root_cluster);
		
		file_instance* create(const span<char>& str, byte attrib = 0);
		void update(file_instance*);
		void detach(file_instance*);
		void attach(file_instance*);

	};
}
