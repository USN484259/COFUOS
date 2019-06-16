#pragma once
#include "..\..\types.hpp"
class Sim {
public:
	Sim(void);
	operator bool(void) const;
	~Sim(void);
	qword dr_match(qword,qword);
};