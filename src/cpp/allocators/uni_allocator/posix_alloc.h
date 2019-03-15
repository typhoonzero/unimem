#pragma once

#include <string>
#include <cassert>
#include "src/cpp/allocator.h"

class PosixAlloc : public Allocator {
 public:
  PosixAlloc() {}
  virtual ~PosixAlloc() {};

  void* Alloc(size_t size) {
    return malloc(size);
  }
  void Free(void* ptr) {
    return free(ptr);
  }
};
