// DEBUG
#include <cstdio>
#include <cassert>

#include "alloc_parts.h"
#include "base_defs.h"
#include "superpage_tracker.h"

//////////////////////////////////

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

	//RegionHeader * region_header (void * ptr);
};

thread_local int tid = -1;

////////////////////////////////



struct PageBlockHeader {
	enum class Type {
		UnavailableSuperpageSpace,
		Unused,
		StartOfPageSequence,
		InsidePageSequence,
		InsideHugeAlloc,
	};

	Type type;
};



struct SuperpageHeader {
	size_t superpage_nb; // If > 1, indicates a huge alloc
	int owner; // thread id
	PageBlockHeader * unusedBySize[10]; // freelist of unused chunks
	
	PageBlockHeader page_headers[VMem::SuperpagePageNB];

	SuperpageHeader (size_t superpage_nb_) :
		superpage_nb (superpage_nb_)
	{
		constexpr size_t superpage_header_nb_page = Math::divide_up (sizeof (SuperpageHeader), VMem::PageSize);
		for (size_t i = 0; i < superpage_header_nb_page; ++i)
			page_headers[i].type = PageBlockHeader::Type::UnavailableSuperpageSpace;
		for (size_t i = superpage_header_nb_page; i < VMem::SuperpagePageNB; ++i)
			page_headers[i].type = PageBlockHeader::Type::Unused;

	}
};

struct RegionHeader {
	// defs
	enum class Type {
		Blah = 0
	};
	struct Splitted {
	};
};

struct GasAllocator {
	using BootstrapAllocator = AllocParts::BackwardBumpPointer;

	enum AllocTypeRange : size_t {
		// Small allocs: using slab system inside pages
		SmallL = sizeof (void *), // TODO choose this later
		SmallH = 32 * VMem::PageSize,
		// Big allocs: few pages, inside superpages
		BigL = SmallH,
		BigH = VMem::SuperpageSize, // TODO should be a bit smaller, as superpage contains metadata...
		// Huge allocs: one or multiple superpages
		HugeL = BigH
	};

	GasLayout layout;
	int node; // Local node

	BootstrapAllocator bootstrap_allocator;
	SuperpageTracker superpage_tracker;

	GasAllocator (const GasLayout & layout_, int node_) :
		layout (layout_),
		node (node_),
		bootstrap_allocator (layout.start),
		superpage_tracker (layout, bootstrap_allocator)
	{
	}

	Ptr allocate (size_t size, size_t align, int thread);
	void deallocate (Ptr ptr, int thread);
	
	// TODO ? extended interface stuff ?
	int allocate_n (size_t size, size_t align, void * ptr_storage, int n, int thread);
	void deallocate_sized (Ptr ptr, size_t size, int thread);
};

Ptr GasAllocator::allocate (size_t size, size_t align, int thread) {
	/* Max supported alignment is SuperpageSize */
	if (size < AllocTypeRange::SmallH) {
		// Small alloc
		// find sizeclass, normalize size
	} else if (size < AllocTypeRange::BigH) {
		// Big alloc
	} else {
		// Huge alloc
		/*Ptr superpage_reservation = superpage_tracker.acquire (
				Math::divide_up (size, VMem::SuperpageSize), node);*/
		// Map, carve superpage header at front, put allocation as far as possible and let the start be used as partial superpage for big/small allocations
	}
	return nullptr;
}

void GasAllocator::deallocate (Ptr ptr, int thread) {
}

int main (void) {
	VMem::runtime_asserts ();

	/* Determining program break, placing Givy after (+ some pages in the middle) */
	auto layout = GasLayout (
			Ptr (sbrk (0)) + 1000 * VMem::PageSize, // start
			1000 * VMem::PageSize, // space_by_node
			4); // nb_node
	GasAllocator allocator (layout, 0);
	return 0;
}

