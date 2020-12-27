#pragma once
#include "types.hpp"
#include "util.hpp"
#include "assert.hpp"

#ifndef THROW
#ifdef COFUOS
#include "bugcheck.hpp"
#define THROW(x) BugCheck(unhandled_exception,x)
#else
#define THROW throw
#endif
#endif

namespace UOS{
	template<typename T>
	class linked_list{
		struct node{
			node* prev = nullptr;
			node* next = nullptr;
			T payload;

			template<typename ... Arg>
			node(Arg&& ... args) : payload(forward<Arg>(args)...) {}
		};
		node* head = nullptr;
		node* tail = nullptr;
		size_t count = 0;
	public:
		class iterator_base{
		protected:
			friend class linked_list;
			linked_list* owner = nullptr;
			node* ptr = nullptr;
			iterator_base(void) = default;
			iterator_base(linked_list* l,node* p) : owner(l),ptr(p) {}
			iterator_base(const iterator_base&) = delete;
		public:
			iterator_base& operator++(void){
				if (!ptr)
					THROW(this);
				ptr = ptr->next;
				return *this;
			}
			iterator_base& operator--(void){
				if (ptr && ptr->prev){
					ptr = ptr->prev;
				}
				else if (owner && owner->count){
					ptr = owner->tail;
				}
				else{
					THROW(this);
				}
				return *this;
			}
			bool operator==(const iterator_base& cmp) const{
				if (owner == cmp.owner){
					return ptr == cmp.ptr;
				}
				THROW(this);
			}
			bool operator!=(const iterator_base& cmp) const{
				if (owner == cmp.owner){
					return ptr != cmp.ptr;
				}
				THROW(this);
			}
			const T& operator*(void) const{
				if (ptr)
					return ptr->payload;
				THROW(this);
			}
			const T* operator->(void) const{
				if (ptr)
					return &ptr->payload;
				THROW(this);
			}
		};
		friend class iterator_base;
	public:
		class iterator : public iterator_base{
			friend class linked_list;
			iterator(linked_list* l,node* p) : iterator_base(l,p) {}
		public:
			iterator(void) = default;
			iterator(const iterator& other) : iterator_base(other.owner,other.ptr) {}
			iterator& operator=(const iterator& other){
				owner = other.owner;
				ptr = other.ptr;
				return *this;
			}
			using iterator_base::operator*;
			using iterator_base::operator->;
			T& operator*(void){
				if (ptr)
					return ptr->payload;
				THROW(this);
			}
			T* operator->(void){
				if (ptr)
					return &ptr->payload;
				THROW(this);
			}
		};
		class const_iterator : public iterator_base{
			friend class linked_list;
			const_iterator(linked_list* l,node* p) : iterator_base(l,p) {}
		public:
			const_iterator(void) = default;
			const_iterator(const const_iterator& other) : iterator_base(other.owner,other.ptr) {}
			const_iterator(const iterator& other) : iterator_base(other.owner,other.ptr) {}
			const_iterator& operator=(const const_iterator& other){
				owner = other.owner;
				ptr = other.ptr;
				return *this;
			}
			const_iterator& operator=(const iterator& other){
				owner = other.owner;
				ptr = other.ptr;
				return *this;
			}
		};
		friend class iterator;
		friend class const_iterator;
	public:
		linked_list(void) = default;
		linked_list(const linked_list& other){
			for (const auto& ele : other){
				push_back(ele);
			}
		}
		linked_list(linked_list&& other){
			swap(head,other.head);
			swap(tail,other.tail);
			swap(count,other.count);
		}
		~linked_list(void){
			clear();
		}
		size_t size(void) const{
			return count;
		}
		bool empty(void) const{
			return count == 0;
		}
		void clear(void){
			while(count)
				pop_back();
		}
		iterator begin(void){
			return iterator(this,head);
		}
		const_iterator begin(void) const{
			return const_iterator(this,head);
		}
		iterator end(void){
			return iterator(this,nullptr);
		}
		const_iterator end(void) const{
			return const_iterator(this,nullptr);
		}
		T& front(void){
			if (count){
				assert(head && tail);
				return head->payload;
			}
			THROW(this);
		}
		const T& front(void) const{
			if (count){
				assert(head && tail);
				return head->payload;
			}
			THROW(this);
		}
		T& back(void){
			if (count){
				assert(head && tail);
				return tail->payload;
			}
			THROW(this);
		}
		const T& back(void) const{
			if (count){
				assert(head && tail);
				return tail->payload;
			}
			THROW(this);
		}
		template<typename ... Arg>
		void push_back(Arg&& ... args){
			auto new_node = new node(forward<Arg>(args)...);
			if (count){
				assert(head && tail);
				assert(tail->next == nullptr);
				tail->next = new_node;
				new_node->prev = tail;
				tail = new_node;
			}
			else{
				assert(!head && !tail);
				head = tail = new_node;
			}
			++count;
		}
		void pop_back(void){
			if (count){
				assert(head && tail);
				assert(tail->next == nullptr);
				auto cur = tail;
				if (cur->prev){
					assert(count > 1);
					assert(cur->prev->next == cur);
					tail = cur->prev;
					tail->next = nullptr;
				}
				else{
					assert(count == 1);
					assert(head == tail);
					head = tail = nullptr;
				}
				delete cur;
				--count;
				return;
			}
			THROW(this);
		}
		template<typename ... Arg>
		void push_front(Arg&& ... args){
			auto new_node = new node(forward<Arg>(args)...);
			if (count){
				assert(head && tail);
				assert(head->prev == nullptr);
				head->prev = new_node;
				new_node.next = head;
				head = new_node;
			}
			else{
				assert(!head && !tail);
				head = tail = new_node;
			}
			++count;
		}
		void pop_front(void){
			if (count){
				assert(head && tail);
				assert(head->prev == nullptr);
				auto cur = head;
				if (cur->next){
					assert(count > 1);
					assert(cur->next->prev == cur);
					head = cur->next;
					head->prev = nullptr;
				}
				else{
					assert(count == 1);
					assert(head == tail);
					head = tail = nullptr;
				}
				delete cur;
				--count;
				return;
			}
			THROW(this);
		}
	};
}