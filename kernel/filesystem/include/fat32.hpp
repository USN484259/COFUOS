#pragma once
#include "types.h"
#include "assert.hpp"
#include "vector.hpp"
#include "linked_list.hpp"
#include "string.hpp"
#include "process/include/thread.hpp"
#include "sync/include/rwlock.hpp"
#include "sync/include/event.hpp"

namespace UOS{
	class folder_instance;
	class file;
	class FAT32{
	public:
		struct record{
			union {
				char name[11];
				struct {
					byte type;
					byte rec_size;
					word rec_index;
				};
			}__attribute__((packed));
			byte attrib;
			dword reserved1;
			dword reserved2;
			word cluster_hi;
			word reserved3;
			word reserved4;
			word cluster_lo;
			dword size;

			//enum ATTRIBUTE : byte {READONLY = 1,HIDDEN = 2,SYSTEM = 4,VID = 8,FOLDER = 0x10,ARCHIVE = 0x20};
		}__attribute__((packed));
		static_assert(sizeof(record) == 0x20,"FAT32_record size mismatch");

		class cluster_cache{
#ifndef NDEBUG
			static constexpr word limit = 0x10;
#else
			static constexpr word limit = 0x100;
#endif
			struct line{
				dword index;
				dword cluster;

				bool operator<(const line& cmp) const{
					return index > cmp.index;
				}
			};
		public:
			FAT32& fs;
		private:
			rwlock objlock;
			dword first_cluster;
			linked_list<line> cache_line;	//decending order
			linked_list<decltype(cache_line)::iterator> lru_queue;

		public:
			cluster_cache(FAT32& f,dword head = 0) : fs(f), first_cluster(head) {}
			void set_head(dword head);
			dword get(dword offset);
		};
		class thread_pool{
			rwlock objlock;
			volatile dword worker_count = 0;
			FAT32& fs;
			event barrier;

			struct{
				file* head;
				file* tail;
			} request_list = {0};

			//static void thread_worker(qword,qword,qword,qword);
		public:
			thread_pool(FAT32& host) : fs(host) {}
			void launch(unsigned count);
			void put(file* f);
			file* get(void);
		};

		class block_pool{
			struct block{
				dword base;
				dword count;
			};
			
			rwlock objlock;
			linked_list<block> pool;
			dword total_count = 0;
		public:
			block_pool(void) = default;
			dword size(void) const{
				return total_count;
			}
			void put(dword base,dword count);
			dword get(dword count);
		};

	private:
		dword base = 0;
		dword top = 0;
		dword table[2] = {0};
		dword data = 0;
		dword eoc = 0;
		byte cluster_size = 0;
		folder_instance* root = nullptr;
		thread* th_init = nullptr;
		//rwlock objlock;
		block_pool free_clusters;
		
		thread_pool workers;

		static void thread_init(qword,qword,qword,qword);
		static void thread_worker(qword,qword,qword,qword);
		void worker_read(file*);
		void worker_write(file*);
	public:
		FAT32(unsigned th_cnt);
		dword get_cluster_size(void);
		qword lba_from_fat(dword fat_index,bool spare_table = false);
		qword lba_from_cluster(dword cluster);
		
		static string resolve_name(const record& rec,const vector<record>& long_name);
		folder_instance* get_root(void) const{
			return root;
		}
		void wait(void);
		void task(file* f);
	};
	extern FAT32 filesystem;
}

/*
basic_file* object;
process* host;

command / status
offset
length
buffer
*/