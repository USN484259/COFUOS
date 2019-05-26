#pragma once
#include "types.hpp"

namespace UOS{

	class IF_guard{
		const qword state;
	public:
		IF_guard(void);
		~IF_guard(void);
		
		bool status(void) const;
		
	};




}