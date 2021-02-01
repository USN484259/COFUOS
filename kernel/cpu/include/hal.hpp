#pragma once
#include "types.hpp"
#include "bugcheck.hpp"

extern "C"{

	void buildIDT(void);
	void fpu_init(void);
	
}

