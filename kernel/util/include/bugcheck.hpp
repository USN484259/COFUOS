#pragma once
#include "types.hpp"


namespace UOS{
	enum stopcode : qword{
		unknown = 0,
		unhandled_exception,
		assert_failed,
		not_implemented,
		bad_alloc,
		out_of_range,
		invalid_argument,
		null_deref,
		corrupted,
		hardware_fault
	};
}
extern "C" {
	[[ noreturn ]]
	void BugCheck(UOS::stopcode,...);
}