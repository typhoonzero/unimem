#include "src/cpp/allocators/uni_allocator/uni_allocator.h"

#include <iostream>

/*
 * NOTE: The most part of this file is copied form Doug Lea's implementation.
 * 
 * STYLE NOTE: Mixed c-style with google C++ style, should fix.
 *  
 *
*/

void UniAllocator::InitOrExpand() {
  std::cout << "InitOrExpand, increasing_step_" << increasing_step_ << std::endl;
  // Since device memory do not have utilities like "morecore" or "mmap"
  // we can not allocate a **INCREASE** memory which keeps the base address
  // the same.
  size_t tsize = increasing_size_ * increasing_step_;
  char* tbase = static_cast<char*>(underlying_allocator_->Alloc(tsize));
  
  // tbase should be sys_alloced memory
  if (!gm_->is_initialized()) { /* first-time initialization */
    if (gm_->least_addr == 0 || tbase < gm_->least_addr) {
      gm_->least_addr = tbase;
    }
    // gm_->seg.base = tbase;
    // gm_->seg.size = tsize;
    // gm_->seg.sflags = 0;
    // gm_->magic = mparams.magic;
    gm_->magic = SIZE_T_ONE;
    gm_->release_checks = MAX_RELEASE_CHECK_RATE;
    gm_->init_bins();
    gm_->init_top(reinterpret_cast<ChunkPtr>(tbase), tsize - TOP_FOOT_SIZE);
  }
}

UniAllocator::~UniAllocator() {
  delete gm_;
}

void* UniAllocator::Alloc(size_t bytes) {
  void* mem;
  size_t nb;
  if (bytes <= MAX_SMALL_REQUEST) {
    bindex_t idx;
    binmap_t smallbits;
    nb = (bytes < MIN_REQUEST)? MIN_CHUNK_SIZE : pad_request(bytes);

    idx = small_index(nb);
    smallbits = gm_->smallmap >> idx;

    if ((smallbits & 0x3U) != 0) { /* Remainderless fit to a smallbin. */
      ChunkPtr b, p;
      idx += ~smallbits & 1;       /* Uses next bin if idx empty */
      b = gm_->smallbin_at(idx);
      p = b->fd;
      DEBUG_ASSERT(p->chunksize() == small_index2size(idx));
      gm_->unlink_first_small_chunk(b, p, idx);
      gm_->set_inuse_and_pinuse(p, small_index2size(idx));
      mem = p->chunk2mem();
      // TODO: debug mode should not do checks
      gm_->do_check_malloced_chunk(mem, nb);
      return mem;
    } else if (nb > gm_->dvsize) {
      if (smallbits != 0) { /* Use chunk in next nonempty smallbin */
        ChunkPtr b, p, r;
        size_t rsize;
        bindex_t i;
        binmap_t leftbits = (smallbits << idx) & gm_->left_bits(gm_->idx2bit(idx));
        binmap_t leastbit = gm_->least_bit(leftbits);
        gm_->compute_bit2idx(leastbit, &i);
        b = gm_->smallbin_at(i);
        p = b->fd;
        DEBUG_ASSERT(chunksize(p) == small_index2size(i));
        gm_->unlink_first_small_chunk(b, p, i);
        rsize = small_index2size(i) - nb;
        /* Fit here cannot be remainderless if 4byte sizes */
        if (SIZE_T_SIZE != 4 && rsize < MIN_CHUNK_SIZE)
          gm_->set_inuse_and_pinuse(p, small_index2size(i));
        else {
          gm_->set_size_and_pinuse_of_inuse_chunk(p, nb);
          r = chunk_plus_offset(p, nb);
          r->set_size_and_pinuse_of_free_chunk(rsize);
          gm_->replace_dv(r, rsize);
        }
        mem = p->chunk2mem();
        gm_->do_check_malloced_chunk(mem, nb);
        return mem;
      } else if (gm_->treemap != 0 && (mem = gm_->tmalloc_small(nb)) != 0) {
        gm_->do_check_malloced_chunk(mem, nb);
        return mem;
      }
    }
  } else if (bytes >= MAX_REQUEST) {
    nb = MAX_SIZE_T; /* Too big to allocate. Force failure (in sys alloc) */
  } else {
    nb = pad_request(bytes);
    if (gm_->treemap != 0 && (mem = gm_->tmalloc_large(nb)) != 0) {
      gm_->do_check_malloced_chunk(mem, nb);
      return mem;
    }
  }

  if (nb <= gm_->dvsize) {
    size_t rsize = gm_->dvsize - nb;
    ChunkPtr p = gm_->dv;
    if (rsize >= MIN_CHUNK_SIZE) { /* split dv */
      ChunkPtr r = gm_->dv = chunk_plus_offset(p, nb);
      gm_->dvsize = rsize;
      r->set_size_and_pinuse_of_free_chunk(rsize);
      p->set_size_and_pinuse_of_inuse_chunk(nb);
    } else { /* exhaust dv */
      size_t dvs = gm_->dvsize;
      gm_->dvsize = 0;
      gm_->dv = 0;
      gm_->set_inuse_and_pinuse(p, dvs);
    }
    mem = p->chunk2mem();
    gm_->do_check_malloced_chunk(mem, nb);
    return mem;
  } else if (nb < gm_->topsize) { /* Split top */
    size_t rsize = gm_->topsize -= nb;
    ChunkPtr p = gm_->top;
    ChunkPtr r = gm_->top = chunk_plus_offset(p, nb);
    r->head = rsize | PINUSE_BIT;
    gm_->set_size_and_pinuse_of_inuse_chunk(p, nb);
    mem = p->chunk2mem();
    // TODO: this check should be enabled in debug mode
    // check_top_chunk(gm, gm->top);
    gm_->do_check_malloced_chunk(mem, nb);
    return mem;
  }

  InitOrExpand();
  mem = Alloc(bytes);
  // mem = sys_alloc(gm_, nb);
  return mem;
}

