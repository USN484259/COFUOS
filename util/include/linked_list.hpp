#pragma once
#include "types.h"
#include "container.hpp"
#include "util.hpp"
#include "assert.hpp"


namespace UOS{
	template<typename T>
	class linked_list{
	public:
		class node{
			friend class linked_list;
			node* prev = nullptr;
			node* next = nullptr;
		public:
			T payload;

			template<typename ... Arg>
			node(Arg&& ... args) : payload(forward<Arg>(args)...) {}
		};
	private:
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
					THROW("iterator move past the end @ %p",this);
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
					THROW("iterator move past the begin @ %p",this);
				}
				return *this;
			}
			bool operator==(const iterator_base& cmp) const{
				if (owner == cmp.owner){
					return ptr == cmp.ptr;
				}
				THROW("compare iterators of different containers (%p,%p)",this,&cmp);
			}
			bool operator!=(const iterator_base& cmp) const{
				if (owner == cmp.owner){
					return ptr != cmp.ptr;
				}
				THROW("compare iterators of different containers (%p,%p)",this,&cmp);
			}
			const T& operator*(void) const{
				if (ptr)
					return ptr->payload;
				THROW("deref empty iterator @ %p",this);
			}
			const T* operator->(void) const{
				if (ptr)
					return &ptr->payload;
				THROW("deref empty iterator @ %p",this);
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
				this->owner = other.owner;
				this->ptr = other.ptr;
				return *this;
			}
			using iterator_base::operator*;
			using iterator_base::operator->;
			T& operator*(void){
				if (this->ptr)
					return this->ptr->payload;
				THROW("deref empty iterator @ %p",this);
			}
			T* operator->(void){
				if (this->ptr)
					return &this->ptr->payload;
				THROW("deref empty iterator @ %p",this);
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
				this->owner = other.owner;
				this->ptr = other.ptr;
				return *this;
			}
			const_iterator& operator=(const iterator& other){
				this->owner = other.owner;
				this->ptr = other.ptr;
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
			THROW("access empty list @ %p",this);
		}
		const T& front(void) const{
			if (count){
				assert(head && tail);
				return head->payload;
			}
			THROW("access empty list @ %p",this);
		}
		T& back(void){
			if (count){
				assert(head && tail);
				return tail->payload;
			}
			THROW("access empty list @ %p",this);
		}
		const T& back(void) const{
			if (count){
				assert(head && tail);
				return tail->payload;
			}
			THROW("access empty list @ %p",this);
		}
		template<typename ... Arg>
		void push_back(Arg&& ... args){
			auto new_node = new node(forward<Arg>(args)...);
			put_node(end(),new_node);
		}
		/*
		template<>
		void push_back(node* new_node){
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
		*/
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
			THROW("access empty list @ %p",this);
		}
		template<typename ... Arg>
		void push_front(Arg&& ... args){
			auto new_node = new node(forward<Arg>(args)...);
			put_node(begin(),new_node);
		}
		/*
		template<>
		void push_front(node* new_node){
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
		*/
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
			THROW("access empty list @ %p",this);
		}

		template<typename ... Arg>
		iterator insert(const_iterator pos,Arg&& ... args){
			auto new_node = new node(forward<Arg>(args)...);
			put_node(pos,new_node);
			return iterator(this,new_node);
		}

		iterator erase(const_iterator pos){
			if (pos.owner != this || pos == end())
				THROW("invalid iterator %p for erase @ %p",&pos,this);
			assert(count && head && tail);
			node* next = pos.ptr->next;
			delete get_node(pos);

			if (count == 0)
				assert(head == nullptr && tail == nullptr);
			return iterator(this,next);
		}
		void splice(const_iterator pos,linked_list& other,const_iterator it){
			if (pos.owner != this || it.owner != &other || it == other.end())
				THROW("invalid iterator (%p,%p) for splice @ (%p,%p)",&pos,&it,this,&other);
			assert(other.count && other.head && other.tail);
			put_node(pos,other.get_node(it));
		}
		node* get_node(const_iterator it){
			if (it.owner != this || it == end())
				THROW("invalid iterator %p for get_node @ %p",&it,this);
			node* pos = it.ptr;
			if (pos->prev){
				pos->prev->next = pos->next;
			}
			else{
				assert(pos == head);
				head = pos->next;
			}
			if (pos->next){
				pos->next->prev = pos->prev;
			}
			else{
				assert(pos == tail);
				tail = pos->prev;
			}
			--count;
			return pos;
		}
		void put_node(const_iterator it,node* cur){
			if (it.owner != this)
				THROW("invalid iterator %p for put_node @ %p",&it,this);
			node* pos = it.ptr;
			if (pos){
				cur->prev = pos->prev;
				cur->next = pos;
				if (pos->prev){
					pos->prev->next = cur;
				}
				else{
					assert(pos == head);
					head = cur;
				}
				pos->prev = cur;
			}
			else{
				cur->next = nullptr;
				if (count){
					assert(head && tail);
					assert(tail->next == nullptr);
					tail->next = cur;
					cur->prev = tail;

					tail = cur;
				}
				else{
					assert(head == nullptr && tail == nullptr);
					cur->prev = nullptr;
					head = tail = cur;
				}
			}
			++count;
		}
	};
}