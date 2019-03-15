cc_library(uni_allocator SRCS uni_allocator.cc)

cc_test(uni_alloc_test SRCS uni_alloc_test.cc DEPS uni_allocator)