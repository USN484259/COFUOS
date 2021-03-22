#include "object.hpp"
#include "lock_guard.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"

using namespace UOS;

bool object_manager::put(literal&& name,waitable* obj,dword properties){
	if (name.size() >= max_name_length)
		return false;
	{
		this_core core;
		auto this_process = core.this_thread()->get_process();
		if (this_process->get_privilege() < (byte)properties)
			return false;
	}
	interrupt_guard<rwlock> guard(objlock);
	auto it = table.find(name);
	if (it == table.end())
		return false;
	table.insert(move(name),obj,properties);
	obj->manage();
	return true;
}

const object_manager::object* object_manager::get(const span<char>& name){
	assert(is_locked() && !is_exclusive());
	auto it = table.find(name);
	if (it == table.end()){
		return nullptr;
	}
	return &*it;
}

void object_manager::erase(waitable* obj){
	assert(obj);
	{
		interrupt_guard<rwlock> guard(objlock);
		auto it = table.begin();
		for (;it != table.end();++it){
			if (obj == it->obj){
				it = table.erase(it);
			}
		}
	}
}
