#pragma once
#include "common.hpp"

namespace UOS{

	template<typename T,typename C = UOS::less<T> >
	class avl_tree{
		
		enum LEAN : byte {LEAN_BALANCE = 0,LEAN_LEFT = 1,LEAN_RIGHT = 2};
		enum SIDE : byte { NONE, LEFT, RIGHT };
		struct node {
			node* l;
			node* r;
			LEAN  lean : 2;
			SIDE side : 2;
			byte : 2;
			//0:ref		1:link
			byte type_l : 1;
			byte type_r : 1;
			T payload;
			
			node(const T& t) : l(nullptr), r(nullptr), lean(LEAN_BALANCE), side(NONE), type_l(0), type_r(0), payload(t) {}
			node(T&& t) : l(nullptr), r(nullptr), lean(LEAN_BALANCE), side(NONE), type_l(0), type_r(0), payload(move(t)) {}
			
			node* left(void) const{
				return type_l ? l : nullptr;
			}
			node* right(void) const{
				return type_r ? r : nullptr;
			}
			
			void left(node* p){
				l = p;
				type_l = p ? 1 : 0;
				if (p)
					p->side = LEFT;
			}
			
			void right(node* p){
				r = p;
				type_r = p ? 1 : 0;
				if (p)
					p->side = RIGHT;
			}
			
			node* prev(void) const{
				if (!type_l)
					return l;
				node* res = l;
				assert(res);
				while (res->right())
					res = res->right();
				assert(res->ref_r() == this);
				return res;

			}
			node* next(void) const{
				if (!type_r)
					return r;
				node* res = r;
				assert(res);
				while (res->left())
					res = res->left();
				assert(res->ref_l() == this);
				return res;
			}
			
			node* ref_l(void) const {
				assert(!type_l);
				return l;
			}
			node* ref_r(void) const {
				assert(!type_r);
				return r;
			}

			void ref_l(node* p){
				assert(!type_l);
				l = p;
			}
			
			void ref_r(node* p){
				assert(!type_r);
				r = p;
			}
			
			
		};
		struct CMP : public C{
			bool operator()(const node* a,const node* b) const{
				return C::operator()(a->payload,b->payload);
			}
		};

		node* root;
		size_t count;
		CMP cmp;
		
		size_t depth(const node* pos) const {
			if (!pos)
				return 0;
			return 1 + max(depth(pos->left()), depth(pos->right()));
		}

		LEAN get_lean(const node* pos) const {
			size_t left = depth(pos->left());
			size_t right = depth(pos->right());
			if (left == right)
				return LEAN_BALANCE;
			if (left + 1 == right)
				return LEAN_RIGHT;
			if (right + 1 == left)
				return LEAN_LEFT;
			error(corrupted,pos);
		}
		/*
		LEAN update_lean(node* pos) {
			if (!pos)
				return LEAN_BALANCE;
			if (pos->lean != LEAN_UNKNOWN)
				return pos->lean;

			LEAN left = update_lean(pos->left);
			LEAN right = update_lean(pos->right);


		}
		*/

		void clear(node* pos) {
			if (!pos)
				return;
			clear(pos->left());
			clear(pos->right());
			delete pos;
		}


		enum DEPTH_CHANGE {HOLD,INC,DEC};
		
		typedef pair<node*, DEPTH_CHANGE> result_type;

