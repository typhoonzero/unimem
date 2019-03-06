# unimem

Unified memory allocator achieving best performance on heterogous devices,
e.g. applications that make use of general `malloc` and GPU `cudaMalloc`.

An allocator is not good enough if it is not "general". Modern allocators
have performing well already on most of the cases like
"jemalloc", so for CPU memory we pass allocating requests directly to it.

For devices have their own memory to manage, we apply a general method
using one allocating backend  "unimalloc" which is derived from
[dlmalloc](http://gee.cs.oswego.edu/dl/html/malloc.html). This is a well
known allocator that proved to be efficient and generating less fragments.

For more detailed use cases, we provide a set of "decorators" to form a
high-level wrappers to reduce the average allocating and free time, like:
`pool_allocator`, `stack_allocator`, etc.
