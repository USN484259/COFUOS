#pragma once
#include "types.hpp"
#include "container.hpp"
#include "util.hpp"

namespace UOS{
    template<typename T>
    class simple_stack{
        struct node{
            T payload;
            node* next;

            template<typename ... Arg>
            node(node* p,Arg&& ... args) : payload(forward<Arg>(args)...), next(p) {}
        };
        node* head;
    public:
        simple_stack(void) : head(nullptr) {}
        simple_stack(const simple_stack&) = delete;
        simple_stack(simple_stack&& other) : head(other.head){
            other.head = nullptr;
        }
        ~simple_stack(void){
            clear();
        }
        bool empty(void) const{
            return head == nullptr;
        }
        T& top(void){
            if (head == nullptr)
                THROW(this);
            return head->payload;
        }
        const T& top(void) const{
            if (head == nullptr)
                THROW(this);
            return head->payload;
        }
        template<typename ... Arg>
        void push(Arg&& ... args){
            head = new node(head,forward<Arg>(args)...);
        }
        void pop(void){
            if (head == nullptr)
                THROW(this);
            auto next = head->next;
            delete head;
            head = next;
        }
        void clear(void){
            while(head)
                pop();
        }
        class iterator {
            friend class simple_stack;
            node* cur;
            iterator(node* p) : cur(p){}
        public:
            iterator(const iterator&) = default;
            iterator& operator=(const iterator&) = default;
            T& operator*(void){
                return cur->payload;
            }
            const T& operator*(void) const{
                return cur->payload;
            }
            bool operator==(const iterator& other) const{
                return cur == other.cur;
            }
            bool operator!=(const iterator& other) const{
                return cur != other.cur;
            }
            iterator& operator++(void){
                if (cur == nullptr)
                    THROW(this);
                cur = cur->next;
                return *this;
            }
        };
        friend class iterator;
        iterator begin(void){
            return iterator(head);
        }
        iterator end(void){
            return iterator(nullptr);
        }
    };
}