		 result_type insert(node* pos,node* item){
			assert(item);
			assert(!item->left() && !item->right());
			assert(item->lean == LEAN_BALANCE);

			if (!pos)
				return result_type(item, INC);

			SIDE side;
			
			if (cmp(item,pos))
				side = LEFT;
			else if (cmp(pos,item))
				side = RIGHT;
			else
				side = pos->lean == LEAN_LEFT ? RIGHT : LEFT;
			assert(side == LEFT || side == RIGHT);
			node* ptr = (side == LEFT ? pos->left() : pos->right());
			if (!ptr) {
				switch (side) {
				case LEFT:
					item->ref_r(pos);
					item->ref_l(pos->ref_l());
					break;
				case RIGHT:
					item->ref_l(pos);
					item->ref_r(pos->ref_r());
					break;
				}
			}
			auto res = insert(ptr, item);
			

			switch (side) {
			case LEFT:
				pos->left(res.first);
				break;
			case RIGHT:
				pos->right(res.first);
				break;
			}

			LEAN new_lean;
			switch(res.second){
			case HOLD:
				new_lean = LEAN_BALANCE;
				break;
			case INC:
				new_lean = (side == LEFT ? LEAN_LEFT : LEAN_RIGHT);
				break;
			case DEC:
				new_lean = (side == LEFT ? LEAN_RIGHT : LEAN_LEFT);
				break;
			default:
				assert(false);
			}
			if (new_lean == LEAN_BALANCE)
				return result_type(pos,HOLD);
			if (pos->lean == LEAN_BALANCE){
				assert(new_lean == LEAN_LEFT || new_lean == LEAN_RIGHT);
				pos->lean = new_lean;
				return result_type(pos,INC);
			}
			
			if (pos->lean != new_lean){
				
				assert((pos->lean == LEAN_LEFT && new_lean == LEAN_RIGHT) || (pos->lean == LEAN_RIGHT && new_lean == LEAN_LEFT));
				pos->lean = LEAN_BALANCE;
				return result_type(pos,HOLD);
			}
			
			if (pos->lean == LEAN_LEFT){
				assert(new_lean == LEAN_LEFT);
				return result_type(rotate_right(pos), HOLD);
			}
			
			if (pos->lean == LEAN_RIGHT){
				assert(new_lean == LEAN_RIGHT);
				return result_type(rotate_left(pos), HOLD);
			}
		
			assert(false);
		}
		
		node* rotate_left(node* pos) {
			assert(pos->lean == LEAN_RIGHT);
			assert(pos->right());
			switch (pos->right()->lean) {
			case LEAN_LEFT:		//RL
			{
				node* new_root = pos->right()->left();
				assert(new_root);
				node* new_left = pos;
				node* new_right = pos->right();

				if (new_left->left() && new_right->right()) {

					new_left->right(new_root->left());
					new_right->left(new_root->right());

					switch (new_root->lean) {
					case LEAN_LEFT:
						assert(new_root->left());
						if (!new_root->right()) {
							assert(new_root->ref_r() == new_right);
							new_right->ref_l(new_root);
						}
						break;
					case LEAN_RIGHT:
						assert(new_root->right());
						if (!new_root->left()) {
							assert(new_root->ref_l() == new_left);
							new_left->ref_r(new_root);
						}
						break;
					default:
						assert(false);
					}
				}
				else {
					assert(!new_left->left() && !new_right->right());
					assert(new_root->ref_l() == new_left);
					assert(new_root->ref_r() == new_right);

					new_left->right(nullptr);
					new_right->left(nullptr);

					new_left->ref_r(new_root);
					new_right->ref_l(new_root);
				}
				//new_left->right(new_root->left());
				//new_right->left(new_root->right());

				new_root->left(new_left);
				new_root->right(new_right);

				new_left->lean = (new_root->lean == LEAN_RIGHT ? LEAN_LEFT : LEAN_BALANCE);
				new_right->lean = (new_root->lean == LEAN_LEFT ? LEAN_RIGHT : LEAN_BALANCE);

				new_root->lean = LEAN_BALANCE;
				pos = new_root;

			}
				break;
			case LEAN_RIGHT:	//RR
			{
				node* new_root = pos->right();
				node* new_left = pos;

				if (new_left->left() && new_root->left())
					new_left->right(new_root->left());
				else {
					assert(!new_left->left() && !new_root->right()->right());
					assert(new_root->right()->ref_l() == new_root);
					assert(new_root->ref_l() == new_left);

					new_left->right(nullptr);
					new_left->ref_r(new_root);
				}

				//new_left->right(new_root->left());
				new_root->left(new_left);

				new_left->lean = LEAN_BALANCE;
				new_root->lean = LEAN_BALANCE;

				pos = new_root;
			}
				break;
			default:
				error(corrupted,pos);
			}

			assert(get_lean(pos) == pos->lean);
			assert(get_lean(pos->left()) == pos->left()->lean);
			assert(get_lean(pos->right()) == pos->right()->lean);
			return pos;
		}
		
