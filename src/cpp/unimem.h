#include <string>
#include <stdlib.h>

#include "device.h"
#include "memblock.h"

namespace unimem {

std::unique_ptr<MemBlock> alloc(Device device, size_t size);

void free(Device device, std::unique_ptr<MemBlock> block);


} // namespace unimem