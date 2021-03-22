#pragma once
#include "types.h"
#include "process/include/waitable.hpp"
#include "literal.hpp"
#include "span.hpp"
#include "hash_set.hpp"
#include "sync/include/rwlock.hpp"

namespace UOS{
	class object_manager{
	public:
		static constexpr word max_name_length = 0x1F8;

		struct object{
			literal const name;
			waitable* const obj;
			const dword property;

			object(literal&& str,waitable* ptr,dword mode) : \
				name(move(str)), obj(ptr), property(mode) {}
		};

	private:
		struct hash{
			UOS::hash<literal> h;
			qword operator()(const object& obj){
				return h(obj.name);
			}
			template<typename T>
			qword operator()(const T& lit){
				return UOS::hash<T>()(lit);
			}
		};
		struct equal{
			template<typename T>
			qword operator()(const object& obj,const T& name){
				return (obj.name == name);
			}
		};

		rwlock objlock;
		hash_set<object,hash,equal> table;
		
	public:
		object_manager(void) = default;
		bool put(literal&& name,waitable* obj,dword properties);
		const object* get(const span<char>& name);
		void erase(waitable* obj);
		inline void lock(void){
			objlock.lock(rwlock::SHARED);
		}
		inline bool try_lock(void){
			return objlock.try_lock(rwlock::SHARED);
		}
		inline void unlock(void){
			assert(objlock.is_locked() && !objlock.is_exclusive());
			objlock.unlock();
		}
		inline bool is_locked(void) const{
			return objlock.is_locked();
		}
		inline bool is_exclusive(void) const{
			return objlock.is_exclusive();
		}
	};
	extern object_manager named_obj;
}