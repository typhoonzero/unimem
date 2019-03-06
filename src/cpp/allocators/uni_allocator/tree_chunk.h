#pragma once

#include <stdlib.h>
#include "chunk.h"

typedef unsigned int bindex_t;

// NOTE: TreeChunk shares all the public member functions with Chunk
class TreeChunk : public Chunk {
  /* The first four fields must be compatible with Chunk */
  size_t     prev_foot;
  size_t     head;
  TreeChunk* fd;
  TreeChunk* bk;

  TreeChunk* child[2];
  TreeChunk* parent;
  bindex_t   index;
};

typedef TreeChunk* TreeChunkPtr;
