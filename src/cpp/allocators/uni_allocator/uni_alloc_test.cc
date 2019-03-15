#include <gtest/gtest.h>
#include <chrono>  // NOLINT
#include <thread>  // NOLINT
#include <vector>
#include <cstring>
#include <stdio.h>
#include <iostream>

#include "src/cpp/allocators/uni_allocator/uni_allocator.h"
#include "src/cpp/allocators/uni_allocator/posix_alloc.h"

TEST(uni_alloc, SimpleTest) {
  auto posix_alloc = new PosixAlloc();
  Device dev;
  dev.type_ = DeviceType();
  UniAllocator unialloc(
    posix_alloc, 1 << 20, dev);
  
  std::vector<void*> mems;
  mems.reserve(10);

  for (int i = 1; i <= 1000; ++i) {
    mems.clear();  
    for (int j = 0; j < 10; ++j) {
      std::cout << "alloc: " << i << std::endl;
      void* ptr = unialloc.Alloc(i * sizeof(float));
      mems.push_back(ptr);
    }
    for (int j = 0; j < 10; ++j) {
      void* ptr = mems[j];
      std::cout << "free: " << ptr << std::endl;
      unialloc.Free(ptr);
    }
  }
  delete posix_alloc;
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
