#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

static_assert(sizeof(byte) == 1, "size_byte_assert_failed");
static_assert(sizeof(word) == 2, "size_word_assert_failed");
static_assert(sizeof(dword) == 4, "size_dword_assert_failed");
static_assert(sizeof(qword) == 8, "size_qword_assert_failed");
static_assert(sizeof(size_t) == 8, "size_sizet_assert_failed");
static_assert(sizeof(void*) == 8, "size_pointer_assert_failed");

#ifdef __cplusplus

namespace UOS {}

#endif