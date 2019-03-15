#pragma once

#include <stdlib.h>
#include <memory>

#include "device.h"

class Allocator {
 public:
  Allocator() {}
  virtual ~Allocator() {};

 // disable copy assign
 public:
  Allocator(const Allocator&) = delete;
  Allocator& operator=(const Allocator&) = delete;

  virtual void* Alloc(size_t size) = 0;
  virtual void Free(void* ptr) = 0;

 protected:
  Device device_;
};
