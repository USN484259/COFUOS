#pragma once
// #include "types.hpp"
// #include "assert.hpp"
// #include "util.hpp"
#include "common.hpp"

namespace UOS {
	template<typename T>
	class list {
		struct node {
			node* prev;
			node* next;
			T payload;

			node(const T& p) : prev(nullptr), next(nullptr), payload(p) {}
			node(T&& p) : prev(nullptr), next(nullptr), payload(move(p)) {}
		};


		class iterator_base {
		protected:
			node* pos;

			iterator_base(node* p = nullptr) : pos(p) {}

		public:
			iterator_base& operator++(void) {
				if (!pos)
					error(out_of_range, this);
				pos = pos->next;
				return *this;
			}
			iterator_base& operator--(void) {
				error(not_implemented,this);
// #pragma message(" dec from end ? ")
				// if (!pos || !pos->prev)
					// error(out_of_range, this);
				// pos = pos->prev;
				// return *this;
			}


			bool operator==(const iterator_base& cmp) const {
				return pos == cmp.pos;
			}
			bool operator!=(const iterator_base& cmp) const {
				return pos != cmp.pos;
			}

			const T& operator*(void) const {
				return pos->payload;
			}

			const T* operator->(void) const {
				return &pos->payload;
			}

		};

		friend class iterator_base;

	public:
		class iterator : public iterator_base {
			friend class list;
			iterator(node* p) : iterator_base(p) {}
		public:
			iterator(void) {}
			iterator(const iterator& obj) : iterator_base(obj.pos) {}
			iterator(iterator&& obj) : iterator_base(obj.pos) {
				obj.pos = nullptr;
			}
			iterator& operator=(const iterator& obj) {
				pos = obj.pos;
				return *this;
			}
			iterator& operator=(iterator&& obj) {
				pos = move(obj.pos);
				obj.pos = nullptr;
				return *this;
			}

			using iterator_base::operator*;
			using iterator_base::operator->;
			//iterator& operator++(void) {
			//	next();
			//	return *this;
			//}
			//iterator& operator--(void) {
			//	prev();
			//	return *this;
			//}
			T& operator*(void) {
				return pos->payload;
			}
			//const T& operator*(void) const {
			//	return pos->payload;
			//}
			T* operator->(void) {
				return &pos->payload;
			}
			//const T& operator->(void) const {
			//	return pos->payload;
			//}
		};

		class const_iterator : public iterator_base{
			friend class list;
			const_iterator(node* p) : iterator_base(p) {}
		public:
			const_iterator(void) {}


			const_iterator(const const_iterator& obj) : iterator_base(obj.pos) {}
			const_iterator(const_iterator&& obj) : iterator_base(obj.pos) {
				obj.pos = nullptr;
			}
			const_iterator(const iterator& obj) : iterator_base(obj.pos) {}

			const_iterator& operator=(const const_iterator& obj) {
				pos = obj.pos;
				return *this;
			}
			const_iterator& operator=(const_iterator&& obj) {
				pos = move(obj.pos);
				obj.pos = nullptr;
				return *this;
			}
			const_iterator& operator=(const iterator& obj) {
				pos = obj.pos;
				return *this;
			}

			/*
			bool operator==(const iterator& cmp) const {
				return pos == cmp.pos;
			}

			bool operator!=(const iterator& cmp) const {
				return pos != cmp.pos;
			}
			*/


			//const_iterator& operator++(void) {
			//	next();
			//	return *this;
			//}

			//const_iterator& operator--(void) {
			//	prev();
			//	return *this;
			//}
			//const T& operator*(void) const {
			//	return pos->payload;
			//}

			//const T& operator->(void) const {
			//	return pos->payload;
			//}

		};






	private:
		node* head;
		node* tail;
		size_t count;

		friend class iterator;



		void insert_node(node* pos, node* cur) {
			assert(nullptr != cur);
			cur->next = pos;
			cur->prev = pos ? pos->prev : tail;

			if (cur->prev)
				cur->prev->next = cur;
			else
				head = cur;

			if (cur->next)
				cur->next->prev = cur;
			else
				tail = cur;



			/*
			ptr->next = pos;
			if (pos) {
				ptr->prev = pos->prev;
				if (pos->prev)
					pos->prev->next = ptr;
				else {
					head = ptr;
				}
				pos->prev = ptr;
			}
			else {
				ptr->prev = tail;
				tail->next = ptr;
				tail = ptr;
			}

			*/

			++count;
		}

		void erase_node(node* pos) {
			assert(nullptr != pos);
			if (pos->next) {
				pos->next->prev = pos->prev;
			}
			else {
				assert(tail == pos);
				tail = pos->prev;
			}

			if (pos->prev) {
				pos->prev->next = pos->next;
			}
			else {
				assert(head == pos);
				head = pos->next;
			}

			--count;

			pos->prev = pos->next = nullptr;

		}


	public:

