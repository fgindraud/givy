#ifndef ALLOC_PARTS_H
#define ALLOC_PARTS_H

#include "reporting.h"
#include "base_defs.h"
#include "utility.h"

namespace Givy {
namespace Allocator {
	namespace Parts {

		/* Bump pointer allocators
		 *
		 * Not thread safe.
		 * Always return memory (or throws).
		 * Behave as region allocators : cannot deallocate invididual blocks, but deallocates everything
		 * at destruction.
		 */

		class BumpPointerBase {
		private:
			Ptr left;         /* leftmost point of used segment (first used byte) */
			Ptr left_mapped;  /* leftmost mapped segment (first mapped byte) */
			Ptr right;        /* rightmost point of used segment (after last used byte) */
			Ptr right_mapped; /* leftmost mapped segment (after last mapped byte) */

		public:
			explicit BumpPointerBase (Ptr start)
			    : left (start), left_mapped (start), right (start), right_mapped (start) {
				ASSERT_SAFE (start.is_aligned (VMem::PageSize));
			}
			~BumpPointerBase () {
				if (left_mapped < right_mapped)
					VMem::unmap (left_mapped, right_mapped - left_mapped);
			}

			Block allocate_right (size_t size, size_t align) {
				Ptr user_mem = right.align_up (align);
				right = user_mem + size;
				if (!(right < right_mapped)) {
					size_t to_map = right.align_up (VMem::PageSize) - right_mapped;
					VMem::map_checked (right_mapped, to_map);
					right_mapped += to_map;
				}
				return {user_mem, size};
			}
			Block allocate_left (size_t size, size_t align) {
				left = left.sub (size).align (align);
				if (!(left >= left_mapped)) {
					Ptr map_from = left.align (VMem::PageSize);
					VMem::map_checked (map_from, left_mapped - map_from);
				}
				return {left, size};
			}
			void deallocate (Block) {}
		};

		class BumpPointer : public BumpPointerBase {
		public:
			explicit BumpPointer (Ptr start) : BumpPointerBase (start) {}
			Block allocate (size_t size, size_t align) { return allocate_right (size, align); }
		};

		class BackwardBumpPointer : public BumpPointerBase {
		public:
			explicit BackwardBumpPointer (Ptr start) : BumpPointerBase (start) {}
			Block allocate (size_t size, size_t align) { return allocate_left (size, align); }
		};
	}
}
}

#endif
