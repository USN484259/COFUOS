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
		friend class folder_instance;
		friend class instance_wrapper;
	protected:
		mutable rwlock objlock;
		dword ref_count = 1;
		folder_instance* parent;
		literal name;
		qword valid_size = 0;
		qword alloc_size = 0;
		dword rec_index = (-1);
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

	public:
		// X locked after construction
		file_instance(exfat& fs,literal&& str,folder_instance* top);
		// X locked before destruction
		virtual ~file_instance(void);
		
		virtual bool is_folder(void) const{
			assert(0 == (attribute & FOLDER));
			return false;
		}
		void acquire(void);
		void relax(void);

		inline void lock(void){
			objlock.lock(rwlock::SHARED);
		}
		inline bool try_lock(void){
			return objlock.try_lock(rwlock::SHARED);
		}
		inline void unlock(void){
			assert(objlock.is_locked() && !objlock.is_exclusive());
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

		qword get_lba(qword offset);

		bool set_name(const span<char>& str);
		bool set_size(qword sz);
		bool move_to(folder_instance* target);
		bool remove(void);
	};
	class folder_instance : public file_instance{

		hash_set<instance_wrapper,instance_wrapper::hash,instance_wrapper::equal> file_table;

		//void imp_flush(file_instance*);
		file_instance* imp_open(dword index,const span<char>& str,const void* buffer);
	public:
		folder_instance(exfat& f,literal&& str,folder_instance* top);
		~folder_instance(void);

		bool is_folder(void) const override{
			assert(attribute & FOLDER);
			return true;
		}

		// instance auto acquired
		file_instance* open(const span<char>& str);
		void close(file_instance*);
		void as_root(dword root_cluster);
		
		file_instance* create(const span<char>& str, byte attrib = 0);
		void update(file_instance*);
		void detach(file_instance*);
		void attach(file_instance*);

	};
}
