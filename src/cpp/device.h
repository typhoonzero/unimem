#pragma once

#include <stdlib.h>

enum DeviceType { CPU=0, CPU_PAGE_LOCKED, GPU, CUDA_PINNED };

static size_t num_device_types = CUDA_PINNED + 1;

struct Device {
  DeviceType type_;
  // CPU thread local ID, 0 for global CPU memory
  // Or can be: GPU device ID
  // NOTE: memory for GPU kernel space thread local or "__shared__"
  //       is not managed here for now. If needed, should use
  //       boost::variant to serve completely different types.
  size_t idx_;
};