/* ---------------------------- free --------------------------- */
void UniAllocator::Free(void* mem) {
  /*
     Consolidate freed chunks with preceeding or succeeding bordering
     free chunks, if they exist, and then place in a bin.  Intermixed
     with special cases for top, dv, mmapped chunks, and usage errors.
  */

  if (mem != 0) {
    ChunkPtr p  = mem2chunk(mem);
#if FOOTERS
    mstate fm = get_mstate_for(p);
    if (!ok_magic(fm)) {
      USAGE_ERROR_ACTION(fm, p);
      return;
    }
#else /* FOOTERS */
#define fm gm
#endif /* FOOTERS */
    gm_->do_check_inuse_chunk(p);
    if (RTCHECK(gm_->ok_address(p) && p->is_inuse())) {
      size_t psize = p->chunksize();
      ChunkPtr next = chunk_plus_offset(p, psize);
      if (!p->pinuse()) {
        size_t prevsize = p->prev_foot;
        {
          ChunkPtr prev = chunk_minus_offset(p, prevsize);
          psize += prevsize;
          p = prev;
          if (RTCHECK(gm_->ok_address(prev))) { /* consolidate backward */
            if (p != gm_->dv) {
              gm_->unlink_chunk(p, prevsize);
            } else if ((next->head & INUSE_BITS) == INUSE_BITS) {
              gm_->dvsize = psize;
              p->set_free_with_pinuse(psize, next);
              return;
            }
          } else {
            throw MemoryCorruptedException();
          }
        }
      }

      if (RTCHECK(ok_next(p, next) && next->pinuse())) {
        if (!next->cinuse()) {  /* consolidate forward */
          if (next == gm_->top) {
            size_t tsize = gm_->topsize += psize;
            gm_->top = p;
            p->head = tsize | PINUSE_BIT;
            if (p == gm_->dv) {
              gm_->dv = 0;
              gm_->dvsize = 0;
            }
            if (gm_->should_trim(tsize)) {
              gm_->sys_trim(0);
            }
            return;
          } else if (next == gm_->dv) {
            size_t dsize = gm_->dvsize += psize;
            gm_->dv = p;
            p->set_size_and_pinuse_of_free_chunk(dsize);
            return;
          } else {
            size_t nsize = next->chunksize();
            psize += nsize;
            gm_->unlink_chunk(next, nsize);
            p->set_size_and_pinuse_of_free_chunk(psize);
            if (p == gm_->dv) {
              gm_->dvsize = psize;
              return;
            }
          }
        } else {
          p->set_free_with_pinuse(psize, next);
        }

        if (is_small(psize)) {
          gm_->insert_small_chunk(p, psize);
          gm_->do_check_free_chunk(p);
        } else {
          TreeChunkPtr tp = static_cast<TreeChunkPtr>(p);
          gm_->insert_large_chunk(tp, psize);
          gm_->do_check_free_chunk(p);
          // if (--fm->release_checks == 0) {
          //   release_unused_segments(fm);
          // }
        }
        return;
      }
    }
  }
#if !FOOTERS
#undef fm
#endif /* FOOTERS */
}