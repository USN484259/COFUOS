#pragma once
#include "common.hpp"

#ifndef BRANCH_TEST
#define BRANCH_TEST
#endif

//#define BRANCH_TEST_INSERT
//#define BRANCH_TEST_ROTATE
#define BRANCH_TEST_ERASE


namespace UOS {

	template<typename T, typename C = UOS::less<T> >
	class avl_tree {

		enum LEAN : byte { LEAN_BALANCE = 0, LEAN_LEFT = 1, LEAN_RIGHT = 2 };
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

				   void set(const node* other) {
					   l = other->l;
					   r = other->r;
					   lean = other->lean;
					   side = other->side;
					   type_l = other->type_l;
					   type_r = other->type_r;
				   }

				   inline node* left(void) const {
					   return type_l ? l : nullptr;
				   }
				   inline node* right(void) const {
					   return type_r ? r : nullptr;
				   }

				   inline void left(node* p) {
					   l = p;
					   type_l = p ? 1 : 0;
					   if (p)
						   p->side = LEFT;
				   }

				   inline void right(node* p) {
					   r = p;
					   type_r = p ? 1 : 0;
					   if (p)
						   p->side = RIGHT;
				   }

				   node* prev(void) const {
					   if (!type_l)
						   return l;
					   node* res = l;
					   assert(res);
					   while (res->right())
						   res = res->right();
					   assert(res->ref_r() == this);
					   return res;

				   }
				   node* next(void) const {
					   if (!type_r)
						   return r;
					   node* res = r;
					   assert(res);
					   while (res->left())
						   res = res->left();
					   assert(res->ref_l() == this);
					   return res;
				   }

				   inline node* ref_l(void) const {
					   assert(!type_l);
					   return l;
				   }
				   inline node* ref_r(void) const {
					   assert(!type_r);
					   return r;
				   }

				   inline void ref_l(node* p) {
					   assert(!type_l);
					   l = p;
				   }

				   inline void ref_r(node* p) {
					   assert(!type_r);
					   r = p;
				   }

				   node* parent(void) const {
					   node* res = (node*)this;
					   switch (side) {
					   case NONE:
						   return nullptr;
					   case LEFT:
						   while (res->right())
							   res = res->right();
						   res = res->ref_r();
						   assert(res->left() == this);
						   break;
					   case RIGHT:
						   while (res->left()) {
							   res = res->left();
						   }
						   res = res->ref_l();
						   assert(res->right() == this);
						   break;
					   default:
						   assert(false);
					   }
					   return res;
				   }

				   inline bool is_leaf(void) const {
					   if (left() || right())
						   ;
					   else {
						   assert(lean == LEAN_BALANCE);
						   return true;
					   }
					   if (!left())
						   assert(lean == LEAN_RIGHT);
					   if (!right())
						   assert(lean == LEAN_LEFT);

					   return false;
				   }

				   //void raw_set(const node* other, SIDE mode) {
				   //	if (mode != RIGHT) {
				   //		l = other->l;
				   //		type_l = other->type_l;
				   //	}
				   //	if (mode != LEFT) {
				   //		r = other->r;
				   //		type_r = other->type_r;
				   //	}
				   //	if (mode == NONE) {
				   //		lean = other->lean;
				   //		side = other->side;
				   //	}
				   //}
		};

		class iterator_base {
			friend class avl_tree;
		protected:
			const avl_tree* container;
			node* pos;
			iterator_base(const avl_tree* c, node* p) : container(c), pos(p) {}

		public:

			iterator_base& operator++(void) {
				assert(container);
				if (pos)
					pos = pos->next();
				else {
					pos = container->root;
					if (!pos)
						error(null_deref, container);
					while (pos->left())
						pos = pos->left();
				}
				return *this;
			}
			iterator_base& operator--(void) {
				assert(container);
				if (pos)
					pos = pos->prev();
				else {
					pos = container->root;
					if (!pos)
						error(null_deref, container);
					while (pos->right())
						pos = pos->right();
				}
				return *this;
			}

			bool operator==(const iterator_base& cmp) const {
				if (container != cmp.container)
					error(invalid_argument, cmp);
				return pos == cmp.pos;
			}
			bool operator!=(const iterator_base& cmp) const {
				if (container != cmp.container)
					error(invalid_argument, cmp);
				return pos != cmp.pos;
			}

