#pragma once
#include "types.h"
#include "lock_guard.hpp"

namespace UOS{
	// min = block_size(bot), max = block_size(top - 1)
	template<byte bot,byte top,typename M>
	class buddy_heap {
		static_assert(bot >= 4 && top > bot,"Invalid buddy_heap instance");

		static constexpr size_t block_mask(byte sh){
			return ((size_t)1 << sh) - 1;
		}
		static constexpr size_t block_size(byte sh){
			return (size_t)1 << sh;
		}
		static constexpr byte block_index(byte sh){
			return sh - bot;
		}
		static constexpr qword block_diff(const void* a,const void* b){
			return reinterpret_cast<qword>(a) ^ reinterpret_cast<qword>(b);
		}
		//get shift of size ( aligned up )
		inline static byte category(const size_t sz){
			assert(sz);
			if (sz > block_size(top - 1))
				return top;
			auto res = top;
			do{
				--res;
				if (sz & ~block_mask(res)){
					if (sz & block_mask(res))
						++res;
					assert(res < top);
					return res;
				}
			}while(res > bot);
			assert(res == bot);
			return res;
		}
		//get shift of given memory range (aligned down)
		inline static byte category(const void* ptr,const size_t sz){
			assert(ptr && sz);
			auto res = top;
			auto cur = bot;
			auto base = reinterpret_cast<qword>(ptr);
			while(cur < top){
				if (base & block_mask(cur))
					break;
				if (sz < block_size(cur))
					break;
				res = cur;
				++cur;
			}
			return res;
		}
	public:
		typedef void* (*EXPANDER)(size_t&);
	private:
		struct node{
			node* prev;
			node* next;
		};

		M lock;
		node* pool[top - bot] = {0};
		size_t cap_size = 0;
		EXPANDER callback = nullptr;

	private:
		void* get(byte sh){
			if (sh >= top)
				return nullptr;
			node* cur = nullptr;
			auto index = block_index(sh);
			if (pool[index]){
				cur = pool[index];
				assert(nullptr == cur->prev);
				assert(0 == (reinterpret_cast<qword>(cur) & block_mask(sh)));
				if (cur->next){
					assert(cur->next->prev == cur);
					cur->next->prev = nullptr;
				}
				pool[index] = cur->next;
				return cur;
			}
			cur = reinterpret_cast<node*>(get(sh + 1));
			if (nullptr == cur)
				return nullptr;
			put(reinterpret_cast<byte*>(cur) + block_size(sh),sh);
			return cur;
		}
		bool put(void* ptr,byte sh){
			if (0 != (reinterpret_cast<qword>(ptr) & block_mask(sh)))
				return false;
			auto index = block_index(sh);
			node* block = reinterpret_cast<node*>(ptr);
			node* cur = pool[index];
			node* prev = nullptr;
			// find place to insert
			while(cur && cur < block){
				assert(cur->prev == prev);
				prev = cur;
				cur = cur->next;
			}
			assert(!prev || prev->next == cur);
			assert(!cur || cur->prev == prev);

			if (sh + 1 < top){
				// check buddy blocks
				node* buddy_block = nullptr;
				if (prev && block_diff(prev,block) == block_size(sh)){
					buddy_block = prev;
				}
				else if (cur && block_diff(cur,block) == block_size(sh)){
					buddy_block = cur;
				}

				if (buddy_block){
					if (buddy_block->next){
						assert(buddy_block->next->prev == buddy_block);
						buddy_block->next->prev = buddy_block->prev;
					}
					if (buddy_block->prev){
						assert(buddy_block->prev->next == buddy_block);
						buddy_block->prev->next = buddy_block->next;
					}
					else{
						assert(buddy_block == pool[index]);
						pool[index] = buddy_block->next;
					}
					return put(min(block,buddy_block),sh + 1);
				}
			}
			// prev < block < cur
			block->prev = prev;
			block->next = cur;
			if (cur)
				cur->prev = block;
			if (prev)
				prev->next = block;
			else{
				assert(pool[index] == cur);
				pool[index] = block;
			}
			return true;
		}
		
	public:
		buddy_heap(EXPANDER xp = nullptr) : callback(xp) {}

		size_t capacity(void) const{
			return cap_size;
		}
		size_t max_size(void) const{
			auto res = top;
			for (auto i = bot;i < top;++i){
				if (pool[i])
					res = i;
			}
			return res == top ? 0 : block_size(res);
		}

		void* allocate(size_t req){
			auto sh = category(req);
			if (sh >= top)
				return nullptr;
			void* ptr = nullptr;
			{
				lock_guard<M> guard(lock);
				ptr = get(sh);
			}
			if (ptr || callback == nullptr)
				return ptr;
			size_t new_size = max(req,max<size_t>(cap_size,0x4000));
			void* new_block = callback(new_size);
			if (!expand(new_block,new_size)){
				return nullptr;
			}
			return allocate(req);
		}
		bool release(void* ptr,size_t sz){
			if (!ptr)
				return false;
			auto sh = category(sz);
			if (sh >= top)
				return false;
			lock_guard<M> guard(lock);
			return put(ptr,sh);
		}
		bool expand(void* ptr,size_t len){
			if (!ptr || !len)
				return false;
			auto base = reinterpret_cast<qword>(ptr);
			auto aligned_base = align_up(base,block_size(bot));
			if (len <= (aligned_base - base))
				return false;
			len -= (aligned_base - base);
			auto cur = reinterpret_cast<byte*>(aligned_base);
			bool res = false;
			lock_guard<M> guard(lock);

			while(len >= block_size(bot)){
				auto sh = category(cur,len);
				if (sh >= top)
					break;
				auto size = block_size(sh);
				assert(size <= len);
				assert(0 == (reinterpret_cast<qword>(cur) & block_mask(sh)));
				if (!put(cur,sh)){
					res = false;
					break;
				}
				cur += size;
				len -= size;
				cap_size += size;
				res = true;
			}
			return res;
		}
	};
}
