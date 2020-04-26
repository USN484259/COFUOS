//#define _TEST

extern void branch_test(unsigned, const char*);

#define _IMP_TOSTRING(x) #x
#define TOSTRING(x) _IMP_TOSTRING(x)
#define BRANCH_TEST branch_test(__COUNTER__,__FILE__ " : " TOSTRING(__LINE__))


#include <iostream>
#include "..\..\STL\vector.hpp"
#include "..\..\STL\list.hpp"
#include "..\..\STL\hash_bucket.hpp"
#include "..\..\STL\hash_set.hpp"
#include "..\..\STL\hash_map.hpp"
#include "..\..\STL\avl_tree.hpp"

#include <set>
#include <random>

#include <cmath>

using namespace std;

extern const unsigned branch_count;
struct Record {
	unsigned id;
	const char* info;
	bool operator<(const Record& cmp) const {
		return id < cmp.id;
	}
};

set<Record> record;

void branch_test(unsigned id, const char* info) {

	record.insert({ id,info });

}

void branch_report(void) {
	cout << endl;
	cout << "BRANCH TEST COVERAGE " << record.size() << '/' << branch_count << endl;

	for (auto it = record.cbegin(); it != record.cend(); ++it) {
		cout << it->id << '\t' << it->info << endl;
	}

	record.clear();
}

extern "C"
void BugCheck(UOS::stopcode stat, ...) {
	__debugbreak();
	abort();
}

class object {
	unsigned val;
public:
	static bool log;
public:
	object(unsigned v = 0) : val(v) {
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
	struct hash : public UOS::hash<unsigned> {
		qword operator()(const object& obj) const {
			return UOS::hash<unsigned>::operator()(obj.val);
		}

	};

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

void test_hash_bucket(void) {

	//object::log = true;
	object::hash h;
	UOS::equal_to<object> e;
	UOS::hash_bucket<object, object::hash> container(64, UOS::move(h), UOS::move(e));
	unsigned counter = 0;

	while (counter != 128) {
		container.insert(container.cend(), object(counter++));
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

	//object::log = true;
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

template<typename C>
void print_map(const C& container) {
	cout << "size " << container.size() << endl;
	cout << container.bucket_count() << '\t' << container.load_factor() << endl;
	for (auto it = container.cbegin(); it != container.cend(); ++it) {
		cout << it->first << '\t' << it->second << endl;
	}
}

void test_hash_map(void) {
	//object::log = true;
	UOS::hash_map<object, object> container;
	unsigned counter = 0;

	while (counter < 1024) {
		container[counter] = counter + 1;
		++counter;
	}
	print_map(container);

	cout << container.at(68) << endl;
	container[3] = 20;
	auto it = container.find(3);
	cout << it->first << '\t' << it->second << endl;
	++it;
	it = container.erase(it);

	{
		auto copy(UOS::move(container));
		container.insert(decltype(container)::value_type(24, 32));
		copy.rehash(256);
		print_map(container);
		print_map(copy);

		container = copy;
	}
	print_map(container);
}

void test_avl_tree(void) {
	using namespace UOS;
	UOS::avl_tree<object> container;
	auto stk = container.get_stack();
	std::random_device rng;

	if (true) {

		std::multiset<object> std_container;


		//static const unsigned data[] = { 14,10,7,4,14,11 };


		for (auto i = 0; i < 0x1000; ++i) {
			object val(rng() % 0x1000);
			//object val(data[i]);

			//cout << val << endl;

			stk.clear();
			container.find(val, stk);

			container.insert(val,stk);

			container.check([](const object&) {});

			std_container.insert(val);
		}
		auto std_it = std_container.cbegin();

		container.check([&](const object& val) {
			assert(val == *std_it);
			++std_it;
			//cout << val << endl;
		});

		assert(std_it == std_container.cend());

		std_it = std_container.cbegin();
		for (auto it = container.cbegin(); it != container.cend(); ++it) {
			assert(*it == *std_it);
			++std_it;
		}
		assert(std_it == std_container.cend());
	}


	if (true) {
		for (auto count = 1; count < 0x40; ++count) {
			cout << "count " << count << endl;
			for (auto sel = 0; sel < count; ++sel) {
				container.clear();

				decltype(container)::iterator pos;

				for (auto i = 0; i < count; ++i) {
					object val(i);
					stk.clear();
					container.find(val, stk);

					auto it = container.insert(val,stk);
					if (i == sel)
						pos = it;
				}
				container.check([&](const object& val) {});

				cout << *pos << '\t';

				pos = container.erase(pos);
				if (pos != container.end())
					cout << *pos;
				else
					cout << "(end)";
				cout << endl;

				container.check([&](const object& val) {
					cout << val << '\t';
				});
				cout << endl;
			}
		}
	}

	if (true) {

		for (auto round = 0; round < 0x100; ++round) {
			cout << "round " << round << endl;
			container.clear();
			for (auto i = 0; i < 0x100; ++i) {
				object val(i);
				stk.clear();
				container.find(val, stk);
				container.insert(val, stk);
			}
			//static const unsigned table[] = { 7,1,2,6,5,2,8,2,0 };
			while (container.size()) {
				//for (auto i = 0;i < 9;++i){
				auto it = container.cbegin();
				auto id = rng() % container.size();
				//auto id = table[i];

				cout << id << '\t';

				while (id--)
					++it;

				cout << *it << endl;
				container.erase(it);

				container.check([](const object& obj) {
					//cout << obj << '\t';
				});
				//cout << endl;
			}
		}
	}

	if (true) {
		std::multiset<object> std_container;

		for (auto count = 1; count <= 0x100; ++count) {
			cout << "count " << count << endl;
			container.clear();
			std_container.clear();

			for (auto i = 0; i < count; ++i) {
				object val(rng() % 0x100);

				stk.clear();
				container.find(val, stk);
				container.insert(val, stk);
				std_container.insert(val);

				container.check([](const object&) {});

			}

			while (container.size()) {
				auto it = container.cbegin();
				auto std_it = std_container.cbegin();

				auto pos = rng() % container.size();
				decltype(container)::const_iterator target;
				decltype(std_container)::const_iterator std_target;

				for (auto i = 0; it != container.cend(); ++i) {
					if (i == pos) {
						target = it;
						std_target = std_it;
					}
					assert(*it == *std_it);
					++it, ++std_it;
				}

				assert(std_it == std_container.cend());

				it = container.erase(target);
				std_it = std_container.erase(std_target);

				container.check([](const object&) {});

			}


		}



	}


	branch_report();
	//ERASE missing branches 6 7 8 15
}


int main(void) {
	//test_vector();

	//test_list();

	//test_hash_bucket();

	//test_hash_set();

	//test_hash_map();

	test_avl_tree();

	return 0;
}


const unsigned branch_count = __COUNTER__;