			const T& operator*(void) const {
				assert(container && pos);
				return pos->payload;
			}
			const T* operator->(void) const {
				assert(container && pos);
				return &pos->payload;
			}

		};
	public:
		class iterator : public iterator_base {
			friend class avl_tree;
			typedef iterator_base super;

			iterator(const avl_tree* c, node* p) : super(c, p) {}

		public:
			iterator(void) : super(nullptr, nullptr) {}
			iterator(const iterator& obj) : super(obj.container, obj.pos) {}

			iterator& operator=(const iterator& obj) {
				container = obj.container;
				pos = obj.pos;
				return *this;
			}

			using super::operator*;
			using super::operator->;

			T& operator*(void) {
				assert(container && pos);
				return pos->payload;
			}
			T* operator->(void) {
				assert(container && pos);
				return &pos->payload;
			}

		};


		class const_iterator : public iterator_base {
			friend class avl_tree;

			typedef iterator_base super;

			const_iterator(const avl_tree* c, node* p) : super(c, p) {}

		public:
			const_iterator(void) : super(nullptr, nullptr) {}
			const_iterator(const const_iterator& obj) : super(obj.container, obj.pos) {}
			const_iterator(const iterator& obj) : super(obj.container, obj.pos) {}

			const_iterator& operator=(const const_iterator& obj) {
				container = obj.container;
				pos = obj.pos;
				return *this;
			}

			const_iterator& operator=(const iterator& obj) {
				container = obj.container;
				pos = obj.pos;
				return *this;
			}

		};

	private:
		/*
		struct CMP : public C{
			CMP(void) = default;
			CMP(const C& c) : C(c) {}

			bool operator()(const node* a,const node* b) const{
				return C::operator()(a->payload,b->payload);
			}

			template<typename T>
			bool operator()(const node* a, const T& b) const {
				return operator()(a->payload, b);
			}
			template<typename T>
			bool operator()(const T& a, const node* b) const {
				return operator()(a, b->payload);
			}
		};
		*/
		node* root;
		size_t count;
		C cmp;

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
			error(corrupted, pos);
		}

		void clear(node* pos) {
			if (!pos)
				return;
			clear(pos->left());
			clear(pos->right());
			delete pos;
		}


		enum DEPTH_CHANGE { HOLD, INC, DEC };

		typedef pair<node*, DEPTH_CHANGE> result_type;

#ifdef BRANCH_TEST_INSERT
#define BRANCH BRANCH_TEST
#endif

#ifndef BRANCH
#define BRANCH
#endif

		result_type insert(node* pos, node* item) {
			assert(item);
			assert(!item->left() && !item->right());
			assert(item->lean == LEAN_BALANCE);

			if (!pos) {
				BRANCH;
				return result_type(item, INC);
			}

			SIDE side;

			if (cmp(item->payload, pos->payload)) {
				BRANCH;
				side = LEFT;
			}
			else if (cmp(pos->payload, item->payload)) {
				BRANCH;
				side = RIGHT;
			}
			else {
				BRANCH;
				side = pos->lean == LEAN_LEFT ? RIGHT : LEFT;
			}
			assert(side == LEFT || side == RIGHT);
			node* ptr = (side == LEFT ? pos->left() : pos->right());
			if (!ptr) {
				switch (side) {
				case LEFT:
					BRANCH;
					item->ref_r(pos);
					item->ref_l(pos->ref_l());
					break;
				case RIGHT:
					BRANCH;
					item->ref_l(pos);
					item->ref_r(pos->ref_r());
					break;
				}
			}
			auto res = insert(ptr, item);


			switch (side) {
			case LEFT:
				BRANCH;
				pos->left(res.first);
				break;
			case RIGHT:
				BRANCH;
				pos->right(res.first);
				break;
			}

			LEAN new_lean;
			switch (res.second) {
			case HOLD:
				BRANCH;
				new_lean = LEAN_BALANCE;
				break;
			case INC:
				BRANCH;
				new_lean = (side == LEFT ? LEAN_LEFT : LEAN_RIGHT);
				break;
			case DEC:
				assert(false);
				//BRANCH;
				new_lean = (side == LEFT ? LEAN_RIGHT : LEAN_LEFT);
				break;
			default:
				assert(false);
			}
			if (new_lean == LEAN_BALANCE) {
				BRANCH;
				return result_type(pos, HOLD);
			}
			if (pos->lean == LEAN_BALANCE) {
				BRANCH;
				assert(new_lean == LEAN_LEFT || new_lean == LEAN_RIGHT);
				pos->lean = new_lean;
				return result_type(pos, INC);
			}

			if (pos->lean != new_lean) {
				BRANCH;
				assert((pos->lean == LEAN_LEFT && new_lean == LEAN_RIGHT) || (pos->lean == LEAN_RIGHT && new_lean == LEAN_LEFT));
				pos->lean = LEAN_BALANCE;
				return result_type(pos, HOLD);
			}

			if (pos->lean == LEAN_LEFT) {
				BRANCH;
				assert(new_lean == LEAN_LEFT);
				auto res = rotate_right(pos);
				assert(res.second == DEC);
				return result_type(res.first, HOLD);
			}

			if (pos->lean == LEAN_RIGHT) {
				BRANCH;
				assert(new_lean == LEAN_RIGHT);
				auto res = rotate_left(pos);
				assert(res.second == DEC);
				return result_type(res.first, HOLD);
			}

			assert(false);
		}

