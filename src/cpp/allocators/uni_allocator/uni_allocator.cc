#include "uni_allocator.h"

/*
 * NOTE: The most part of this file is copied form Doug Lea's implementation.
 * 
 * STYLE NOTE: Mixed c-style with google C++ style, should fix.
 *  
 *
*/

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
      b = smallbin_at(gm_, idx);
      p = b->fd;
      if (p->chunksize() == small_index2size(idx)) {
        throw;
      }
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
        b = smallbin_at(gm_, i);
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
      } else if (gm_->treemap != 0 && (mem = tmalloc_small(gm, nb)) != 0) {
        gm_->do_check_malloced_chunk(mem, nb);
        return mem;
      }
    }
  } else if (bytes >= MAX_REQUEST) {
    nb = MAX_SIZE_T; /* Too big to allocate. Force failure (in sys alloc) */
  } else {
    nb = pad_request(bytes);
    if (gm_->treemap != 0 && (mem = tmalloc_large(gm, nb)) != 0) {
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
      gm->dvsize = 0;
      gm->dv = 0;
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
    mem = chunk2mem(p);
    check_top_chunk(gm, gm->top);
    check_malloced_chunk(gm, mem, nb);
    return mem;
  }

  mem = sys_alloc(gm_, nb);
  return mem;
}