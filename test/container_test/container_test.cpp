#define _TEST

#include <iostream>
#include "..\..\STL\vector.hpp"
#include "..\..\STL\list.hpp"
#include "..\..\STL\hash_bucket.hpp"
#include "..\..\STL\hash_set.hpp"
using namespace std;



extern "C"
void BugCheck(UOS::stopcode stat,...) {
	__debugbreak();
	abort();
}

class object {
	unsigned val;
public:
	static bool log;
public:
	object(unsigned v) : val(v) {
		if (log)
			cout << v << endl;
	}
	object(const object& obj) : val(obj.val) {
		if (log)
			cout << 'C' << val << endl;
	}
	object(object&& obj) : val(obj.val) {
		//obj.val = 0;
		if (log)
			cout << 'M' << val << endl;
	}
	object& operator=(const object& obj) {
		val = obj.val;
		if (log)
			cout << "AC" << val << endl;
		return *this;
	}
	object& operator=(object&& obj) {
		val = obj.val;
		//obj.val = 0;
		if (log)
			cout << "AM" << val << endl;
		return *this;
	}
	~object(void) {
		if (log)
			cout << '~' << val << endl;
	}
	operator unsigned(void) const {
		return val;
	}
	bool operator==(const object& cmp) const {
		return val == cmp.val;
	}
	bool operator!=(const object& cmp) const {
		return val != cmp.val;
	}

	static qword hash(const object& obj) {
		return UOS::hash(obj.val);
	}

};

bool object::log = false;

template<typename C>
void print(const C& container) {
	cout << "size " << container.size() << endl;
	for (auto it = container.cbegin(); it != container.cend(); ++it) {
		cout << *it << '\t';
	}
	cout << endl;
}


void test_vector(void) {

	unsigned counter = 0;

	UOS::vector<object> container;

	container.push_front(object(counter++));

	print(container);

	while (counter < 100) {
		container.push_back(object(counter++));
	}

	print(container);

	while (container.size() > 97)
		container.pop_back();

	print(container);

	while (counter < 128) {
		container.push_front(object(counter++));
	}

	print(container);

	while (container.size() > 120)
		container.pop_front();

	print(container);

	auto it = container.erase(container.cbegin() + container.size() / 2);
	container.insert(it + container.size() / 4, object(counter++));

	print(container);

	cout << container[container.size() / 4] << '\t' << *(container.cend() - container.size() / 4) << endl;

	{
		auto copy(UOS::move(container));
		print(container);
		copy.reserve(copy.size() * 2);
		print(copy);
		while (copy.shink()) {
			cout << "shinked" << copy.capacity() << endl;
			print(copy);
		}

		container = copy;

	}

	print(container);
}


void test_list(void) {
	unsigned counter = 0;

	//object::log = true;
	UOS::list<object> container;

	container.insert(container.cbegin(), object(counter++));

	print(container);

	while (container.size() < 64)
		container.push_back(object(counter++));
	print(container);
	cout << container.front() << '\t' << container.back() << endl;
	container.pop_back();
	container.pop_front();
	
	print(container);

	while (container.size() < 128) {
		object o(counter++);
		container.push_front(o);
	}
	print(container);

	auto it = container.cbegin();
	while (*it > container.back())
		++it;

	it = container.insert(it, object(counter++));

	cout << *it << endl;
	print(container);

	it = UOS::find(container.begin(), container.end(), object(100));

	it = container.erase(it);
	cout << *it << endl;
	print(container);

	{
		auto copy(UOS::move(container));
		auto it = copy.cbegin();
		++it;
		container.splice(container.cbegin(), copy, it);

		print(container);

		print(copy);

		container = copy;
	}

	print(container);

}

void test_hash_bucket(void){

	//object::log = true;
	UOS::hash_bucket<object, object::hash> container(64);
	unsigned counter = 0;

	while (counter != 128) {
		container.insert(container.cend(),object(counter++));
	}
	
	cout << container.bucket_count() << '\t' << container.size() << '\t' << container.max_load() << endl;

	for (auto it = container.cbegin(); it != container.cend(); ++it) {
		cout << hex << it->first << '\t' << dec << it->second << endl;
	}

	container.rehash(16);

	cout << container.bucket_count() << '\t' << container.size() << '\t' << container.max_load() << endl;

	for (auto it = container.cbegin(); it != container.cend(); ++it) {
		cout << hex << it->first << '\t' << dec << it->second << endl;
	}

	//print(container);

}

void test_hash_set(void) {

	object::log = true;
	UOS::hash_set<object, object::hash> container;

	unsigned counter = 0;
	while (counter < 1024) {
		container.insert(object(counter++));
		cout << container.load_factor() << endl;
	}
	cout << container.bucket_count() << '\t' << container.load_factor() << endl;
	print(container);

	cout << container.insert(34).second << endl;

	auto it = container.find(28);

	cout << *it << endl;

	++it;
	it = container.erase(it);

	{
		auto copy(UOS::move(container));

		container.insert(98);

		print(container);
		print(copy);


		container = copy;
	}
	print(container);

}


int main(void) {
	test_vector();

	test_list();

	test_hash_bucket();

	test_hash_set();

	return 0;
}