#undef BRANCH

#ifdef BRANCH_TEST_ERASE
#define BRANCH BRANCH_TEST
#endif

#ifndef BRANCH
#define BRANCH
#endif

		void erase(node* target) {
			assert(target);

			node* target_parent = target->parent();
			SIDE target_side = target->side;

			node* replace = nullptr;
			SIDE replace_side = NONE;

			node* rebalance = nullptr;
			//SIDE deleted_side = NONE;
			LEAN new_lean = LEAN_BALANCE;

			if (!target->is_leaf()) {	//target is not leaf
				BRANCH;
				switch (target->lean) {
				case LEAN_BALANCE:
					BRANCH;
					assert(target->left() && target->right());
					//TRICK select left aka prev()
				case LEAN_LEFT:
					BRANCH;
					replace = target->prev();
					replace_side = LEFT;
					break;
				case LEAN_RIGHT:
					BRANCH;
					replace = target->next();
					replace_side = RIGHT;
					break;
				default:
					assert(false);
				}
			}
			assert(!replace || replace_side != NONE);

			if (replace) {	//target is not leaf
				BRANCH;
				node* replace_parent = replace->parent();

				/*	remove ref chain together with replace removal
				{	//remove replace from ref chain
					node* prev = replace->prev();
					node* next = replace->next();
					if (prev && !prev->right()) {	//right is ref
						BRANCH;
						assert(prev->ref_r() == replace);
						prev->ref_r(next);
					}
					if (next && !next->left()) {	//left is ref
						BRANCH;
						assert(next->ref_l() == replace);
						next->ref_l(prev);
					}
				}	*/
				//delete replace from replace_parent's link
				switch (replace_side) {
				case LEFT:
					BRANCH;
					assert(replace->ref_r() == target);
					if (replace->left()) {
						BRANCH;
						assert(replace->left()->is_leaf());
						assert(replace->prev()->ref_r() == replace);
						replace->prev()->ref_r(target);

						if (replace_parent == target) {
							BRANCH;
							assert(replace_parent->left() == replace);
							replace_parent->left(replace->left());

							//replace_parent -L
							rebalance = replace;	//target will be replaced
							new_lean = LEAN_RIGHT;
						}
						else {
							BRANCH;
							assert(replace_parent->right() == replace);
							replace_parent->right(replace->left());

							//replace_parent -R
							rebalance = replace_parent;
							new_lean = LEAN_LEFT;
						}
					}
					else {
						BRANCH;
						assert(replace->is_leaf());
						if (replace_parent == target) {
							BRANCH;
							assert(replace_parent->left() == replace);
							replace_parent->left(nullptr);
							replace_parent->ref_l(replace->ref_l());	//???
						}
						else {
							BRANCH;
							assert(replace_parent->right() == replace);
							replace_parent->right(nullptr);
							replace_parent->ref_r(target);
						}
#error "rebalance ?"
					}
					break;
				case RIGHT:
					BRANCH;
					assert(replace->ref_l() == target);
					if (replace->right()) {
						BRANCH;
						assert(replace->right()->is_leaf());
						assert(replace->next()->ref_l() == replace);
						replace->next()->ref_l(target);

						if (replace_parent == target) {
							BRANCH;
							assert(replace_parent->right() == replace);
							replace_parent->right(replace->right());

							//replace_parent -R
							rebalance = replace;	//target will be replaced
							new_lean = LEAN_LEFT;
						}
						else {
							BRANCH;
							assert(replace_parent->left() == replace);
							replace_parent->left(replace->right());

							//replace_parent -L
							rebalance = replace_parent;
							new_lean = LEAN_RIGHT;
						}
					}
					else {
						BRANCH;
						assert(replace->is_leaf());
						if (replace_parent == target) {
							BRANCH;
							assert(replace_parent->right() == replace);
							replace_parent->right(nullptr);
							replace_parent->ref_r(replace->ref_r());	//???
						}
						else {
							BRANCH;
							assert(replace_parent->left() == replace);
							replace_parent->left(nullptr);
							replace_parent->ref_l(target);
						}
#error "rebalance ?"
					}
					break;
				}

				//at this point only replace is removed from the container and the chain is valid except that lean may be outdated


				{	//redirect ref pointing to target to replace
					node* prev = target->prev();
					node* next = target->next();
					if (prev && !prev->right()) {	//right is ref
						BRANCH;
						assert(prev->ref_r() == target);
						prev->ref_r(replace);
					}
					if (next && !next->left()) {	//left is ref
						BRANCH;
						assert(next->ref_l() == target);
						next->ref_l(replace);
					}
				}

				//replace target
				replace->set(target);


				//point target_parent's link to replace
				switch (target_side) {
				case NONE:
					BRANCH;
					assert(nullptr == target_parent);
					assert(root == target);
					root = replace;
					replace->side = NONE;
					break;
				case LEFT:
					BRANCH;
					target_parent->left(replace);
					break;
				case RIGHT:
					BRANCH;
					target_parent->right(replace);
					break;
				}

				//rebalance = replace_parent == target ? replace : replace_parent;	//????
				//deleted_side = replace_side;

			}
			else {	//target is leaf
				BRANCH;
				switch (target_side) {
				case NONE:
					BRANCH;
					assert(nullptr == target_parent);
					assert(root == target);
					root = nullptr;
					break;
				case LEFT:
					BRANCH;
					assert(target->ref_r() == target_parent);
					target_parent->left(nullptr);
					target_parent->ref_l(target->ref_l());

					rebalance = target_parent;
					new_lean = LEAN_RIGHT;
					break;
				case RIGHT:
					BRANCH;
					assert(target->ref_l() == target_parent);
					target_parent->right(nullptr);
					target_parent->ref_r(target->ref_r());

					rebalance = target_parent;
					new_lean = LEAN_LEFT;
					break;

				}

			}
			//LEAN target_lean = target->lean;
			delete target;

			//lean not updated yet
//#pragma message("for TEST rebalance not activated")
//			return;

			bool allow_weird_transform = true;
			while (rebalance && new_lean != LEAN_BALANCE) {
				if (rebalance->lean == LEAN_BALANCE) {
					rebalance->lean = new_lean;
					break;
				}

				node* parent = rebalance->parent();
				SIDE parent_side = rebalance->side;
				node* new_root = nullptr;
				DEPTH_CHANGE dc = DEC;

				if (rebalance->lean != new_lean) {
					rebalance->lean = LEAN_BALANCE;
					//depth dec
				}


				if (rebalance->lean == new_lean) {

					result_type res({ nullptr,HOLD });
					switch (new_lean) {
					case LEAN_LEFT:
						res = rotate_right(rebalance, allow_weird_transform);
						dc = res.second;
						new_root = res.first;
						break;
					case LEAN_RIGHT:
						res = rotate_left(rebalance, allow_weird_transform);
						dc = res.second;
						new_root = res.first;
						break;
					}
					assert(res.second == DEC || (allow_weird_transform && res.second == HOLD));
					if (res.second == HOLD) {
						new_lean = LEAN_BALANCE;
						allow_weird_transform = false;

					}
				}



				if (parent) {
					switch (parent_side) {
					case NONE:
						assert(false);
						break;
					case LEFT:
						if (new_root)
							parent->left(new_root);
						if (dc == DEC)
							new_lean = LEAN_RIGHT;
						break;
					case RIGHT:
						if (new_root)
							parent->right(new_root);
						if (dc == DEC)
							new_lean = LEAN_LEFT;
						break;
					}
				}
				else {
					assert(new_root);	//?????
					root = new_root;
					new_root->side = NONE;
				}
				rebalance = parent;
			}


		}

