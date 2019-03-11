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
#define SMALLBIN_WIDTH    (SIZE_T_ONE << SMALLBIN_SHIFT)  /* 8 */
#define TREEBIN_SHIFT     (8U)
#define MIN_LARGE_SIZE    (SIZE_T_ONE << TREEBIN_SHIFT)   /* 256 */
#define MAX_SMALL_SIZE    (MIN_LARGE_SIZE - SIZE_T_ONE)   /* 255 */
#define MAX_SMALL_REQUEST (MAX_SMALL_SIZE - CHUNK_ALIGN_MASK - CHUNK_OVERHEAD) /* 232 */

#define MAX_SIZE_T           (~(size_t)0)

// bytes < 256 is treated as "small bins"
#define is_small(s)         (((s) >> SMALLBIN_SHIFT) < NSMALLBINS)
#define small_index(s)      (bindex_t)((s)  >> SMALLBIN_SHIFT)
#define small_index2size(i) ((i)  << SMALLBIN_SHIFT)
#define MIN_SMALL_INDEX     (small_index(MIN_CHUNK_SIZE))

#define smallbin_at(M, i)   ((SBinPtr)((char*)&((M)->smallbins[(i)<<1])))

// checks
#define RTCHECK(e)  __builtin_expect(e, 1)
#define MAX_RELEASE_CHECK_RATE MAX_SIZE_T
#define DEFAULT_TRIM_THRESHOLD ((size_t)2U * (size_t)1024U * (size_t)1024U)

class UniAllocator : public Allocator {
 public:
  explicit UniAllocator(Allocator* underlying_allocator, 
                        size_t increasing_size,
                        Device* device) : 
    underlying_allocator_(underlying_allocator),
    increasing_size_(increasing_size),
    increasing_step_(1),
    // copy device info
    device_(*device) {
      gm_ = new MallocState;
      // TODO: do init MallocState here
  };
  virtual ~UniAllocator();

  void InitOrExpand();
  void* Alloc(size_t bytes);
  void Free(void* ptr);

 private:
  void AllocInternalIncrease();

 private:
  Allocator* underlying_allocator_;
  size_t increasing_size_;
  size_t increasing_step_;
  Device device_;
  // NOTE: that typical allocators store metadata together with actual
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

    /* -------------------------- Inits -------------------------- */
    bool is_initialized() {
      return top != 0;
    }

    /* Initialize top chunk and its size */
    void init_top(ChunkPtr p, size_t psize) {
      /* Ensure alignment */
      size_t offset = align_offset(p->chunk2mem());
      p = static_cast<ChunkPtr>((char*)p + offset);
      psize -= offset;

      top = p;
      topsize = psize;
      p->head = psize | PINUSE_BIT;
      /* set size of fake trailing chunk holding overhead space only once */
      chunk_plus_offset(p, psize)->head = TOP_FOOT_SIZE;
      // trim_check = mparams.trim_threshold; /* reset on each update */
      trim_check = DEFAULT_TRIM_THRESHOLD;
    }

    /* Initialize bins for a new mstate that is otherwise zeroed out */
    void init_bins() {
      /* Establish circular links for smallbins */
      bindex_t i;
      for (i = 0; i < NSMALLBINS; ++i) {
        SBinPtr bin = smallbin_at(gm_, i);
        bin->fd = bin->bk = bin;
      }
    }

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

