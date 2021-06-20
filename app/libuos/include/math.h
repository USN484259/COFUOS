#pragma once

inline double sqrt(double val){
	double res;
	__asm__ (
		"sqrtpd %0,%1"
		: "=x" (res)
		: "x" (val)
	);
	return res;
}