#pragma once

#include <string>
#include <cassert>
#include "src/cpp/allocator.h"
#include "src/cpp/allocator_backends/system_alloc.h"
#include "chunk.h"
#include "exception.h"
#include "tree_chunk.h"
#include "debug.h"

typedef unsigned int binmap_t;
typedef unsigned int flag_t;

/* Bin types, widths and sizes */
#define NSMALLBINS        (32U)
#define NTREEBINS         (32U)
#define SMALLBIN_SHIFT    (3U)
#define SMALLBIN_WIDTH    (SIZE_T_ONE << SMALLBIN_SHIFT)
#define TREEBIN_SHIFT     (8U)
#define MIN_LARGE_SIZE    (SIZE_T_ONE << TREEBIN_SHIFT)
#define MAX_SMALL_SIZE    (MIN_LARGE_SIZE - SIZE_T_ONE)
#define MAX_SMALL_REQUEST (MAX_SMALL_SIZE - CHUNK_ALIGN_MASK - CHUNK_OVERHEAD)

// bytes < 256 is treated as "small bins"
#define is_small(s)         (((s) >> SMALLBIN_SHIFT) < NSMALLBINS)
#define small_index(s)      (bindex_t)((s)  >> SMALLBIN_SHIFT)
#define small_index2size(i) ((i)  << SMALLBIN_SHIFT)
#define MIN_SMALL_INDEX     (small_index(MIN_CHUNK_SIZE))

#define smallbin_at(M, i)   ((SBinPtr)((char*)&((M)->smallbins[(i)<<1])))

// checks
#define RTCHECK(e)  __builtin_expect(e, 1)

class UniAllocator : public Allocator {
 public:
  explicit UniAllocator(Allocator* underlying_allocator) : 
    underlying_allocator_(underlying_allocator) {
      gm_ = new MallocState;
      // TODO: do init MallocState here
  };
  virtual ~UniAllocator();

  void Init();
  void* Alloc(size_t bytes);
  void Free(void* ptr);

 private:
  Allocator* underlying_allocator_;
  // NOTE that typical allocators store metadata together with actual
  // memory chunks, but for on device memory, it may be a waist to store
  // metadata in device, because we may always need to find the device
  // memory pointer from CPU side, should find a better way to do this.

  // Currently, we must store metadata on device memory for correctly
  // indexing the real device memory pointer.
  struct MallocState {
    binmap_t   smallmap;
    binmap_t   treemap;
    size_t     dvsize;
    size_t     topsize;
    char*      least_addr;
    Chunk*     dv;
    Chunk*     top;
    size_t     trim_check;
    size_t     release_checks;
    size_t     magic;
    Chunk*     smallbins[(NSMALLBINS+1)*2];
    TreeChunk*   treebins[NTREEBINS];
    size_t     footprint;
    size_t     max_footprint;
    size_t     footprint_limit; /* zero means no limit */
    flag_t     mflags;
  #if USE_LOCKS
    MLOCK_T    mutex;     /* locate lock among fields that rarely change */
  #endif /* USE_LOCKS */
    // FIXME(typhoonzero): segment is used for mmap, not the current case.
    // msegment   seg;
    void*      extp;      /* Unused but available for extensions */
    size_t     exts;

    /* -------------------------- Actions -------------------------- */
    inline binmap_t idx2bit(size_t i) {
      return ((binmap_t)(1) << (i));
    }
    inline void mark_smallmap(size_t i) {
      smallmap |=  idx2bit(i);
    }
    inline void clear_smallmap(size_t i) {
      (smallmap &= ~idx2bit(i));
    }
    inline size_t smallmap_is_marked(size_t i) {
      return smallmap & idx2bit(i);
    }

    /* isolate the least set bit of a bitmap */
    inline binmap_t least_bit(binmap_t x) {
      return ((x) & -(x));
    }

    /* mask with all bits to left of least bit of x on */
    inline binmap_t left_bits(binmap_t x) {
      return ((x<<1) | -(x<<1));
    }

    inline void compute_bit2idx(binmap_t X, binmap_t* I) {
      unsigned int J;
      J = __builtin_ctz(X);
      *I = (bindex_t)J;
    }

    inline bool ok_address(ChunkPtr a) {
      return (static_cast<char*>(a) >= least_addr);
    }
    /* Unlink the first chunk from a smallbin */
    inline void unlink_first_small_chunk(ChunkPtr B, ChunkPtr P, size_t I) {
      ChunkPtr F = P->fd;
      DEBUG_ASSERT(P != B);
      DEBUG_ASSERT(P != F);
      DEBUG_ASSERT(chunksize(P) == small_index2size(I));
      if (B == F) {
        clear_smallmap(I);
      } else if (RTCHECK(ok_address(F) && F->bk == P)) {
        F->bk = B;
        B->fd = F;
      } else {
        throw MemoryCorruptedException();
      }
    }

