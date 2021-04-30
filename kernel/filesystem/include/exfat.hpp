#pragma once
#include "types.h"
#include "assert.hpp"
#include "constant.hpp"
#include "vector.hpp"
#include "linked_list.hpp"
#include "string.hpp"
#include "process/include/thread.hpp"
#include "sync/include/rwlock.hpp"
#include "sync/include/event.hpp"

namespace UOS{
	class folder_instance;
	class file;

	class exfat{
		class thread_pool{
			rwlock objlock;
			volatile dword worker_count = 0;
			event barrier;

			struct{
				file* head;
				file* tail;
			} request_list = {0};

			//static void thread_worker(qword,qword,qword,qword);
		public:
			thread_pool(void) = default;
			void launch(exfat* fs,unsigned count);
			void stop(void);
			void put(file* f);
			file* get(void);
		};
	private:
		qword base = 0;
		qword top = 0;
		qword table = 0;
		qword bitmap = 0;
		qword heap = 0;
		dword cluster_count = 0;
		byte cluster_shift = 0;
		byte flags = 0;		//writable, stopping
		folder_instance* root = nullptr;
		thread* th_init = nullptr;
		thread_pool workers;

		static void thread_init(qword,qword,qword,qword);
		static void thread_worker(qword,qword,qword,qword);
		
		//returns root cluster,MSB set on FAT_1, 0 on failure
		dword parse_header(const void* ptr);
		void worker_read(file*);
		void worker_write(file*);
	public:
		//static constexpr dword fatentry_per_sector = SECTOR_SIZE/sizeof(dword);
	public:
		exfat(unsigned worker_count);
		dword cluster_size(void) const{
			return (dword)1 << cluster_shift;
		}
		folder_instance* get_root(void) const{
			return root;
		}
		void wait(void){
			th_init->wait();
		}
		void stop(void);
		void task(file* f);
		//qword lba_from_fat(dword fat_index) const;
		qword lba_of_fat(dword sector) const;
		qword lba_of_cluster(dword cluster) const;

		template<typename T>
		static word name_hash(const T& str){
			word sum = 0;
			for (byte ch : str){
				if (ch >= 'a' && ch <= 'z')
					ch &= ~0x20;
				sum = (((sum & 1) ? 0x8000 : 0) | (sum >> 1)) + ch;
				sum = ((sum & 1) ? 0x8000 : 0) | (sum >> 1);
			}
			return sum;
		}
		template<typename A,typename B>
		static bool name_equal(const A& a,const B& b){
			size_t sz = a.size();
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
	class cluster_chain{
#ifndef NDEBUG
		static constexpr word limit = 4;
#else
		static constexpr word limit = 0x40;
#endif
		struct line{
			dword index;
			dword cluster;

			bool operator<(const line& cmp) const{
				return index > cmp.index;
			}
		};

		exfat& fs;
		rwlock objlock;
		const bool root;
		bool linear = false;
		dword first_cluster = 0;
		qword alloc_size = 0;
		linked_list<line> cache_line;	//decending order
		linked_list<decltype(cache_line)::iterator> lru_queue;
	public:
		cluster_chain(exfat& f,bool r = false) : fs(f), root(r) {}
		exfat& host(void) const{
			return fs;
		}
		qword size(void) const{
			return alloc_size;
		}
		void assign(dword head,qword sz,bool linear_block);
		dword get(qword offset);
		// expand or truncate
		// returns true if mode changes
		bool set(qword offset);

	};
	
	extern exfat filesystem;
}