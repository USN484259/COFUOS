#pragma once
#include "sync/include/spin_lock.hpp"
#include "buddy_heap.hpp"

namespace UOS{
	extern buddy_heap<4,12,spin_lock> heap;
}