		list(void) : head(nullptr), tail(nullptr), count(0) {}

		list(const list& obj) : head(nullptr), tail(nullptr), count(0) {
			for (auto it = obj.cbegin(); it != obj.cend(); ++it) {
				push_back(*it);
			}
			assert(count == obj.count);
		}
		list(list&& obj) : head(obj.head), tail(obj.tail), count(obj.count) {
			obj.head = obj.tail = nullptr;
			obj.count = 0;
		}
		list& operator=(const list& obj) {
			clear();
			for (auto it = obj.cbegin(); it != obj.cend(); ++it) {
				push_back(*it);
			}
			assert(count == obj.count);
			return *this;
		}
		list& operator=(list&& obj) {
			clear();
			assert(0 == count);
			UOS::swap(head, obj.head);
			UOS::swap(tail, obj.tail);
			UOS::swap(count, obj.count);
			return *this;
		}
		~list(void) {
			clear();
		}

		void swap(list& other) {
			UOS::swap(head, other.head);
			UOS::swap(tail, other.tail);
			UOS::swap(count, other.count);
		}


		size_t size(void) const {
			return count;
		}

		bool empty(void) const {
			return !count;
		}
		T& front(void) {
			if (head)
				return head->payload;
			error(out_of_range, this);
			//throw out_of_range();
		}

		const T& front(void) const {
			if (head)
				return head->payload;
			error(out_of_range, this);
			//throw out_of_range();
		}

		T& back(void) {
			if (tail)
				return tail->payload;
			error(out_of_range, this);
			//throw out_of_range();
		}

		const T& back(void) const {
			if (tail)
				return tail->payload;
			error(out_of_range, this);
			//throw out_of_range();
		}

		void clear(void) {
			while (head != tail) {
				node* cur = head;
				head = head->next;
				delete cur;
			}
			if (head) {
				delete head;
				head = tail = nullptr;
			}
			assert(nullptr == head);
			assert(nullptr == tail);
			count = 0;
		}

		void push_front(const T& val) {
			insert(cbegin(), val);
		}
		void push_front(T&& val) {
			insert(cbegin(), move(val));
		}

		void push_back(const T& val) {
			insert(cend(), val);
		}
		void push_back(T&& val) {
			insert(cend(), move(val));
		}

		void pop_front(void) {
			erase(iterator(head));

			/*
			if (!head)
				BugCheck(out_of_range, this, head);
			//throw out_of_range;
			node* cur = head;
			assert(nullptr, cur->prev);
			head = head->next;
			delete cur;
			count--;
			if (head) {
				assertinv(0, count);
				head->prev = nullptr;
			}
			else {
				assert(0, count);
				tail = head;
			}
			*/
		}

		void pop_back(void) {
			erase(iterator(tail));
			/*
			if (!tail)
				BugCheck(out_of_range, this, tail);
			//throw out_of_range;
			node* cur = tail;
			assert(nullptr, cur->next);
			tail = tail->prev;
			delete cur;
			count--;
			if (tail) {
				assertinv(0, count);
				tail->next = nullptr;
			}
			else {
				assert(0, count);
				head = tail;
			}
			*/
		}
		iterator begin(void) {
			return iterator(head);
		}
		iterator end(void) {
			return iterator();
		}
		const_iterator cbegin(void) const {
			return const_iterator(head);
		}
		const_iterator cend(void) const {
			return const_iterator();
		}


		iterator insert(const_iterator pos, const T& val) {
			node* cur = new node(val);

			insert_node(pos.pos, cur);

			/*
			if (cur->prev)
				cur->prev->next = cur;
			else
				head = cur;

			if (cur->next)
				it.pos->prev = cur;
			else
				tail = cur;

			count++;
			*/
			return iterator(cur);
		}
		iterator insert(const_iterator pos, T&& val) {
			node* cur = new node(move(val));

			insert_node(pos.pos, cur);
			/*
			if (cur->prev)
				cur->prev->next = cur;
			else
				head = cur;

			if (cur->next)
				it.pos->prev = cur;
			else
				tail = cur;

			count++;
			*/
			return iterator(cur);
		}

		iterator erase(const_iterator it) {
			if (!it.pos)
				error(out_of_range, this);
			//throw out_of_range();
			node* ret = it.pos->next;

			erase_node(it.pos);

			delete it.pos;

			/*

			if (it.pos->next)
				it.pos->next->prev = it.pos->prev;
			else {
				assert(tail,it.pos);
				tail = it.pos->prev;
			}

			if (it.pos->prev)
				it.pos->prev->next = it.pos->next;
			else
				head = it.pos->next;

			delete it.pos;
			count--;
			*/
			return iterator(ret);
		}

		void splice(const_iterator pos, list& other, const_iterator it) {
			if (it == other.cend())
				error(out_of_range, &other);

			node* cur = it.pos;

			other.erase_node(cur);

			insert_node(pos.pos, cur);

		}


	};






}