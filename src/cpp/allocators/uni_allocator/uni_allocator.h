#include <string>
#include "src/cpp/allocator.h"
#include "src/cpp/allocator_backends/system_alloc.h"
#include "chunk.h"
#include "tree_chunk.h"

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

// bytes < 256
#define is_small(s)         (((s) >> SMALLBIN_SHIFT) < NSMALLBINS)
#define small_index(s)      (bindex_t)((s)  >> SMALLBIN_SHIFT)
#define small_index2size(i) ((i)  << SMALLBIN_SHIFT)
#define MIN_SMALL_INDEX     (small_index(MIN_CHUNK_SIZE))

#define smallbin_at(M, i)   ((sbinptr)((char*)&((M)->smallbins[(i)<<1])))

class UniAllocator : public Allocator {
 public:
  explicit UniAllocator(Allocator* underlying_allocator) : 
    underlying_allocator_(underlying_allocator) {};
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
  };

  // global malloc status
  MallocState gm_;
}