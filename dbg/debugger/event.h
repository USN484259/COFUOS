#pragma once
#include <mutex>
#include <atomic>
#include <chrono>
#include <condition_variable>

class Event {
	std::atomic<bool> state;
	std::mutex m;
	std::condition_variable cv;

public:
	void wait(void);
	std::cv_status wait(std::chrono::microseconds);
	void notify(void);
	void reset(void);
};
