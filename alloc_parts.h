#ifndef ALLOC_PARTS_H
#define ALLOC_PARTS_H

#include "assert_level.h"
#include "base_defs.h"

namespace AllocParts {

	struct Block {
		Ptr ptr;
		size_t size;
	};
	
	/* System */

	struct System {
		Block allocate (size_t size, size_t) { return {Ptr (new char[size]), size}; }
		void deallocate (Block blk) { delete[] blk.ptr.as<char *> (); }
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
			ASSERT_OPT (start.is_aligned (VMem::PageSize));
		}
		~BumpPointerBase () {
			VMem::unmap_noexcept (left_mapped_, right_mapped_ - left_mapped_);
		}
		
		Block allocate_right (size_t size, size_t align) {
			Ptr user_mem = right_.align_up (align);
			right_ = user_mem + size;
			if (! (right_ < right_mapped_)) {
				size_t to_map = right_.align_up (VMem::PageSize) - right_mapped_;
				VMem::map (right_mapped_, to_map);
				right_mapped_ += to_map;
			}
			return {user_mem, size};
		}
		Block allocate_left (size_t size, size_t align) {
			left_ = left_.sub (size).align (align);
			if (! (left_ >= left_mapped_)) {
				Ptr map_from = left_.align (VMem::PageSize);
				VMem::map (map_from, left_mapped_ - map_from);
			}
			return {left_, size};
		}
	};
	
	struct BumpPointer : public BumpPointerBase {
		BumpPointer (Ptr start) : BumpPointerBase (start) {}
		Block allocate (size_t size, size_t align) {
			return allocate_right (size, align);
		}
	};
	struct BackwardBumpPointer : public BumpPointerBase {
		BackwardBumpPointer (Ptr start) : BumpPointerBase (start) {}
		Block allocate (size_t size, size_t align) {
			return allocate_left (size, align);
		}
	};

	/* Combinators */

	template<typename Alloc> class Ref {
		private:
			Alloc & ref;
		public:
			Ref (Alloc & ref_) : ref (ref_) {}
			Block allocate (size_t size) { return ref.allocate (size); }
			void deallocate (Block blk) { ref.deallocate (blk); }
			bool owns (Block blk) { return ref.owns (blk); }
	};

	template<typename Primary, typename Secondary>
	struct Fallback : private Primary, private Secondary {
		public:
			Block allocate (size_t size) {
				Block blk = Primary::allocate (size);
				if (blk.ptr == nullptr)
					blk = Secondary::allocate (size);
				return blk;
			}
			void deallocate (Block blk) {
				if (Primary::owns (blk))
					Primary::deallocate (blk);
				else
					Secondary::deallocate (blk);
			}
			bool owns (Block blk) {
				return Primary::owns (blk) || Secondary::owns (blk);
			}
	};

}

#endif
