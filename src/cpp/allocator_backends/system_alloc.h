#include <stdlib.h>


class SystemAllocator {
 public:
  virtual ~SystemAllocator() {}
  virtual void* Alloc(size_t* index, size_t size) = 0;
  virtual void Free(void* p, size_t size, size_t index) = 0;
  virtual bool UseGpu() const = 0;
};

class CPUAllocator : public SystemAllocator {
 public:
  virtual void* Alloc(size_t* index, size_t size);
  virtual void Free(void* p, size_t size, size_t index);
  virtual bool UseGpu() const;
};