    inline void mark_treemap(size_t i) {
      treemap  |=  idx2bit(i);
    }
    inline void clear_treemap(size_t i) {
      treemap  &= ~idx2bit(i);
    }
    inline size_t treemap_is_marked(size_t i) {
      return (treemap & idx2bit(i));
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
    inline bool ok_address(TreeChunkPtr a) {
      return (static_cast<char*>(a) >= least_addr);
    }
    /* Unlink the first chunk from a smallbin */
    inline void unlink_first_small_chunk(ChunkPtr B, ChunkPtr P, size_t I) {
      ChunkPtr F = P->fd;
      DEBUG_ASSERT(P != B);
      DEBUG_ASSERT(P != F);
      DEBUG_ASSERT(P->chunksize() == small_index2size(I));
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

    /* Insert chunk into tree */
    inline void insert_large_chunk(TreeChunkPtr X, size_t S) {
      tbinptr* H;
      bindex_t I;
      compute_tree_index(S, &I);
      H = treebin_at(I);
      X->index = I;
      X->child[0] = X->child[1] = 0;
      if (!treemap_is_marked(I)) {
        mark_treemap(I);
        *H = X;
        X->parent = static_cast<TreeChunkPtr>(H);
        X->fd = X->bk = X;
      } else {
        TreeChunkPtr T = *H;
        size_t K = S << leftshift_for_tree_index(I);
        for (;;) {
          if (T->chunksize() != S) {
            TreeChunkPtr* C = &(T->child[(K >> (SIZE_T_BITSIZE-SIZE_T_ONE)) & 1]);
            K <<= 1;
            if (*C != 0) {
              T = *C;
            } else if (RTCHECK(ok_address(C))) {
              *C = X;
              X->parent = T;
              X->fd = X->bk = X;
              break;
            } else {
              throw MemoryCorruptedException();
              break;
            }
          } else {
            TreeChunkPtr F = static_cast<TreeChunkPtr>(T->fd);
            if (RTCHECK(ok_address(T) && ok_address(F))) {
              T->fd = F->bk = X;
              X->fd = F;
              X->bk = T;
              X->parent = 0;
              break;
            } else {
              throw MemoryCorruptedException();
              break;
            }
          }
        }
      }
    }

    inline void insert_chunk(ChunkPtr P, size_t S) {
      if (is_small(S)) {
        insert_small_chunk(P, S);
      } else {
        TreeChunkPtr TP = static_cast<TreeChunkPtr>(P);
        insert_large_chunk(TP, S);
      }
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

    inline TreeChunkPtr* treebin_at(bindex_t i) {
      // TODO: make this c++ style
      return &(treebins[i]);
    }

    inline void unlink_large_chunk(TreeChunkPtr X) {
      TreeChunkPtr XP = X->parent;
      TreeChunkPtr R;
      if (X->bk != X) {
        TreeChunkPtr F = static_cast<TreeChunkPtr>(X->fd);
        R = static_cast<TreeChunkPtr>(X->bk);
        if (RTCHECK(ok_address(F) && F->bk == X && R->fd == X)) {
          F->bk = R;
          R->fd = F;
        } else {
          throw MemoryCorruptedException();
        }
      } else {
        TreeChunkPtr* RP;
        if (((R = *(RP = &(X->child[1]))) != 0) ||
            ((R = *(RP = &(X->child[0]))) != 0)) {
          TreeChunkPtr* CP;
          while ((*(CP = &(R->child[1])) != 0) ||
                (*(CP = &(R->child[0])) != 0)) {
            R = *(RP = CP);
          }
          if (RTCHECK(ok_address(RP))) {
            *RP = 0;
          } else {
            throw MemoryCorruptedException();
          }
        }
      }
      if (XP != 0) {
        tbinptr* H = static_cast<tbinptr*>(treebin_at(X->index));
        if (X == *H) {
          if ((*H = R) == 0) {
            clear_treemap(X->index);
          }
        } else if (RTCHECK(ok_address(XP))) {
          if (XP->child[0] == X) {
            XP->child[0] = R;
          } else {
            XP->child[1] = R;
          }
        } else {
          throw MemoryCorruptedException();
        }
        if (R != 0) {
          if (RTCHECK(ok_address(R))) {
            TreeChunkPtr C0, C1;
            R->parent = XP;
            if ((C0 = X->child[0]) != 0) {
              if (RTCHECK(ok_address(C0))) {
                R->child[0] = C0;
                C0->parent = R;
              } else {
                throw MemoryCorruptedException();
              }
            }
            if ((C1 = X->child[1]) != 0) {
              if (RTCHECK(ok_address(C1))) {
                R->child[1] = C1;
                C1->parent = R;
              } else {
                throw MemoryCorruptedException();
              }
            }
          } else {
            throw MemoryCorruptedException();
          }
        }
      }
    }

    /* allocate a small request from the best fitting chunk in a treebin */
    void* tmalloc_small(size_t nb) {
      TreeChunkPtr t, v;
      size_t rsize;
      bindex_t i;
      binmap_t leastbit = least_bit(treemap);
      compute_bit2idx(leastbit, &i);
      v = t = *treebin_at(i);
      rsize = t->chunksize() - nb;

      while ((t = t->leftmost_child()) != 0) {
        size_t trem = t->chunksize() - nb;
        if (trem < rsize) {
          rsize = trem;
          v = t;
        }
      }

      if (RTCHECK(ok_address(v))) {
        ChunkPtr r = chunk_plus_offset(v, nb);
        DEBUG_ASSERT(chunksize(v) == rsize + nb);
        if (RTCHECK(ok_next(v, r))) {
          unlink_large_chunk(v);
          if (rsize < MIN_CHUNK_SIZE)
            set_inuse_and_pinuse(v, (rsize + nb));
          else {
            set_size_and_pinuse_of_inuse_chunk(v, nb);
            r->set_size_and_pinuse_of_free_chunk(rsize);
            replace_dv(r, rsize);
          }
          return v->chunk2mem();
        }
      }
      throw MemoryCorruptedException();
      return 0;
    }


  inline void compute_tree_index(size_t S, bindex_t* I) {
    unsigned int X = S >> TREEBIN_SHIFT;
    if (X == 0) {
      *I = 0;
    } else if (X > 0xFFFF) {
      *I = NTREEBINS-1;
    } else {
      unsigned int K = (unsigned) sizeof(X)*__CHAR_BIT__ - 1 - (unsigned) __builtin_clz(X);
      *I =  (bindex_t)((K << 1) + ((S >> (K + (TREEBIN_SHIFT-1)) & 1)));
    }
  }

  /* Shift placing maximum resolved bit in a treebin at i as sign bit */
  inline bindex_t leftshift_for_tree_index(size_t i) {
    return ((i == NTREEBINS-1)? 0 : ((SIZE_T_BITSIZE-SIZE_T_ONE) - (((i) >> 1) + TREEBIN_SHIFT - 2)));
  }
      
  /* allocate a large request from the best fitting chunk in a treebin */
  void* tmalloc_large(size_t nb) {
    TreeChunkPtr v = 0;
    size_t rsize = -nb; /* Unsigned negation */
    TreeChunkPtr t;
    bindex_t idx;
    compute_tree_index(nb, &idx);
    if ((t = *gm_->treebin_at(idx)) != 0) {
      /* Traverse tree for this bin looking for node with size == nb */
      size_t sizebits = nb << leftshift_for_tree_index(idx);
      TreeChunkPtr rst = 0;  /* The deepest untaken right subtree */
      for (;;) {
        TreeChunkPtr rt;
        size_t trem = t->chunksize() - nb;
        if (trem < rsize) {
          v = t;
          if ((rsize = trem) == 0)
            break;
        }
        rt = t->child[1];
        t = t->child[(sizebits >> (SIZE_T_BITSIZE-SIZE_T_ONE)) & 1];
        if (rt != 0 && rt != t)
          rst = rt;
        if (t == 0) {
          t = rst; /* set t to least subtree holding sizes > nb */
          break;
        }
        sizebits <<= 1;
      }
    }
    if (t == 0 && v == 0) { /* set t to root of next non-empty treebin */
      binmap_t leftbits = left_bits(idx2bit(idx)) & gm_->treemap;
      if (leftbits != 0) {
        bindex_t i;
        binmap_t leastbit = least_bit(leftbits);
        compute_bit2idx(leastbit, &i);
        t = *treebin_at(i);
      }
    }

    while (t != 0) { /* find smallest of tree or subtree */
      size_t trem = t->chunksize() - nb;
      if (trem < rsize) {
        rsize = trem;
        v = t;
      }
      t = t->leftmost_child();
    }

    /*  If dv is a better fit, return 0 so malloc will use it */
    if (v != 0 && rsize < (size_t)(dvsize - nb)) {
      if (RTCHECK(ok_address(v))) { /* split */
        ChunkPtr r = chunk_plus_offset(v, nb);
        DEBUG_ASSERT(chunksize(v) == rsize + nb);
        if (RTCHECK(ok_next(v, r))) {
          unlink_large_chunk(v);
          if (rsize < MIN_CHUNK_SIZE)
            set_inuse_and_pinuse(v, (rsize + nb));
          else {
            set_size_and_pinuse_of_inuse_chunk(v, nb);
            r->set_size_and_pinuse_of_free_chunk(rsize);
            insert_chunk(r, rsize);
          }
          return v->chunk2mem();
        }
      }
      throw MemoryCorruptedException();
    }
    return 0;
  }

  /* Unlink a chunk from a smallbin  */
  inline void unlink_small_chunk(ChunkPtr P, size_t S) {
    ChunkPtr F = P->fd;
    ChunkPtr B = P->bk;
    bindex_t I = small_index(S);
    DEBUG_ASSERT(P != B);
    DEBUG_ASSERT(P != F);
    DEBUG_ASSERT(chunksize(P) == small_index2size(I));
    if (RTCHECK(F == smallbin_at(gm_, I) || (ok_address(F) && F->bk == P))) {
      if (B == F) {
        clear_smallmap(I);
      } else if (RTCHECK(B == smallbin_at(gm_, I) ||
                      (ok_address(B) && B->fd == P))) {
        F->bk = B;
        B->fd = F;
      } else {
        throw MemoryCorruptedException();
      }
    } else {
      throw MemoryCorruptedException();
    }
  }

  inline void unlink_chunk(ChunkPtr P, size_t S) {
    if (is_small(S)) {
      unlink_small_chunk(P, S);
    } else {
      TreeChunkPtr TP = static_cast<TreeChunkPtr>(P);
      unlink_large_chunk(TP); 
    }
  }

  /* Check properties of free chunks */
  inline void do_check_free_chunk(ChunkPtr p) {
    size_t sz = p->chunksize();
    ChunkPtr next = chunk_plus_offset(p, sz);
    do_check_any_chunk(p);
    DEBUG_ASSERT(!p->is_inuse());
    DEBUG_ASSERT(!p->next_pinuse());
    // DEBUG_ASSERT(!p->is_mmapped());
    if (p != dv && p != top) {
      if (sz >= MIN_CHUNK_SIZE) {
        DEBUG_ASSERT((sz & CHUNK_ALIGN_MASK) == 0);
        DEBUG_ASSERT(is_aligned(p->chunk2mem()));
        DEBUG_ASSERT(next->prev_foot == sz);
        DEBUG_ASSERT(p->pinuse());
        DEBUG_ASSERT (next == top || next->is_inuse());
        DEBUG_ASSERT(p->fd->bk == p);
        DEBUG_ASSERT(p->bk->fd == p);
      } else {  /* markers are always of size SIZE_T_SIZE */
        DEBUG_ASSERT(sz == SIZE_T_SIZE);
      }
    }
  }

  inline bool should_trim(size_t s) {
    return s > trim_check;
  }

  };
  typedef MallocState* MallocStatePtr;

  // global malloc state
  static MallocStatePtr gm_;
};
