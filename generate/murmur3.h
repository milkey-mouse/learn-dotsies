#pragma once
#include <stddef.h>
#include <stdint.h>

uint32_t murmurhash3_32(const char *data, size_t len, uint32_t seed);
