#pragma once
#include "types.h"
#include "sync/include/rwlock.hpp"
#include "assert.hpp"
#include "literal.hpp"
#include "span.hpp"
#include "hash_set.hpp"
#include "fat32.hpp"

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
		qword size = 0;
		dword rec_index = 0;
		byte rec_size = 0;
		byte attribute = 0;	//FAT attributes
		byte state = 0;	//share_read, share_write, dirty, deleted
		FAT32::cluster_cache clusters;
		enum : byte {DIRTY = 0x40, DELETED = 0x80};

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
			auto begin(void) const{
				return instance->name.begin();
			}
			auto end(void) const{
				return instance->name.end();
			}
			size_t size(void) const{
				return instance->name.size();
			}
		};

		struct hash{
			template<typename T>
			qword operator()(const T& obj){
				char buffer[obj.size()];
				unsigned index = 0;
				for (auto it = obj.begin();it != obj.end();++it){
					assert(index < obj.size());
					auto ch = *it;
					if (ch >= 'a' && ch <= 'z')
						ch &= ~0x20;
					buffer[index++] = ch;
				}
				assert(index == obj.size());
				return fasthash64(buffer,index,0);
			}
		};

		struct equal{
			// bool operator()(const file_instance* obj,const file_instance*& cmp){
			// 	return obj == cmp;
			// }
			template<typename A,typename B>
			bool operator()(const A& a,const B& b){
				auto sz = a.size();
				if (sz != b.size())
					return false;
				return sz == match(a.begin(),b.begin(),sz,[](char x,char y) -> bool{
					if (x == y)
						return true;
					if ((x ^ y) == 0x20){
						x &= ~0x20;
						return (x >= 'A' && x <= 'Z');
					}
					return false;
				});
			}
		};
		friend class hash;
		friend class equal;

	public:
		file_instance(FAT32& f,literal&& str,folder_instance* top,const FAT32::record* rec = nullptr);

		virtual ~file_instance(void);
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
			return size;
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

		qword get_lba(dword offset);

		bool set_name(const span<char>& str);
		bool move_to(folder_instance* target);
		bool remove(void);
	};
	class folder_instance : public file_instance{
		//size always 0
		hash_set<instance_wrapper,hash,equal> file_table;

		void imp_flush(file_instance*);
	public:
		folder_instance(FAT32& f,literal&& str,folder_instance* top,const FAT32::record* rec = nullptr);
		~folder_instance(void);

		//literal get_record(dword& off,FAT32_record& rec);
		// instance acquired
		file_instance* open(const span<char>& str);

		void flush(file_instance*);
		//flush if dirty
		void detach(file_instance*);
		void attach(file_instance*);

	};
}

/*
file		
folder		detach	attach

open
close
rename
move_to		top->detach		tar->attach
remove		top->detach
create		tar->attach

move_to:
interrupt_guard<rwlock> guard(this->lock);
if (ref_count != 1)
	return false;
top->lock();
tar->lock();

top->detach(this);
tar->attach(this);

top->unlock();
tar->unlock();



*/