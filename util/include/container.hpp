#pragma once

#ifndef THROW
#ifdef COFUOS
#include "bugcheck.hpp"
#define THROW(x) BugCheck(unhandled_exception,x)
#else
#define THROW throw
#endif
#endif