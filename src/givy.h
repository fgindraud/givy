#ifndef GIVY_H
#define GIVY_H

#include "block.h"

namespace Givy {

/* Init
 */
void init (int & argc, char **& argv);

/* Allocator interface
 */
Block allocate (size_t size, size_t align);
void deallocate (void * ptr);

/* Coherence interface
 */
void require_read_only (void * ptr);
void require_read_write (void * ptr);

}

#endif
