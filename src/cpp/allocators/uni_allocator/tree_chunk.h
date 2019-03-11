#pragma once

#include <stdlib.h>
#include "chunk.h"

typedef unsigned int bindex_t;

class TreeChunk;
typedef TreeChunk* TreeChunkPtr;
typedef struct TreeChunk* tbinptr; /* The type of bins of trees */

// NOTE: TreeChunk shares all the public member functions with Chunk
class TreeChunk : public Chunk {
 public:
  inline TreeChunkPtr leftmost_child() {
    return (this->child[0] != 0? this->child[0] : this->child[1]);
  }
 public:
  /* The first four fields must be compatible with Chunk, they are inherited */
  /* 
  size_t     prev_foot;
  size_t     head;
  TreeChunk* fd;
  TreeChunk* bk;
  */

  TreeChunk* child[2];
  TreeChunk* parent;
  bindex_t   index;
};
