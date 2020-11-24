#pragma once
#include "types.hpp"
#include "hal.hpp"

namespace UOS{
	class IF_guard{
	public:
		const qword state;
		IF_guard(void);
		~IF_guard(void);
	};
}
