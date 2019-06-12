#include "event.h"

using namespace std;


void Event::wait(void) {
	unique_lock<mutex> lck(m);
	if(!state) {
		cv.wait(lck);
	}
	state = false;
}

cv_status Event::wait(chrono::microseconds ms) {
	unique_lock<mutex> lck(m);
	if (state) {
		state = false;
		return cv_status::no_timeout;
	}
	cv_status ret = cv.wait_for(lck, ms);
	if (ret == cv_status::no_timeout)
		state = false;
	return ret;
}

void Event::notify(void) {
	state = true;
	cv.notify_all();
}

void Event::reset(void) {
	state = false;
}