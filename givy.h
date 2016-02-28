#ifndef GIVY_H
#define GIVY_H

#include "pointer.h"

namespace Givy {
/* Init
 */
void init (int & argc, char **& argv);

/* Allocator interface
 */
Block allocate (size_t size, size_t align);
void deallocate (Block blk);
void deallocate (Ptr ptr);
}

#endif
