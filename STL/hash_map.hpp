#pragma once
#include "common.hpp"
#include "hash_bucket.hpp"


namespace UOS {
	template<typename K,typename V, typename H = UOS::hash<K>,typename E = UOS::equal_to<K> >
	class hash_map {
	public:
		typedef pair<const K, V> value_type;
	private:

		struct K_H : public H{
			K_H(void) = default;
			K_H(const H& h) : H(h) {}
			using H::operator();
			qword operator()(const value_type& vt) const {
				return operator()(vt.first);
			}

		};

		struct K_E : public E {
			typedef E super;
			K_E(void) = default;
			K_E(const super& e) : super(e) {}



			bool operator()(const value_type& a, const value_type& b) const {
				return super::operator()(a.first, b.first);
			}
			template<typename T>
			bool operator()(const value_type& a, const T& b) const {
				return super::operator()(a.first, b);
			}
			template<typename T>
			bool operator()(const T& a, const value_type& b) const {
				return super::operator()(a, b.first);
			}
		};

		typedef hash_bucket<value_type,K_H,K_E> container_type;
		

		enum : size_t { initial_count = 64 };

		container_type container;
		float factor;

	public:
		
		class iterator : public container_type::iterator {
			typedef typename container_type::iterator super;
		public:
			iterator(const super& s) :super(s){}
			iterator(super&& s) : super(move(s)) {}
			iterator(const iterator&) = default;
			iterator(iterator&&) = default;

			iterator& operator=(const iterator&) = default;
			iterator& operator=(iterator&&) = default;

			value_type& operator*(void) {
				return super::operator*().second;
			}
			const value_type& operator*(void) const{
				return super::operator*().second;
			}
			value_type* operator->(void) {
				return &super::operator*().second;
			}
			const value_type* operator->(void) const{
				return &super::operator*().second;
			}

		};

		class const_iterator : public container_type::const_iterator {
			typedef typename container_type::const_iterator super;
		public:
			const_iterator(const super& s) : super(s) {}
			const_iterator(super&& s) : super(move(s)) {}
			const_iterator(const const_iterator&) = default;
			const_iterator(const_iterator&&) = default;

			const_iterator(const iterator& i) : super(i) {}

			const_iterator& operator=(const const_iterator&) = default;
			const_iterator& operator=(const_iterator&&) = default;
			const_iterator& operator=(const iterator& i) {
				super::operator=(i);
				return *this;
			}
			const value_type& operator*(void) const {
				return super::operator*().second;
			}
			const value_type* operator->(void) const {
				return &super::operator*().second;
			}

		};


	public:
		hash_map(size_t bucket_count = initial_count,H h = H(),E e = E()) : container(bucket_count,K_H(h),K_E(e)), factor(8.0) {}

		hash_map(const hash_map& obj) : container(obj.container.bucket_count()), factor(obj.factor) {
			container.clone(obj.container);
		}
		hash_map(hash_map&& obj) : container(initial_count), factor(obj.factor) {
			container.swap(obj.container);
		}

		hash_map& operator=(const hash_map& obj) {
			clear();
			factor = obj.factor;
			container.rehash(obj.container.bucket_count());
			container.clone(obj.container);
			return *this;
		}
		hash_map& operator=(hash_map&& obj) {
			clear();
			factor = obj.factor;
			container.swap(obj.container);
			return *this;
		}

		void swap(hash_map& other) {
			container.swap(other.container);
			UOS::swap(factor, other.factor);
		}

		size_t size(void) const {
			return container.size();
		}
		bool empty(void) const {
			return 0 == container.size();
		}

		void clear(void) {
			container.clear();
		}
		size_t bucket_count(void) const {
			return container.bucket_count();
		}
		float max_load_factor(void) const {
			return factor;
		}
		void max_load_factor(float f) {
			if (f <= 0.0)
				error(invalid_argument, f);
			factor = f;
			rehash();
		}

		float load_factor(void) const {
			return (float)size() / bucket_count();
		}

		iterator begin(void) {
			return iterator(container.begin());
		}
		const_iterator cbegin(void) const {
			return const_iterator(container.cbegin());
		}
		iterator end(void) {
			return iterator(container.end());
		}
		const_iterator cend(void) const {
			return const_iterator(container.cend());
		}

		template<typename C>
		iterator find(const C& key) {
			return iterator(container.find(key));
		}
		template<typename C>
		const_iterator find(const C& key) const {
			return const_iterator(container.find(key));
		}

		pair<iterator, bool> insert(const value_type& val) {
			auto pos = container.find(val);

			if (pos != container.end())
				return make_pair(iterator(pos), false);

			while (load_factor() * container.max_load() >= factor) {
				container.rehash(4 * container.bucket_count());
			}
			return make_pair(iterator(container.insert(container.cend(), val)), true);
		}
		pair<iterator, bool> insert(value_type&& val) {
			auto pos = container.find(val);

			if (pos != container.end())
				return make_pair(iterator(pos), false);

			while (load_factor() * container.max_load() >= factor) {
				container.rehash(4 * container.bucket_count());
			}
			return make_pair(iterator(container.insert(container.cend(), move(val))), true);
		}

		V& operator[](const K& key) {
			iterator pos = find(key);
			if (pos == end())
				pos = insert(value_type(key, V())).first;
			return pos->second;
		}
		V& at(const K& key) {
			iterator pos = find(key);
			if (pos == end())
				error(out_of_range, key);
			return pos->second;
		}
		const V& at(const K& key) const {
			const_iterator pos = find(key);
			if (pos == cend())
				error(out_of_range, key);
			return pos->second;
		}

		iterator erase(const_iterator pos) {
			if (pos == cend())
				error(out_of_range, pos);

			return iterator(container.erase(pos));
		}
		void rehash(size_t count) {
			container.rehash(count);
		}

		void rehash(void) {
			while (load_factor() * container.max_load() < 0.2 * factor) {
				container.rehash(container.bucket_count / 4);
			}
			while (load_factor() * container.max_load() >= factor) {
				container.rehash(4 * container.bucket_count());
			}


		}
	};

}