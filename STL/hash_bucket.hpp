#pragma once
#include "common.hpp"
#include "vector.hpp"
#include "list.hpp"
#include "hash.hpp"

namespace UOS{
	template<typename T, typename H = UOS::hash<T>,typename E = UOS::equal_to<T> >//qword(*H)(const T&) = UOS::hash<T>, bool(*E)(const T&, const T&) = UOS::equal<T> >
	class hash_bucket {
	public:
		typedef pair<qword, T> value_type;
		typedef list<value_type> bucket_type;
		typedef typename bucket_type::iterator bucket_iterator;
		typedef vector<bucket_type> host_type;
		typedef typename host_type::iterator host_iterator;

	private:
		
		mutable host_type buckets;

		host_iterator max_load_pos;
		size_t count;
		H obj_hash;
		E obj_equal;

		host_iterator find_bucket(qword hash_val) const{
			assert(0 != bucket_count());
			return buckets.begin() + (hash_val % bucket_count());
		}


		class iterator_base {
		protected:
			const host_type* host;
			host_iterator bkt;
			bucket_iterator it;

			iterator_base(const host_type* h, host_iterator b, bucket_iterator i) : host(h), bkt(b), it(i) {
				while (bkt != host->cend() && it == bkt->end()) {
					++bkt;
					it = bkt->begin();
				}
			}
			iterator_base(void) : host(nullptr) {}
		public:

			iterator_base& operator++(void) {
				if (nullptr == host)
					error(null_deref, this);
				if (bkt == host->cend())
					error(out_of_range, this);
				
				++it;
				while (bkt != host->cend() && it == bkt->end()) {
					++bkt;
					it = bkt->begin();
				}

				return *this;
			}

			iterator_base& operator--(void) {
				if (nullptr == host)
					error(null_deref, this);
				error(not_implemented, this);
			}

			bool operator==(const iterator_base& cmp) const {
				if (nullptr == host)
					error(null_deref, this);
				if (host != cmp.host)
					error(invalid_argument, cmp.host);
				if (bkt == cmp.bkt)
					return bkt == host->cend() ? true : it == cmp.it;
				return false;
			}
			bool operator!=(const iterator_base& cmp) const {
				return !operator==(cmp);
			}

			const value_type& operator*(void) const {
				if (nullptr == host)
					error(null_deref, this);
				return *it;
			}
			const value_type* operator->(void) const {
				if (nullptr == host)
					error(null_deref, this);
				return it.operator->();
			}


		};

		friend class iterator_base;

	public:

		class iterator : public iterator_base {
			friend class hash_bucket;

			iterator(const host_type& h, host_iterator b, bucket_iterator i) : iterator_base(&h, b, i) {}
		public:
			iterator(void) {}
			iterator(const iterator& obj) : iterator_base(obj.host, obj.bkt, obj.it) {}
			iterator(iterator&& obj) : iterator_base(obj.host, obj.bkt, obj.it) {
				obj.host = nullptr;
			}

			iterator& operator=(const iterator& obj) {
				host = obj.host;
				bkt = obj.bkt;
				it = obj.it;
				return *this;
			}

			iterator& operator=(iterator&& obj) {
				host = obj.host;
				bkt = obj.bkt;
				it = obj.it;

				obj.host = nullptr;

				return *this;
			}

			using iterator_base::operator*;
			using iterator_base::operator->;

			value_type& operator*(void) {
				return *it;
			}
			value_type* operator->(void) {
				return it.operator->();
			}

		};

		class const_iterator : public iterator_base {
			friend class hash_bucket;
			const_iterator(const host_type& h, host_iterator b, bucket_iterator i) : iterator_base(&h, b, i) {}
		public:
			const_iterator(void) {}

			const_iterator(const const_iterator& obj) : iterator_base(obj.host, obj.bkt, obj.it) {}
			const_iterator(const_iterator&& obj) : iterator_base(obj.host, obj.bkt, obj.it) {
				obj.host = nullptr;
			}
			const_iterator(const iterator& obj) : iterator_base(obj.host, obj.bkt, obj.it) {}

			const_iterator& operator=(const const_iterator& obj) {
				host = obj.host;
				bkt = obj.bkt;
				it = obj.it;
				return *this;
			}

			const_iterator& operator=(const_iterator&& obj) {
				host = obj.host;
				bkt = obj.bkt;
				it = obj.it;

				obj.host = nullptr;

				return *this;
			}

			const_iterator& operator=(const iterator& obj) {
				host = obj.host;
				bkt = obj.bkt;
				it = obj.it;
				return *this;
			}
		};


	private:
		hash_bucket(size_t fac, hash_bucket&& old) : buckets(fac,bucket_type()),max_load_pos(buckets.begin()),count(0),obj_hash(move(old.obj_hash)),obj_equal(move(old.obj_equal)) {
			if (0 == fac)
				error(invalid_argument, fac);
			assert(bucket_count() == fac);

			for (auto it = old.begin(); it != old.end();) {
				host_iterator bkt = find_bucket(it->first);
				bucket_type& old_bucket(*it.bkt);
				bucket_iterator old_it(it.it);
				++it;
				bkt->splice(bkt->begin(), old_bucket, old_it);
				++count;
				if (bkt->size() > max_load_pos->size())
					max_load_pos = bkt;
			}
			assert(count == old.count);
			old.count = 0;
			old.buckets.clear();
		}



