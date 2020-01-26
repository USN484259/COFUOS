#pragma once
// #include "types.hpp"
// #include "util.hpp"
// #include "list.hpp"

#include "common.hpp"
#include "hash_bucket.hpp"


namespace UOS{
	template<typename T, qword (*H)(const T&) = UOS::hash<T>, bool (*E)(const T&, const T&) = UOS::equal<T> >
	class hash_set{
		
		typedef hash_bucket<T, H, E> container_type;
		enum : size_t { initial_count = 64 };

		container_type container;

		float factor;

	public:

		class const_iterator : public container_type::const_iterator {
			typedef typename container_type::const_iterator super;
		public:
			const_iterator(const super& s) : super(s) {}
			const_iterator(super&& s) : super(move(s)) {}
			const_iterator(const const_iterator&) = default;
			const_iterator(const_iterator&&) = default;

			const_iterator& operator=(const const_iterator&) = default;
			const_iterator& operator=(const_iterator&&) = default;

			const T& operator*(void) const {
				return super::operator*().second;
			}

			const T* operator->(void) const {
				return &super::operator*().second;
			}


		};

		typedef const_iterator iterator;


	public:
	
	
	
		hash_set(size_t bucket_count = initial_count) : container(bucket_count), factor(8.0) {}
		
		hash_set(const hash_set& obj) : container(obj.container.bucket_count()), factor(obj.factor) {
			container.clone(obj.container);
		}
		hash_set(hash_set&& obj) : container(initial_count), factor(obj.factor) {
			container.swap(obj.container);
		}

		hash_set& operator=(const hash_set& obj) {
			clear();
			factor = obj.factor;
			container.rehash(obj.container.bucket_count());
			container.clone(obj.container);
			return *this;
		}
		hash_set& operator=(hash_set&& obj){
			clear();
			factor = obj.factor;
			container.swap(obj.container);
			return *this;
		}
		
		void swap(hash_set& other) {
			container.swap(other.container);
			UOS::swap(factor, other.factor);
		}

		size_t size(void) const{
			return container.size();
		}
		
		bool empty(void) const{
			return 0 == container.size();
		}
		
		void clear(void){
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

		iterator begin(void){
			return iterator(container.begin());
		}
		const_iterator cbegin(void) const{
			return const_iterator(container.cbegin());
		}

		iterator end(void){
			return iterator(container.end());

		}
		const_iterator cend(void) const{
			return const_iterator(container.cend());
		}

		
		iterator find(const T& key){
			return iterator(container.find(key));
			
		}
		const_iterator find(const T& key) const{
			return const_iterator(container.find(key));
		}
		
		
		pair<iterator,bool> insert(const T& val){

			while (load_factor() * container.max_load() >= factor) {
				container.rehash(4 * container.bucket_count());
			}

			auto pos = container.find(val);

			if (pos != container.end())
				return make_pair(iterator(pos), false);

			return make_pair(iterator(container.insert(pos, val)), true);

		}
		
		pair<iterator,bool> insert(T&& val){

			while (load_factor() * container.max_load() >= factor) {
				container.rehash(4 * container.bucket_count());
			}

			auto pos = container.find(val);

			if (pos != container.end())
				return make_pair(iterator(pos), false);

			return make_pair(iterator(container.insert(pos, move(val))), true);

		}
		iterator erase(const_iterator pos) {
			if (pos == end())
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