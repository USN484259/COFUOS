#pragma once
#include "types.h"
#include "assert.hpp"
#include "constant.hpp"
#include "linked_list.hpp"
#include "string.hpp"
#include "dev/include/disk_interface.hpp"
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
			volatile bool stopping = false;
			struct{
				file* head;
				file* tail;
			} request_list = {0};

		public:
			thread_pool(void) = default;
			void launch(exfat* fs,unsigned count);
			void stop(void);
			void put(file* f);
			file* get(void);
		};
	private:
		enum : byte { ALLOW_WRITE = 2, STOPPING = 0x80 };
		qword base = 0;
		qword top = 0;
		qword table = 0;
		qword bitmap = 0;
		qword heap = 0;
		dword cluster_count = 0;
		byte cluster_shift = 0;
		byte volume_state = 0;
		volatile word flags = 0;	//writable, stopping
		folder_instance* root = nullptr;
		thread* th_init = nullptr;
		thread_pool workers;
		rwlock bmp_lock;
		dword bmp_last_index = 0;

		static void thread_init(qword,qword,qword,qword);
		static void thread_worker(qword,qword,qword,qword);
		
		//returns root cluster,MSB set on FAT_1, returns 0 on failure
		dword parse_header(const void* ptr);
		void worker_read(file*);
		void worker_write(file*);
		void worker_list(file*);
	public:
		static constexpr dword record_size = 0x20;
		static constexpr dword record_per_sector = SECTOR_SIZE / record_size;
		static constexpr dword bit_per_sector = SECTOR_SIZE * 8;
		struct record{
			byte magic_0;
			byte entrance_size;
			word checksum;
			word attributes;
			word reserved[0x0D];
			byte magic_1;
			byte alloc_stat;
			byte reserved_1;
			byte name_size;
			word name_hash;
			word reserved_2;
			qword valid_size;
			dword reserved_3;
			dword first_cluster;
			qword alloc_size;
		};
		static_assert(sizeof(record) == 0x40,"exfat::record size mismatch");

		class allocator{
			exfat& fs;
			qword block_lba = 0;
			disk_interface::slot* block = nullptr;
		public:
			allocator(exfat& f);
			~allocator(void);
			dword get(void);
			bool put(dword cluster);
		};
		friend class allocator;
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
		bool is_rw(void) const{
			return flags & ALLOW_WRITE;
		}
		bool is_running(void) const{
			return 0 == (flags & STOPPING);
		}
		bool set_rw(bool force = false);
		bool stop(void);
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
		dword first_cluster;
		linked_list<line> cache_line;	//decending order
		linked_list<decltype(cache_line)::iterator> lru_queue;

		bool linear;
	public:
		//cluster_chain(exfat& f,bool r = false) : fs(f), root(r) {}
		cluster_chain(exfat& f, dword head, bool l = false) : fs(f), first_cluster(head), linear(l) {}
		exfat& host(void) const{
			return fs;
		}
		bool is_linear(void) const{
			return linear;
		}
		//void assign(dword head,bool linear_block);
		dword get(dword index);

		bool expand(dword index);
		bool truncate(dword count);
		// expand or truncate to (count) clusters
		// bool set(qword count);

	};
	extern exfat filesystem;
}