		node* rotate_right(node* pos) {
			assert(pos->lean == LEAN_LEFT);
			assert(pos->left());
			switch (pos->left()->lean)
			{
			case LEAN_LEFT:		//LL
			{
				node* new_root = pos->left();
				node* new_right = pos;

				if (new_right->right() && new_root->right())
					new_right->left(new_root->right());
				else {
					assert(!new_right->right() && !new_root->left()->left());
					assert(new_root->left()->ref_r() == new_root);
					assert(new_root->ref_r() == new_right);

					new_right->left(nullptr);
					new_right->ref_l(new_root);
				}
				//new_right->left(new_root->right());
				new_root->right(new_right);

				new_right->lean = LEAN_BALANCE;
				new_root->lean = LEAN_BALANCE;

				pos = new_root;
			}
				break;
			case LEAN_RIGHT:	//LR
			{
				node* new_root = pos->left()->right();
				assert(new_root);
				node* new_left = pos->left();
				node* new_right = pos;

				if (new_left->left() && new_right->right()) {

					new_left->right(new_root->left());
					new_right->left(new_root->right());

					switch (new_root->lean) {
					case LEAN_LEFT:
						assert(new_root->left());
						if (!new_root->right()) {
							assert(new_root->ref_r() == new_right);
							new_right->ref_l(new_root);
						}
						break;
					case LEAN_RIGHT:
						assert(new_root->right());
						if (!new_root->left()) {
							assert(new_root->ref_l() == new_left);
							new_left->ref_r(new_root);
						}
						break;
					default:
						assert(false);
					}
				}
				else {
					assert(!new_left->left() && !new_right->right());
					assert(new_root->ref_l() == new_left);
					assert(new_root->ref_r() == new_right);

					new_left->right(nullptr);
					new_right->left(nullptr);

					new_left->ref_r(new_root);
					new_right->ref_l(new_root);
				}
				//new_left->right(new_root->left());
				//new_right->left(new_root->right());

				new_root->left(new_left);
				new_root->right(new_right);

				new_left->lean = (new_root->lean == LEAN_RIGHT ? LEAN_LEFT : LEAN_BALANCE);
				new_right->lean = (new_root->lean == LEAN_LEFT ? LEAN_RIGHT : LEAN_BALANCE);

				new_root->lean = LEAN_BALANCE;
				pos = new_root;

			}
				break;
			default:
				error(corrupted,pos);
			}
			assert(get_lean(pos) == pos->lean);
			assert(get_lean(pos->left()) == pos->left()->lean);
			assert(get_lean(pos->right()) == pos->right()->lean);
			return pos;
		}
		
		template<typename F>
		void check(F& fun,const node* pos) const {
			assert(get_lean(pos) == pos->lean);
			if (pos->left()) {
				assert(pos->left()->side == LEFT);
				check(fun, pos->left());
			}
			fun(pos->payload);
			if (pos->right()) {
				assert(pos->right()->side == RIGHT);
				check(fun, pos->right());
			}
		}

	public:
		
		avl_tree(void) : root(nullptr),count(0) {}
		avl_tree(C& c) : root(nullptr), count(0), cmp(move(c)) {}
		avl_tree(const avl_tree&) = delete;
		avl_tree& operator=(const avl_tree&) = delete;

		~avl_tree(void) {
			clear();
		}

		void swap(avl_tree& other) {
			using UOS::swap;
			swap(root, other.root);
			swap(count, other.count);
			swap(cmp, other.cmp);
		}

		void clear(void) {
			clear(root);
		}

		size_t size(void) const {
			return count;
		}

		void insert(const T& val) {
			node* new_node = new node(val);
			root = insert(root, new_node).first;
			++count;
		}

		template<typename F>
		void check(F& fun) {
			if (!root)
				return;

			check(fun, root);

			node* head = root;
			while (head->left())
				head = head->left();

			size_t cnt = 0;
			while (head) {
				++cnt;
				head = head->next();
			}

			assert(cnt == count);

		}
		
		//iterator insert(const_iterator hint, const T& val) {

		//}


	};

}
