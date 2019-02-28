# unimem

Unified memory allocator for various device platforms like applications
make use of general `malloc` and GPU `cudaMalloc`.

The main allocator backend is "unimalloc" which is derived from
[dlmalloc](http://gee.cs.oswego.edu/dl/html/malloc.html) which is proved
to be efficient and generating less fragments.

Recent allocators have performing well already like
"jemalloc", so for CPU memory we pass allocating requests directly to it.

For more detailed use cases, we provide a set of "decorators" to form a
high-level wrappers to reduce the average allocating and free time, like:
`pool_allocator`, `stack_allocator`, etc.

