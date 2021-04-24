#pragma once
#include "libuos.h"
#include "buddy_heap.hpp"

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

extern UOS::buddy_heap<4,12,mutex> heap;