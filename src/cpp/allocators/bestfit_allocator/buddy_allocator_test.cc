#include <gtest/gtest.h>
#include <chrono>  // NOLINT
#include <thread>  // NOLINT
#include <vector>
#include <cstring>
#include <stdio.h>

#include "buddy_allocator.h"
#include "system_allocator.h"

using namespace paddle::memory::detail;

TEST(buddy_allocator, AllocationTest) {
  auto a = new BuddyAllocator(
    std::unique_ptr<SystemAllocator>(new CPUAllocator),
    1 << 12, 1 << 20);
  
  for (int i = 1; i <= 10000; ++i) {
    void* ptr = a->Alloc(i * sizeof(float));
    a->Free(ptr);
  }
  delete a;
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
