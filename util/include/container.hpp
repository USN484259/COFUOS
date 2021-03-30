#pragma once

#ifndef THROW
#ifdef COFUOS
#ifdef UOS_KRNL
#include "lang.hpp"
#define THROW bugcheck
#else
#define THROW(...) abort()
#endif
#else
#define THROW throw
#endif
#endif
