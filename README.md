# unimem

Unified memory allocator achieving best performance on heterogous devices,
e.g. applications that make use of general `malloc` and GPU `cudaMalloc`.

For devices have their own memory to manage, we apply a general method
using one allocating backend  "unimalloc" which is derived from
[dlmalloc](http://gee.cs.oswego.edu/dl/html/malloc.html). This is a well
known allocator that proved to be efficient and generating less fragments.

Note that the "metadata" which stores "bins" and "chunks" are actually stored
in CPU memory but the memory allocation no GPU device memory still have
memory overhead due to make implementaion simpler.

For more detailed use cases, we provide a set of "decorators" to form a
high-level wrappers to reduce the average allocating and free time, like:
`pool_allocator`, `stack_allocator`, etc.

