#pragma once
#include "types.hpp"


namespace UOS{
	enum stopcode : qword{
		failed,
		assert_failed,
		not_implemented,
		bad_alloc,
		out_of_range,
		invalid_argument,
		null_deref,
		corrupted
		//inconsistent,
	};
	
}

