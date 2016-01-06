#ifndef ALLOCATOR_BOOTSTRAP_H
#define ALLOCATOR_BOOTSTRAP_H

#include "reporting.h"
#include "gas_layout.h"
#include "pointer.h"
#include "memory_mapping.h"

namespace Givy {
namespace Bootstrap {
	class Allocator {
		/* Boostrap allocator is a down growing bump pointer allocator.
		 *
		 * Not thread safe.
		 * Always return memory (or fails).
		 * Behave as region allocators : cannot deallocate invididual blocks, but deallocates everything
		 * at destruction.
		 */
	private:
		Ptr left;        // leftmost point of used segment (first used byte)
		Ptr left_mapped; // leftmost mapped segment (first mapped byte)
		Ptr end_mapped;  // rightmost mapped segment (after last mapped byte)

	public:
		explicit Allocator (Ptr start)
		    : left (start), left_mapped (start), end_mapped (start) {
			ASSERT_SAFE (start.is_aligned (VMem::PageSize));
		}
		~Allocator () {
			if (left_mapped < end_mapped)
				VMem::unmap (left_mapped, end_mapped - left_mapped);
		}

		Block allocate (size_t size, size_t align) {
			left = left.sub (size).align (align);
			if (!(left >= left_mapped)) {
				Ptr map_from = left.align (VMem::PageSize);
				VMem::map_checked (map_from, left_mapped - map_from);
			}
			return {left, size};
		}
		void deallocate (Block) {}
	};
}
}

#endif
