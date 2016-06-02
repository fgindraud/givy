#pragma once
#ifndef ALLOCATOR_BOOTSTRAP_H
#define ALLOCATOR_BOOTSTRAP_H

#include "reporting.h"
#include "block.h"
#include "math.h"

#include <cstdlib>

namespace Givy {
namespace Allocator {
	class Bootstrap {
		/* Bootstrap allocator is temporarily malloc/free of libc.
		 * TODO use mmap (will break if symbols are overriden)
		 */
	public:
		Block allocate (size_t size, size_t align) {
			ASSERT_SAFE (align >= sizeof (void *));
			ASSERT_SAFE (Math::is_power_of_2 (align));
			ASSERT_SAFE (size > 0);
			void * p = nullptr;
			int r = posix_memalign (&p, align, size);
			(void) r;
			ASSERT_SAFE (r == 0);
			return {p, size};
		}
		void deallocate (Block blk) { free (blk.ptr); }
	};
}
}

#endif