    /* Set cinuse and pinuse of this chunk and pinuse of next chunk */
    // TODO: can put this in chunks's member since it do not depend on gm_
    inline void set_inuse_and_pinuse(ChunkPtr p, size_t s) {
      p->head = (s | PINUSE_BIT | CINUSE_BIT);
      (static_cast<ChunkPtr>((static_cast<char*>(p)) + (s)))->head |= PINUSE_BIT;
    }

    /* Set size, cinuse and pinuse bit of this chunk */
    inline void set_size_and_pinuse_of_inuse_chunk(ChunkPtr p, size_t s) {
      ((p)->head = (s|PINUSE_BIT|CINUSE_BIT));
    }

    /* Check properties of any chunk, whether free, inuse, mmapped etc  */
    inline void do_check_any_chunk(ChunkPtr p) {
      DEBUG_ASSERT((is_aligned(p->chunk2mem())) || (p->head == FENCEPOST_HEAD));
      DEBUG_ASSERT(gm_->ok_address(p));
    }

    /* Check properties of inuse chunks */
    inline void do_check_inuse_chunk(ChunkPtr p) {
      gm_->do_check_any_chunk(p);
      DEBUG_ASSERT(is_inuse(p));
      DEBUG_ASSERT(next_pinuse(p));
      /* If not pinuse and not mmapped, previous chunk has OK offset */
      DEBUG_ASSERT(pinuse(p) || next_chunk(prev_chunk(p)) == p);
    }

    /* Check properties of malloced chunks at the point they are malloced */
    inline void do_check_malloced_chunk(void* mem, size_t s) {
      if (mem != 0) {
        ChunkPtr p = mem2chunk(mem);
        size_t sz = p->head & ~INUSE_BITS;
        gm_->do_check_inuse_chunk(p);
        DEBUG_ASSERT((sz & CHUNK_ALIGN_MASK) == 0);
        DEBUG_ASSERT(sz >= MIN_CHUNK_SIZE);
        DEBUG_ASSERT(sz >= s);
        /* unless mmapped, size is less than MIN_CHUNK_SIZE more than request */
        DEBUG_ASSERT(sz < (s + MIN_CHUNK_SIZE));
      }
    }

    /* Link a free chunk into a smallbin  */
    inline void insert_small_chunk(ChunkPtr P, size_t S) {
      bindex_t I  = small_index(S);
      ChunkPtr B = smallbin_at(gm_, I);
      ChunkPtr F = B;
      assert(S >= MIN_CHUNK_SIZE);
      if (!gm_->smallmap_is_marked(I)) {
        gm_->mark_smallmap(I);
      } else if (RTCHECK(gm_->ok_address(B->fd))) {
        F = B->fd;
      } else {
        throw MemoryCorruptedException();
      }
      B->fd = P;
      F->bk = P;
      P->fd = F;
      P->bk = B;
    }

    /* Replace dv node, binning the old one */
    /* Used only when dvsize known to be small */
    inline void replace_dv(ChunkPtr P, size_t S) {
      size_t DVS = dvsize;
      assert(is_small(DVS));
      if (DVS != 0) {
        ChunkPtr DV = dv;
        gm_->insert_small_chunk(DV, DVS);
      }
      dvsize = S;
      dv = P;
    }

    /* allocate a small request from the best fitting chunk in a treebin */
    void* tmalloc_small(mstate m, size_t nb) {
      TreeChunkPtr t, v;
      size_t rsize;
      bindex_t i;
      binmap_t leastbit = least_bit(m->treemap);
      compute_bit2idx(leastbit, i);
      v = t = *treebin_at(m, i);
      rsize = t->chunksize() - nb;

      while ((t = leftmost_child(t)) != 0) {
        size_t trem = chunksize(t) - nb;
        if (trem < rsize) {
          rsize = trem;
          v = t;
        }
      }

      if (RTCHECK(ok_address(m, v))) {
        mchunkptr r = chunk_plus_offset(v, nb);
        assert(chunksize(v) == rsize + nb);
        if (RTCHECK(ok_next(v, r))) {
          unlink_large_chunk(m, v);
          if (rsize < MIN_CHUNK_SIZE)
            set_inuse_and_pinuse(m, v, (rsize + nb));
          else {
            set_size_and_pinuse_of_inuse_chunk(m, v, nb);
            set_size_and_pinuse_of_free_chunk(r, rsize);
            replace_dv(m, r, rsize);
          }
          return chunk2mem(v);
        }
      }

      CORRUPTION_ERROR_ACTION(m);
      return 0;
    }

  };
  typedef MallocState* MallocStatePtr;

  // global malloc state
  static MallocStatePtr gm_;
};