#undef BRANCH

#ifdef BRANCH_TEST_ROTATE
#define BRANCH BRANCH_TEST
#endif


#ifndef BRANCH
#define BRANCH
#endif

#ifdef BRANCH_TEST_ERASE
#define BRANCH_WEIRD BRANCH
#else
#define BRANCH_WEIRD
#endif

		result_type rotate_left(node* pos,bool allow_weird_transform = false) {
			assert(pos->lean == LEAN_RIGHT);
			assert(pos->right());

			DEPTH_CHANGE dc = DEC;

			switch (pos->right()->lean) {
			case LEAN_LEFT:		//RL
			{
				BRANCH;
				node* new_root = pos->right()->left();
				assert(new_root);
				node* new_left = pos;
				node* new_right = pos->right();

				if (new_left->left() && new_right->right()) {
					BRANCH;
					new_left->right(new_root->left());
					new_right->left(new_root->right());

					switch (new_root->lean) {
					case LEAN_LEFT:
						BRANCH;
						assert(new_root->left());
						if (!new_root->right()) {
							BRANCH;
							assert(new_root->ref_r() == new_right);
							new_right->ref_l(new_root);
						}
						break;
					case LEAN_RIGHT:
						BRANCH;
						assert(new_root->right());
						if (!new_root->left()) {
							BRANCH;
							assert(new_root->ref_l() == new_left);
							new_left->ref_r(new_root);
						}
						break;
					default:
						assert(false);
					}
				}
				else {
					BRANCH;
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

			case LEAN_BALANCE:
				BRANCH_WEIRD;
				if (!allow_weird_transform)
					error(corrupted, pos);
				dc = HOLD;
				//TRICK do RR transform

			case LEAN_RIGHT:	//RR
			{
				BRANCH;
				node* new_root = pos->right();
				node* new_left = pos;

				if (new_left->left() && new_root->left()) {
					BRANCH;
					new_left->right(new_root->left());
				}
				else {
					BRANCH;
					assert(!new_left->left() && !new_root->right()->right());
					assert(new_root->right()->ref_l() == new_root);

					if (allow_weird_transform) {
						BRANCH_WEIRD;
						assert(new_root->left()->ref_l() == new_left);
						new_left->right(new_root->left());
					}
					else {
						BRANCH;
						assert(new_root->ref_l() == new_left);
						new_left->right(nullptr);
						new_left->ref_r(new_root);
					}
				}

				//new_left->right(new_root->left());
				new_root->left(new_left);

				new_left->lean = allow_weird_transform ? LEAN_RIGHT : LEAN_BALANCE;
				new_root->lean = allow_weird_transform ? LEAN_LEFT : LEAN_BALANCE;

				pos = new_root;
			}
			break;
			default:
				assert(false);
			}

			assert(get_lean(pos) == pos->lean);
			assert(get_lean(pos->left()) == pos->left()->lean);
			assert(get_lean(pos->right()) == pos->right()->lean);
			return result_type(pos,dc);
		}
		
		result_type rotate_right(node* pos,bool allow_weird_transform = false) {
			assert(pos->lean == LEAN_LEFT);
			assert(pos->left());

			DEPTH_CHANGE dc = DEC;

			switch (pos->left()->lean)
			{
			case LEAN_BALANCE:
				BRANCH_WEIRD;
				if (!allow_weird_transform)
					error(corrupted, pos);
				dc = HOLD;
				//TRICK do LL transform

			case LEAN_LEFT:		//LL
			{
				BRANCH;
				node* new_root = pos->left();
				node* new_right = pos;

				if (new_right->right() && new_root->right()) {
					BRANCH;
					new_right->left(new_root->right());
				}
				else {
					BRANCH;
					assert(!new_right->right() && !new_root->left()->left());
					assert(new_root->left()->ref_r() == new_root);

					if (allow_weird_transform) {
						BRANCH_WEIRD;
						assert(new_root->right()->ref_r() == new_right);
						new_right->left(new_root->right());
					}
					else{
						BRANCH;
						assert(new_root->ref_r() == new_right);
						new_right->left(nullptr);
						new_right->ref_l(new_root);
					}
				}
				//new_right->left(new_root->right());
				new_root->right(new_right);

				new_right->lean = allow_weird_transform ? LEAN_LEFT : LEAN_BALANCE;
				new_root->lean = allow_weird_transform ? LEAN_RIGHT : LEAN_BALANCE;

				pos = new_root;
			}
			break;
			case LEAN_RIGHT:	//LR
			{
				BRANCH;
				node* new_root = pos->left()->right();
				assert(new_root);
				node* new_left = pos->left();
				node* new_right = pos;

				if (new_left->left() && new_right->right()) {
					BRANCH;
					new_left->right(new_root->left());
					new_right->left(new_root->right());

					switch (new_root->lean) {
					case LEAN_LEFT:
						BRANCH;
						assert(new_root->left());
						if (!new_root->right()) {
							BRANCH;
							assert(new_root->ref_r() == new_right);
							new_right->ref_l(new_root);
						}
						break;
					case LEAN_RIGHT:
						BRANCH;
						assert(new_root->right());
						if (!new_root->left()) {
							BRANCH;
							assert(new_root->ref_l() == new_left);
							new_left->ref_r(new_root);
						}
						break;
					default:
						assert(false);
					}
				}
				else {
					BRANCH;
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
				assert(false);
			}
			assert(get_lean(pos) == pos->lean);
			assert(get_lean(pos->left()) == pos->left()->lean);
			assert(get_lean(pos->right()) == pos->right()->lean);
			return result_type(pos,dc);
		}

#undef BRANCH_WEIRD
#undef BRANCH

		template<typename F>
		void check(F& fun, const node* pos) const {
//#pragma message("for TEST depth assert not activated")
			assert(get_lean(pos) == pos->lean);
			if (pos->left()) {
				assert(pos->left() != pos);
				assert(pos->left()->side == LEFT);
				check(fun, pos->left());
			}
			node* prev = pos->prev();
			node* next = pos->next();

			assert(!prev || prev->next() == pos);
			assert(!next || next->prev() == pos);

			switch (pos->side) {
			case NONE:
				assert(!pos->parent());
				break;
			case LEFT:
				assert(pos->parent()->left() == pos);
				break;
			case RIGHT:
				assert(pos->parent()->right() == pos);
				break;
			default:
				assert(false);
			}

			fun(pos->payload);
			if (pos->right()) {
				assert(pos->right() != pos);
				assert(pos->right()->side == RIGHT);
				check(fun, pos->right());
			}
		}

	public:

		avl_tree(void) : root(nullptr), count(0) {}
		avl_tree(const C& c) : root(nullptr), count(0), cmp(c) {}
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
			root = nullptr;
			count = 0;
		}

		size_t size(void) const {
			return count;
		}

		iterator begin(void) {
			if (!root)
				return end();
			iterator it(this, nullptr);
			++it;
			return it;
		}
		const_iterator cbegin(void) const {
			if (!root)
				return cend();
			const_iterator it(this, nullptr);
			++it;
			return it;
		}

		iterator end(void) {
			return iterator(this, nullptr);
		}
		const_iterator cend(void) const {
			return const_iterator(this, nullptr);
		}

		iterator insert(const T& val) {
			node* new_node = new node(val);
			root = insert(root, new_node).first;
			root->side = NONE;
			++count;
			return iterator(this, new_node);
		}
		iterator insert(T&& val) {
			node* new_node = new node(move(val));
			root = insert(root, new_node).first;
			root->side = NONE;
			++count;
			return iterator(this, new_node);
		}

		iterator erase(const_iterator pos) {
			if (pos == cend())
				error(invalid_argument,pos);
			iterator ret(this,pos.pos->next());
			assert(count);
			erase(pos.pos);
			--count;
			return ret;
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


	};

}
