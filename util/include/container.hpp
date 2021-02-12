#pragma once

#ifndef THROW
#ifdef COFUOS
#include "lang.hpp"
#define THROW bugcheck
#else
#define THROW throw
#endif
#endif