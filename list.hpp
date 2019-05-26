#pragma once
#include "types.hpp"
#include "assert.hpp"
#include "exception.hpp"
#include "heap.hpp"
#include "util.hpp"
//#include "iterator.hpp"

namespace UOS{
	template<typename T>
	class list{
		struct node{
			node* prev;
			node* next;
			T payload;
		};
		
		
		public:
		
		class iterator {
			mutable node* pos;
			friend class list;
		public:
			iterator(node* p = nullptr) : pos(p){}
			
			bool operator==(const iterator& cmp) const{
				return pos==cmp.pos;
			}
			
			bool operator!=(const iterator& cmp) const{
				return pos!=cmp.pos;
			}
			
			iterator& operator++(void)const {
				if (pos)
					pos=pos->next;
				return *this;
			}
			
			iterator& operator--(void)const {
				if (pos)
					pos=pos->prev;

				return *this;
			}
			T& operator*(void){
				return pos->payload;
			}
			const T& operator*(void) const{
				return pos->payload;
			}
			
			
		};
		
		private:
			node* head;
			node* tail;
			size_t count;
			
			friend class iterator;
			
		public:
		
		list(void) : head(nullptr),tail(nullptr),count(0){}
		
		list(const list&) = delete;
		
		list& operator=(const list&) = delete;
		
		~list(void){
			clear();
		}
		
		void swap(list& other){
			swap(head,other.head);
			swap(tail,other.tail);
			swap(count,other.count);
		}
			
		
		size_t size(void) const{
			return count;
		}
		
		bool empty(void) const{
			return !count;
		}
		T& front(void) {
			if (head)
				return head->payload;
			throw out_of_range();
		}
		
		const T& front(void) const{
			if (head)
				return head->payload;
			throw out_of_range();
		}
		
		T& back(void) {
			if (tail)
				return tail->payload;
			throw out_of_range();
		}
		
		const T& back(void) const{
			if (tail)
				return tail->payload;
			throw out_of_range();
		}
		
		void clear(void){
			while(head!=tail){
				node* cur=head;
				head=head->next;
				delete cur;
			}
			if (head){
				delete head;
				head=tail=nullptr;
			}
			count=0;
		}
		
		void push_front(const T& val){
			head=new node{nullptr,head,val};
			if (!tail){
				assert(!count);
				tail=head;
			}
			count++;
		}
		
		void push_back(const T& val){
			tail=new node{tail,nullptr,val};
			if (!head){
				assert(!count);
				head=tail;
			}
			count++;
		}
		
		void pop_front(void){
			if (!head)
				throw out_of_range;
			node* cur=head;
			head=head->next;
			delete cur;
			count--;
			if (!head){
				assert(!count);
				tail=head;
			}
		}
		
		void pop_back(void){
			if (!tail)
				throw out_of_range;
			node* cur=tail;
			tail=tail->prev;
			delete cur;
			count--;
			if (!tail){
				assert(!count);
				head=tail;
			}
		}
		iterator begin(void){
			return iterator(head);
		}
		iterator end(void){
			return iterator();
		}
		const iterator cbegin(void){
			return iterator(head);
		}
		const iterator cend(void){
			return iterator();
		}
		
		iterator insert(const iterator it,const T& val){
			if (!it.pos){
				push_back(val);
				return iterator(tail);
			}
			node* cur=new node{it.pos->prev,it.pos,val};
			if (cur->prev)
				cur->prev->next=cur;
			else
				head=cur;
			
			it.pos->prev=cur;
			count++;
			return iterator(cur);
			
		}
		
		iterator erase(const iterator it){
			if (!it.pos)
				throw out_of_range();
			node* ret=it.pos->next;
			if (it.pos->next)
				it.pos->next->prev=it.pos->prev;
			else
				tail=it.pos->prev;
			
			if (it.pos->prev)
				it.pos->prev->next=it.pos->next;
			else
				head=it.pos->next;
			
			delete it.pos;
			count--;
			return iterator(ret);
		}
	};
	
	
	
	
	
	
}