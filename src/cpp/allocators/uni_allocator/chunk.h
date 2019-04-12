#pragma once

#include <stdlib.h>
#include "src/cpp/allocators/uni_allocator/size_bits.h"

// TODO: change to use c++ style unique_ptr.
class Chunk;
typedef Chunk* ChunkPtr;       /* Chunk pointer */
typedef Chunk* SBinPtr;        /* Small bin */

class Chunk {
 public:
  Chunk();

  inline void* chunk2mem() {
    return static_cast<void*>(
      reinterpret_cast<char*>(this) + TWO_SIZE_T_SIZES);
  }
  inline size_t cinuse () {
    return this->head & CINUSE_BIT;
  }
  inline size_t pinuse() {
    return this->head & PINUSE_BIT;
  }
  inline size_t flag4inuse() {
    return this->head & FLAG4_BIT;
  }
  inline size_t is_inuse() {
    return (this->head & INUSE_BITS) != PINUSE_BIT;
  }
  inline Chunk* next_chunk() {
    return this + (this->head & ~FLAG_BITS);
  }
  inline Chunk* prev_chunk() {
    return this - (this->prev_foot);
  }
  inline size_t next_pinuse() {
    return this->next_chunk()->pinuse();
  }
  inline size_t chunksize() {
    return (head & ~(FLAG_BITS));
  }
  /* Get/set size at footer */
  inline size_t get_foot(size_t s) {
    return ((Chunk*)((char*)(this) + (s)))->prev_foot;
  }
  inline void set_foot(size_t s) {
    ((Chunk*)((char*)(this) + (s)))->prev_foot = s;
  }
  /* Set size, pinuse bit, and foot */
  inline void set_size_and_pinuse_of_free_chunk(size_t s) {
    this->head = s | PINUSE_BIT;
    set_foot(s);
  }

  /* Set size, cinuse and pinuse bit of this chunk */
  inline void set_size_and_pinuse_of_inuse_chunk(size_t s) {
    this->head = (s|PINUSE_BIT|CINUSE_BIT);
  }

  inline void clear_pinuse() {
    head &= ~PINUSE_BIT;
  }

  inline void set_free_with_pinuse(size_t s, ChunkPtr next) {
    next->clear_pinuse();
    set_size_and_pinuse_of_free_chunk(s);
  }

 public:
  /* This four fields must be the same as TreeChunk. */
  size_t prev_foot;        /* Size of previous chunk (if free).  */
  size_t head;             /* Size and inuse bits. */
  Chunk* fd = nullptr;     /* Double links, used only if free. */
  Chunk* bk = nullptr;
};

inline Chunk* mem2chunk(void* mem) {
  return reinterpret_cast<Chunk*>(
    static_cast<char*>(mem) - TWO_SIZE_T_SIZES);
}

#define MALLOC_ALIGNMENT ((size_t)(2 * sizeof(void *)))
#define CHUNK_OVERHEAD      (SIZE_T_SIZE)

#define MCHUNK_SIZE         (sizeof(Chunk))

#define CHUNK_ALIGN_MASK    (MALLOC_ALIGNMENT - SIZE_T_ONE)

/* The smallest size we can malloc is an aligned minimal chunk */
#define MIN_CHUNK_SIZE\
  ((MCHUNK_SIZE + CHUNK_ALIGN_MASK) & ~CHUNK_ALIGN_MASK)

/* the number of bytes to offset an address to align it */
#define align_offset(A)\
 ((((size_t)(A) & CHUNK_ALIGN_MASK) == 0)? 0 :\
  ((MALLOC_ALIGNMENT - ((size_t)(A) & CHUNK_ALIGN_MASK)) & CHUNK_ALIGN_MASK))

/* chunk associated with aligned address A */
#define align_as_chunk(A)   (ChunkPtr)((A) + align_offset(A->chunk2mem()))

/*
  TOP_FOOT_SIZE is padding at the end of a segment, including space
  that may be needed to place segment records and fenceposts when new
  noncontiguous segments are added.
*/
// NOTE: below lines are original define of TOP_FOOT_SIZE
// #define TOP_FOOT_SIZE\
//  (align_offset(chunk2mem(0))+pad_request(sizeof(struct malloc_segment))+MIN_CHUNK_SIZE)
#define TOP_FOOT_SIZE SIZE_T_SIZE

/* Bounds on request (not chunk) sizes. */
#define MAX_REQUEST         ((-MIN_CHUNK_SIZE) << 2)
#define MIN_REQUEST         (MIN_CHUNK_SIZE - CHUNK_OVERHEAD - SIZE_T_ONE)

/* pad request bytes into a usable size */
#define pad_request(req) \
   (((req) + CHUNK_OVERHEAD + CHUNK_ALIGN_MASK) & ~CHUNK_ALIGN_MASK)

/* pad request, checking for minimum (but not maximum) */
#define request2size(req) \
  (((req) < MIN_REQUEST)? MIN_CHUNK_SIZE : pad_request(req))

#define chunk_plus_offset(p, s)  ((ChunkPtr)(((char*)(p)) + (s)))
#define chunk_minus_offset(p, s) ((ChunkPtr)(((char*)(p)) - (s)))

/* Check if address of next chunk n is higher than base chunk p */
#define ok_next(p, n)    ((char*)(p) < (char*)(n))
