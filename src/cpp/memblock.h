#pragma once

#include <stdlib.h>

// Memory block allocated for API side use.
struct MemBlock {
  MemBlock();
  void* ptr();

  void* ptr_;
  Device device_;
  size_t size_;
};