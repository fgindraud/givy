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
void deallocate (Ptr ptr);

/* Coherence interface
 */
void require_read_only (Ptr ptr);
void require_read_write (Ptr ptr);

}

#endif
