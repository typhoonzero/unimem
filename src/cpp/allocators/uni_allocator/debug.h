#include "exception.h"
#ifdef DEBUG

#define DEBUG_ASSERT(COND) if(!(x)) throw DebugAssertionFail()

#else

#define DEBUG_ASSERT(COND)

#endif