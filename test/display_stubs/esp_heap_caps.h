#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
extern bool displayTestAllowAllocation;
extern int displayTestAllocationAttempts;
inline void *heap_caps_aligned_alloc(size_t, size_t size, uint32_t) {
  displayTestAllocationAttempts++;
  return displayTestAllowAllocation ? malloc(size) : nullptr;
}
inline void *heap_caps_malloc(size_t size, uint32_t) {
  displayTestAllocationAttempts++;
  return displayTestAllowAllocation ? malloc(size) : nullptr;
}
