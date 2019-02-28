#pragma once

#include <stdlib.h>
#include <memory>

#include "device.h"
#include "memblock.h"

// Allocator Interface
// Any allocator can 
class Allocator {
 public:
  virtual ~Allocator() {};

 // disable copy assign
 public:
  Allocator(const Allocator&) = delete;
  Allocator& operator=(const Allocator&) = delete;

  virtual std::unique_ptr<MemBlock> Alloc(Device device, size_t size) = 0;
  virtual void Free(Device device, std::unique_ptr<MemBlock>) = 0;
};
