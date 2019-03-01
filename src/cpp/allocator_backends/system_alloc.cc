#include "system_alloc.h"
#include <assert.h>

void* AlignedMalloc(size_t size) {
  void* p = nullptr;
  size_t alignment = 32ul;
#ifdef PADDLE_WITH_MKLDNN
  // refer to https://github.com/01org/mkl-dnn/blob/master/include/mkldnn.hpp
  // memory alignment
  alignment = 4096ul;
#endif
#ifdef _WIN32
  p = _aligned_malloc(size, alignment);
#else
  assert(posix_memalign(&p, alignment, size) == 0);
#endif
  assert(p != nullptr);
  return p;
}

void* CPUAllocator::Alloc(size_t* index, size_t size) {
  // According to http://www.cplusplus.com/reference/cstdlib/malloc/,
  // malloc might not return nullptr if size is zero, but the returned
  // pointer shall not be dereferenced -- so we make it nullptr.
  if (size <= 0) return nullptr;

  *index = 0;  // unlock memory

  void* p = AlignedMalloc(size);
  return p;
}

void CPUAllocator::Free(void* p, size_t size, size_t index) {
  free(p);
}

bool CPUAllocator::UseGpu() const { return false; }
