cc_library(buddy_allocator SRCS buddy_allocator.cc memory_block.cc
  system_allocator.cc meta_cache.cc memory_block_desc.cc)

cc_test(buddy_allocator_test SRCS buddy_allocator_test.cc DEPS buddy_allocator)