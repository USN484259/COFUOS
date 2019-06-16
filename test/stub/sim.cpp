#include "sim.h"
#include <ctime>
#include <cmath>
#include "..\..\sysinfo.hpp"
#include "..\..\context.hpp"
#include "..\..\mm.hpp"
#include "..\..\mp.hpp"
#include "..\..\apic.hpp"
using namespace UOS;


SYSINFO* UOS::sysinfo = nullptr;
MP* UOS::mp = nullptr;
APIC* UOS::apic = nullptr;

extern bool kdb_enable;


Sim::Sim(void) {
	sysinfo = new SYSINFO();
	mp = new MP();
	srand(time(NULL));
	sysinfo->MP_cnt = 1;
	kdb_enable = true;
}

Sim::operator bool(void) const{
	return kdb_enable;
}

Sim::~Sim(void) {
	kdb_enable = false;
	delete sysinfo;
	delete mp;
}

extern "C"
byte DR_match(void) {
	return 1;
}

static DR_STATE dr = { 0 };


extern "C"
void DR_get(void* dst) {
	memcpy(dst, &dr, sizeof(DR_STATE));
}

extern "C"
void DR_set(const void* sor) {
	memcpy(&dr, sor, sizeof(DR_STATE));
}


qword Sim::dr_match(qword cur,qword prev) {
	qword mask = 0x03;
	for (int i = 0; i < 4; i++, mask <<= 2) {
		if (dr.dr7 & mask && prev < dr.dr[i] && dr.dr[i] <= cur)
			return dr.dr[i];
	}
	return cur;
}

void gen(byte* dst, size_t len) {
	while (len--) {
		*dst++ = rand();
	}
}




bool VM::spy(void* dst, qword base, size_t len) {
	if (base < 0xFFFF800000000000)
		return false;
	gen((byte*)dst, len);
	return true;
}

bool PM::spy(void* dst, qword base, size_t len) {
	if (base > 0x4000000)
		return false;
	gen((byte*)dst, len);
	return true;
}

interrupt_guard::interrupt_guard(void) : state(0){}
interrupt_guard::~interrupt_guard(void) {}


void APIC::mp_break(void) {}
byte APIC::id(void) {
	return 0;
}

__declspec(noreturn)
int BugCheck(UOS::status, qword, qword) {
	abort();
}
