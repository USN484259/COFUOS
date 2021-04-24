#pragma once
#include "types.h"
#include "container.hpp"
#include "util.hpp"
#include "hash.hpp"
#include "vector.hpp"
#include "linked_list.hpp"

namespace UOS{
	template<typename T,typename H,typename E,typename C = dword>
	class hash_set{
		struct value_type{
			qword hash_value;
			T payload;

			template<typename ... Arg>
			value_type(Arg&& ... args) : payload(forward<Arg>(args)...) {}
		};
		typedef linked_list<value_type> chain_type;
		typedef typename chain_type::iterator chain_iterator;
		typedef vector<chain_type,C> bucket_type;
		typedef typename bucket_type::iterator bucket_iterator;

		mutable bucket_type bucket;
		C count = 0;
		H obj_hash;
		E obj_equal;

		bucket_iterator find_bucket(qword hash_value) const{
			auto cnt = bucket_count();
			assert(cnt);
			return bucket.begin() + (hash_value % cnt);
		}

		class iterator_base{
			friend class hash_set;
		protected:
			bucket_type* container = nullptr;
			bucket_iterator bucket;
			chain_iterator chain;

			iterator_base(void) = default;
			iterator_base(bucket_type* h,bucket_iterator b,chain_iterator c) : container(h), bucket(b), chain(c){
				//points to a valid position
				while (bucket != container->end() && chain == bucket->end()){
					++bucket;
					chain = bucket->begin();
				}
			}
		public:
			iterator_base(const iterator_base&) = default;

			iterator_base& operator++(void){
				if (!container || bucket == container->end())
					THROW("iterator move past the end @ %p",this);
				assert(chain != bucket->end());
				++chain;
				//points to a valid position
				while (bucket != container->end() && chain == bucket->end()){
					++bucket;
					chain = bucket->begin();
				}
				return *this;
			}

			bool operator==(const iterator_base& cmp) const{
				if (!container || !cmp.container)
					return false;
				if (bucket == cmp.bucket)
					return bucket == container->end() ? true : chain == cmp.chain;
				return false;
			}
			bool operator!=(const iterator_base& cmp) const{
				return ! operator==(cmp);
			}
			T const& operator*(void) const{
				if (!container)
					THROW("deref empty iterator @ %p",this);
				return chain->payload;
			}
			T const* operator->(void) const{
				if (!container)
					THROW("deref empty iterator @ %p",this);
				return &chain->payload;
			}
		};

	public:
		class iterator : public iterator_base{
			friend class hash_set;

			iterator(bucket_type& h, bucket_iterator b,chain_iterator c) \
				: iterator_base(&h,b,c) {}
		public:
			iterator(void) = default;
			iterator(const iterator&) = default;

			iterator& operator=(const iterator& obj){
				this->container = obj.container;
				this->bucket = obj.bucket;
				this->chain = obj.chain;
				return *this;
			}
			
			using iterator_base::operator*;
			using iterator_base::operator->;

			T& operator*(void) {
				if (!this->container)
					THROW("deref empty iterator @ %p",this);
				return this->chain->payload;
			}
			T* operator->(void) {
				if (!this->container)
					THROW("deref empty iterator @ %p",this);
				return &this->chain->payload;
			}
		};

		class const_iterator : public iterator_base{
			friend class hash_set;

			const_iterator(bucket_type& h, bucket_iterator b,chain_iterator c) \
				: iterator_base(&h,b,c) {}
		public:
			const_iterator(void) = default;
			const_iterator(const const_iterator&) = default;
			const_iterator(const iterator& obj) \
				: iterator_base(obj.container, obj.bucket, obj.chain) {}
			
			const_iterator& operator=(const iterator_base& obj){
				this->container = obj.container;
				this->bucket = obj.bucket;
				this->chain = obj.chain;
				return *this;
			}
		};

