#ifndef ALLOC_PARTS_H
#define ALLOC_PARTS_H

#include <stdexcept>
#include <type_traits>

#include "base_defs.h"

namespace AllocParts {

	struct Block {
		Ptr ptr;
		size_t size;
	};
	
	/* System (for tests) */

	struct System {
		Ptr allocate (size_t size, size_t) { return Ptr (new char[size]); }
		void deallocate (Ptr ptr) { delete[] ptr.as<char *> (); }
	};

	/* Basic */
	
	struct BumpPointerBase {
		Ptr left_; /* leftmost point of used segment (first used byte) */
		Ptr left_mapped_; /* leftmost mapped segment (first mapped byte) */
		Ptr right_; /* rightmost point of used segment (after last used byte) */
		Ptr right_mapped_; /* leftmost mapped segment (after last mapped byte) */
		
		BumpPointerBase (Ptr start) :
			left_ (start),
			left_mapped_ (start),
			right_ (start),
			right_mapped_ (start)
		{
			if (not start.is_aligned (VMem::PageSize))
				throw std::runtime_error ("BumpPointerBase(): start not aligned");
		}
		~BumpPointerBase () {
			VMem::unmap_noexcept (left_mapped_, right_mapped_ - left_mapped_);
		}
		
		Ptr allocate_right (size_t size, size_t align) {
			Ptr user_mem = right_.align_up (align);
			right_ = user_mem + size;
			if (! (right_ < right_mapped_)) {
				size_t to_map = right_.align_up (VMem::PageSize) - right_mapped_;
				VMem::map (right_mapped_, to_map);
				right_mapped_ += to_map;
			}
			return user_mem;
		}
		Ptr allocate_left (size_t size, size_t align) {
			left_ = left_.sub (size).align (align);
			if (! (left_ >= left_mapped_)) {
				Ptr map_from = left_.align (VMem::PageSize);
				VMem::map (map_from, left_mapped_ - map_from);
			}
			return left_;
		}
	};
	
	struct BumpPointer : public BumpPointerBase {
		BumpPointer (Ptr start) : BumpPointerBase (start) {}
		Ptr allocate (size_t size, size_t align) {
			return allocate_right (size, align);
		}
	};
	struct BackwardBumpPointer : public BumpPointerBase {
		BackwardBumpPointer (Ptr start) : BumpPointerBase (start) {}
		Ptr allocate (size_t size, size_t align) {
			return allocate_left (size, align);
		}
	};

}

#endif
