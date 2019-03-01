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
    smallbits = gm->smallmap >> idx;

    if ((smallbits & 0x3U) != 0) { /* Remainderless fit to a smallbin. */
      ChunkPtr b, p;
      idx += ~smallbits & 1;       /* Uses next bin if idx empty */
      b = smallbin_at(gm, idx);
      p = b->fd;
      assert(chunksize(p) == small_index2size(idx));
      unlink_first_small_chunk(gm, b, p, idx);
      set_inuse_and_pinuse(gm, p, small_index2size(idx));
      mem = chunk2mem(p);
      check_malloced_chunk(gm, mem, nb);
      return mem;
    } else if (nb > gm->dvsize) {
      if (smallbits != 0) { /* Use chunk in next nonempty smallbin */
        ChunkPtr b, p, r;
        size_t rsize;
        bindex_t i;
        binmap_t leftbits = (smallbits << idx) & left_bits(idx2bit(idx));
        binmap_t leastbit = least_bit(leftbits);
        compute_bit2idx(leastbit, i);
        b = smallbin_at(gm, i);
        p = b->fd;
        assert(chunksize(p) == small_index2size(i));
        unlink_first_small_chunk(gm, b, p, i);
        rsize = small_index2size(i) - nb;
        /* Fit here cannot be remainderless if 4byte sizes */
        if (SIZE_T_SIZE != 4 && rsize < MIN_CHUNK_SIZE)
          set_inuse_and_pinuse(gm, p, small_index2size(i));
        else {
          set_size_and_pinuse_of_inuse_chunk(gm, p, nb);
          r = chunk_plus_offset(p, nb);
          set_size_and_pinuse_of_free_chunk(r, rsize);
          replace_dv(gm, r, rsize);
        }
        mem = chunk2mem(p);
        check_malloced_chunk(gm, mem, nb);
        return mem;
      } else if (gm->treemap != 0 && (mem = tmalloc_small(gm, nb)) != 0) {
        check_malloced_chunk(gm, mem, nb);
        return mem;
      }
    }
  } else if (bytes >= MAX_REQUEST) {
    nb = MAX_SIZE_T; /* Too big to allocate. Force failure (in sys alloc) */
  } else {
    nb = pad_request(bytes);
    if (gm->treemap != 0 && (mem = tmalloc_large(gm, nb)) != 0) {
      check_malloced_chunk(gm, mem, nb);
      return mem;
    }
  }

  if (nb <= gm->dvsize) {
    size_t rsize = gm->dvsize - nb;
    ChunkPtr p = gm->dv;
    if (rsize >= MIN_CHUNK_SIZE) { /* split dv */
      ChunkPtr r = gm->dv = chunk_plus_offset(p, nb);
      gm->dvsize = rsize;
      set_size_and_pinuse_of_free_chunk(r, rsize);
      set_size_and_pinuse_of_inuse_chunk(gm, p, nb);
    }
    else { /* exhaust dv */
      size_t dvs = gm->dvsize;
      gm->dvsize = 0;
      gm->dv = 0;
      set_inuse_and_pinuse(gm, p, dvs);
    }
    mem = chunk2mem(p);
    check_malloced_chunk(gm, mem, nb);
    return mem;
  } else if (nb < gm->topsize) { /* Split top */
    size_t rsize = gm->topsize -= nb;
    ChunkPtr p = gm->top;
    ChunkPtr r = gm->top = chunk_plus_offset(p, nb);
    r->head = rsize | PINUSE_BIT;
    set_size_and_pinuse_of_inuse_chunk(gm, p, nb);
    mem = chunk2mem(p);
    check_top_chunk(gm, gm->top);
    check_malloced_chunk(gm, mem, nb);
    return mem;
  }

  mem = sys_alloc(gm, nb);
  return mem;
}