	private:
		hash_set(C fac,hash_set&& old) : bucket(fac,chain_type()), obj_hash(move(old.obj_hash)), obj_equal(move(old.obj_equal)) {
			for (chain_type& bkt : old.bucket){
				while(!bkt.empty()){
					chain_iterator it = bkt.begin();
					bucket_iterator target = find_bucket(it->hash_value);

					target->splice(target->end(),bkt,it);
					++count;
				}
			}
			// for (auto it = old.begin();it != old.end();++it){
			// 	chain_type& old_chain = *it.bucket;
			// 	chain_iterator old_it = it.chain;
			// 	bucket_iterator bkt = find_bucket(old_it->hash_value);

			// 	bkt->splice(bkt->end(),old_chain,old_it);
			// 	++count;
			// }
			assert(count == old.count);
			old.count = 0;
			old.bucket.clear();	//??
		}

	public:
		hash_set(C fac = 0x10) : bucket(fac,chain_type()) { assert(fac); }
		hash_set(C fac,const H& h,const E& e) : bucket(fac,chain_type()), obj_hash(h), obj_equal(e) { assert(fac); }
		hash_set(const hash_set&) = delete;	//TODO
		hash_set(hash_set&&) = delete;	//TODO
		// ~hash_set(void);	//use ~vector

		C size(void) const{
			return count;
		}
		bool empty(void) const{
			return count == 0;
		}
		void clear(void){
			for (auto it = bucket.begin();it != bucket.end();++it)
				it->clear();
			count = 0;
		}
		C bucket_count(void) const{
			return bucket.size();
		}
		void swap(hash_set& other){
			using UOS::swap;
			bucket.swap(other.bucket);
			swap(count,other.count);
			swap(obj_hash,other.obj_hash);
			swap(obj_equal,other.obj_equal);
		}
		void rehash(C fac){
			assert(fac);
			hash_set tmp(fac,move(*this));
			swap(tmp);
		}

		iterator begin(void){
			auto head = bucket.begin();
			return iterator(bucket,head,head->begin());
		}
		const_iterator begin(void) const{
			auto head = bucket.begin();
			return const_iterator(bucket,head,head->begin());
		}
		iterator end(void){
			return iterator(bucket,bucket.end(),chain_iterator());
		}
		const_iterator end(void) const{
			return const_iterator(bucket,bucket.end(),chain_iterator());
		}
		template<typename K>
		iterator find(const K& key){
			qword key_hash = obj_hash(key);
			bucket_iterator bkt = find_bucket(key_hash);
			for (chain_iterator it = bkt->begin();it != bkt->end();++it){
				if (it->hash_value == key_hash && obj_equal(it->payload,key))
					return iterator(bucket,bkt,it);
			}
			return end();
		}
		template<typename K>
		const_iterator find(const K& key) const{
			qword key_hash = obj_hash(key);
			bucket_iterator bkt = find_bucket(key_hash);
			for (chain_iterator it = bkt->begin();it != bkt->end();++it){
				if (it->hash_value == key_hash && obj_equal(it->payload,key))
					return const_iterator(bucket,bkt,it);
			}
			return end();
		}

		iterator erase(const_iterator pos){
			if (pos.container != &bucket || pos == end())
				THROW("invalid iterator %p for erase @ %p",&pos,this);
			assert(pos.chain != pos.bucket->end());
			assert(count);

			auto res = pos.bucket->erase(pos.chain);
			--count;

			return iterator(bucket,pos.bucket,res);
		}

		template<typename ... Arg>
		iterator insert(Arg&& ... args){
			if (bucket.size() <= count)
				rehash(2*count);
			auto node = new typename chain_type::node(forward<Arg>(args)...);
			auto hash_value = obj_hash(node->payload.payload);
			node->payload.hash_value = hash_value;
			auto bkt = find_bucket(hash_value);
			bkt->put_node(bkt->begin(),node);
			++count;
			return iterator(bucket,bkt,bkt->begin());
		}
	};
}