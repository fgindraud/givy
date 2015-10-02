#include <climits> // CHAR_BIT
#include <atomic>

// DEBUG
#include <iostream>
#include <cstdio>

#include "alloc_parts.h"
#include "base_defs.h"

struct SuperpageTracker {

	using IntType = std::intmax_t;
	using AtomicIntType = std::atomic<IntType>;
	enum {
		BitsPerInt = sizeof (IntType) * CHAR_BIT,
		BitsPerSuperpage = 2,
	};

	size_t array_size_;
	AtomicIntType * int_array_;

	template<typename Alloc>
	SuperpageTracker (size_t superpage_nb, Alloc & allocator) :
		array_size_ (Math::divide_up (superpage_nb, BitsPerInt / BitsPerSuperpage))
	{
		static_assert (
				AllocParts::has_trait<Alloc, AllocParts::Traits::FreesEverythingAtDestruction> (),
				"SuperpageTracker requires a FreesEverythingAtDestruction allocator");

		int_array_ = allocator.allocate (array_size_ * sizeof (AtomicIntType), alignof (AtomicIntType));
		for (size_t it = 0; it < array_size_; ++it)
			new (&int_array_[it]) AtomicIntType (0);
	}
};

struct Superpage {
	/* Aligned to 2MB : kernel will use hugepage if it can */
};

struct RegionHeader {
	// defs
	enum class Type {
		Blah = 0
	};
	struct Splitted {
	};
};

struct GasLayout {
	Ptr gas_start;
	size_t gas_space_by_node;
	int nb_node;
};

struct GasAllocator {
	using BootstrapAllocator = AllocParts::BackwardBumpPointer;

	GasLayout layout_;
	BootstrapAllocator bootstrap_allocator_;
	SuperpageTracker superpage_tracker_;

	GasAllocator (const GasLayout & layout) :
		layout_ (layout),
		bootstrap_allocator_ (layout_.gas_start),
		superpage_tracker_ (
				Math::divide_up (layout_.gas_space_by_node, VMem::SuperpageSize) * layout_.nb_node,
				bootstrap_allocator_)
	{
	}
};

struct Gas {
	int node_id_;
	int nb_threads_;

	/***** Interface *******/

	/* Region creation/destruction (equivalent to malloc / posix_memalign / free)
	 * Take a thread_id parameter, will be initialised if -1
	 */
	void * region_create (size_t size, int & tid); // local
	void * region_create_aligned (size_t size, size_t alignment, int & tid); // local
	void region_destroy (void * ptr, int & tid); // may call gas if remote copies

	/* Split/merge
	 * always take the pointer to the block itself
	 */
	void split (void * ptr, size_t nb_piece); // will notify remotes
	void merge (void * ptr); // will notify remotes

	/* Extended interface: "lazy_create"
	 * Will map to normal create for small blocks.
	 * Delayed malloc for bigger ones (multi superpages)
	 */
	void * region_create_noalloc (size_t size, int & tid); // local
	void region_delayed_alloc (void * ptr); //

	/* Threaded model coherence interface
	 * Assumes DRF accesses
	 */
	enum AccessMode { NoAccess, ReadOnly, ReadWrite };
	void region_ensure_accesses (void ** regions, AccessMode * modes, int nb_region);

	/***** Internals ******/

	RegionHeader * region_header (void * ptr);
};


thread_local int tid = -1;

int main (void) {
	using AllocParts::System;
	VMem::runtime_asserts ();

	/* Determining program break, placing Givy after (+ some pages in the middle) */
	GasLayout layout = {
		.gas_start = Ptr (sbrk (0)) + 1000 * VMem::PageSize,
		.gas_space_by_node = 1000 * VMem::PageSize,
		.nb_node = 4
	};
	GasAllocator allocator (layout);

	return 0;
}