	public:
		hash_bucket(size_t fac) : buckets(fac, bucket_type()), max_load_pos(buckets.begin()), count(0) {}
		hash_bucket(size_t fac,H& h,E& e) : buckets(fac,bucket_type()),max_load_pos(buckets.begin()),count(0),obj_hash(move(h)),obj_equal(move(e)) {
			if (0 == fac)
				error(invalid_argument, 0);
			//while (factor--)
			//	buckets.push_back(bucket_type());
			assert(bucket_count() == fac);
		}

		hash_bucket(const hash_bucket&) = delete;

		//~hash_bucket(void){}	//vector deletes everything

		hash_bucket& operator=(const hash_bucket&) = delete;

		void swap(hash_bucket& other) {
			using UOS::swap;
			buckets.swap(other.buckets);
			swap(max_load_pos, other.max_load_pos);
			swap(count, other.count);
			swap(obj_hash, other.obj_hash);
			swap(obj_equal, other.obj_equal);
		}

		void clone(const hash_bucket& from) {
			if (0 != count)
				error(invalid_argument, *this);
			if (bucket_count() != from.bucket_count())
				error(invalid_argument, from);

			obj_hash = from.obj_hash;
			obj_equal = from.obj_equal;

			auto it_dst = buckets.begin();
			auto it_sor = from.buckets.cbegin();

			while (it_sor != from.buckets.cend()) {
				assert(it_dst != buckets.end());

				for (auto it = it_sor->cbegin(); it != it_sor->cend(); ++it) {
					it_dst->push_back(*it);
					++count;
				}

				++it_sor;
				++it_dst;
			}

			assert(count == from.count);

		}


		void clear(void) {
			for (auto it = buckets.begin(); it != buckets.end(); ++it)
				it->clear();
			max_load_pos = buckets.begin();
			count = 0;
		}

		size_t bucket_count(void) const {
			return buckets.size();
		}
		
		size_t size(void) const {
			return count;
		}
		/*
		size_t bucket_size(const T& key) const {
			qword key_hash = obj_hash(key);
			auto bkt = find_bucket(key_hash);
			return bkt->size();
		}
		*/
		size_t max_load(void) const {
			return max_load_pos->size();
		}


		void rehash(size_t fac) {
			hash_bucket tmp(fac,move(*this));
			swap(tmp);
		}


		iterator begin(void) {
			auto head = buckets.begin();
			return iterator(buckets, head, head->begin());
		}
		iterator end(void) {
			return iterator(buckets, buckets.end(), bucket_iterator());
		}
		const_iterator cbegin(void) const {
			auto head = buckets.begin();
			return const_iterator(buckets, head, head->begin());
		}
		const_iterator cend(void) const{
			return const_iterator(buckets, buckets.end(), bucket_iterator());
		}
		template<typename C>
		iterator find(const C& key) {
			qword key_hash = obj_hash(key);
			host_iterator bkt = find_bucket(key_hash);
			for (bucket_iterator it = bkt->begin(); it != bkt->end(); ++it) {
				if (it->first == key_hash && obj_equal(it->second,key))
					return iterator(buckets, bkt, it);
			}
			return end();
		}
		template<typename C>
		const_iterator find(const C& key) const{
			qword key_hash = obj_hash(key);
			host_iterator bkt = find_bucket(key_hash);
			for (bucket_iterator it = bkt->begin(); it != bkt->end(); ++it) {
				if (it->first == key_hash && obj_equal(it->second,key))
					return const_iterator(buckets, bkt, it);
			}
			return cend();
		}

		iterator erase(const_iterator pos) {
			if (pos == end())
				error(invalid_argument, pos);

			assert(pos.it != pos.bkt->end());
			
			assert(0 != count);

			auto res = pos.bkt->erase(pos.it);

			--count;

			if (pos.bkt == max_load_pos) {
				for (auto it = buckets.begin(); it != buckets.end(); ++it) {
					if (it->size() > max_load_pos->size())
						max_load_pos = it;
				}
			}

			return iterator(buckets, pos.bkt, res);

		}

		iterator insert(const_iterator hint, const T& val) {
			qword hash_value = obj_hash(val);
			host_iterator bkt = find_bucket(hash_value);
			bucket_iterator it = bkt->begin();
			if (hint != end()) {
				assert(hint.host == &buckets);
				if (hint.bkt != bkt || false == obj_equal(*hint, val))
					error(invalid_argument, hint);
				it = hint.it;
			}
			auto res = bkt->insert(it, value_type(hash_value, val));

			++count;

			if (bkt->size() > max_load_pos->size())
				max_load_pos = bkt;

			return iterator(buckets, bkt, res);

		}

		iterator insert(const_iterator hint, T&& val) {
			qword hash_value = obj_hash(val);
			host_iterator bkt = find_bucket(hash_value);
			bucket_iterator it = bkt->begin();
			if (hint != end()) {
				assert(hint.host == &buckets);
				if (hint.bkt != bkt || false == obj_equal(hint->second, val))
					error(invalid_argument, hint);
				it = hint.it;
			}

			auto res = bkt->insert(it, move(value_type(move(hash_value), move(val))));

			++count;

			if (bkt->size() > max_load_pos->size())
				max_load_pos = bkt;

			return iterator(buckets, bkt, res);

		}
	};





}