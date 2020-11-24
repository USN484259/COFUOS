#include "util.hpp"
#include "hal.hpp"

namespace UOS {

	qword rdseed(void) {
		qword result = 0;
		while (!_rdseed64_step(&result)) {
			_mm_pause();
		}
		return result;
	}

	qword rdrand(void) {
		qword result = 0;
		while (!_rdrand64_step(&result)) {
			_mm_pause();
		}
		return result;
	}

}








