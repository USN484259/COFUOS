#pragma once
#include "libuos.h"

struct mutex{
	HANDLE semaphore;
public:
	mutex(void);
	//kernel will clean up everything
	//~mutex(void);
	void lock(void);
	bool try_lock(void);
	void unlock(void);
};

#include "buddy_heap.hpp"