// DEBUG
#include <cstdio>
#include <cassert>

#include "alloc_parts.h"
#include "base_defs.h"
#include "superpage_tracker.h"
#include "chain.h"

//////////////////////////////////

struct Gas {
	int node_id_;
	int nb_threads_;

	/***** Interface *******/

	/* Region creation/destruction (equivalent to malloc / posix_memalign / free)
	 * Take a thread_id parameter, will be initialised if -1
	 */
	void * region_create (size_t size, int & tid);                           // local
	void * region_create_aligned (size_t size, size_t alignment, int & tid); // local
	void region_destroy (void * ptr, int & tid);                             // may call gas if remote copies

	/* Split/merge
	 * always take the pointer to the block itself
	 */
	void split (void * ptr, size_t nb_piece); // will notify remotes
	void merge (void * ptr);                  // will notify remotes

	/* Extended interface: "lazy_create"
	 * Will map to normal create for small blocks.
	 * Delayed malloc for bigger ones (multi superpages)
	 */
	void * region_create_noalloc (size_t size, int & tid); // local
	void region_delayed_alloc (void * ptr);                //

	/* Threaded model coherence interface
	 * Assumes DRF accesses
	 */
	enum AccessMode { NoAccess, ReadOnly, ReadWrite };
	void region_ensure_accesses (void ** regions, AccessMode * modes, int nb_region);

	/***** Internals ******/

	// RegionHeader * region_header (void * ptr);
};

thread_local int tid = -1;

////////////////////////////////

enum AllocTypeRange : size_t {
	// Small allocs: using slab system inside pages
	SmallL = sizeof (void *), // TODO choose this later
	SmallH = 32 * VMem::PageSize,
	// Big allocs: few pages, inside superpages
	BigL = SmallH,
	BigH = VMem::SuperpageSize, // TODO should be a bit smaller, as superpage contains metadata...
	// Huge allocs: one or multiple superpages
	HugeL = BigH
	// Small | Big alloc = ContainedAlloc
};

using AllocParts::Block;
struct PageBlockHeader;
struct SuperpageBlock;
struct ThreadLocalHeap;
struct GasAllocator;
using PageBlockUnusedList = QuickList<PageBlockHeader, 10>;

struct PageBlockHeader : public PageBlockUnusedList::Element {
	enum class Type {
		Invalid, // Used to mark header space
		Unused,
		ContainedAlloc,
		HugeAlloc,
	};

	Type type = Type::Invalid;
	size_t block_page_nb = 0;
	PageBlockHeader * head = nullptr;

	size_t length (void) noexcept { return block_page_nb; }

	static void format (PageBlockHeader * pbh, size_t size, Type type) {
		for (size_t i = 0; i < size; ++i) {
			pbh[i].type = type;
			pbh[i].block_page_nb = size;
			pbh[i].head = pbh;
		}
	}
	static void format_unused_list (PageBlockHeader * pbh, size_t size) {
		for (size_t i = 0; i < size; ++i)
			pbh[i].reset ();
	}
};

struct SuperpageBlock : public Chain<SuperpageBlock>::Element {
	struct Header {
		ThreadLocalHeap * owner = nullptr;

		size_t superpage_nb;
		Block huge_alloc = {nullptr, 0};

		PageBlockHeader page_headers[VMem::SuperpagePageNB];
		PageBlockUnusedList unused;
	};

	Header header;

	/* Structure of a superpage
	 * - superpage header, followed by unused space to align to page boundaries
	 * - sequence of pages for small & big allocations
	 * - last pages may be part of a huge_alloc
	 * - a huge alloc will also span over the next superpages, which have no header (managed by this
	 * superpage)
	 */
	static constexpr size_t HeaderSpacePages = Math::divide_up (sizeof (Header), VMem::PageSize);
	static constexpr size_t ContainedAllocMaxPages = VMem::SuperpagePageNB - HeaderSpacePages;

	/* Creation / destruction */
	SuperpageBlock (size_t superpage_nb, size_t huge_alloc_page_nb, ThreadLocalHeap * owner);
	static SuperpageBlock & createForHugeAlloc (SuperpageTracker & tracker, size_t size, ThreadLocalHeap * owner);
	static SuperpageBlock & createForContainedAlloc (SuperpageTracker & tracker, ThreadLocalHeap * owner);
	void destroy_huge_page (void) {}

	/* Alloc page blocks */
	Block get_page_block (PageBlockHeader * pbh);
	PageBlockHeader * allocate_page_block (size_t page_nb);
	void free_page_block (PageBlockHeader * pbh);
};

struct ThreadLocalHeap : public Chain<ThreadLocalHeap>::Element {
	GasAllocator & allocator;
	Chain<SuperpageBlock> superpage_blocks;

	ThreadLocalHeap (GasAllocator & allocator_) : allocator (allocator_) {}
	Block allocate (size_t size, size_t align);
	void deallocate (Block blk);
};

struct GasAllocator {
	using BootstrapAllocator = AllocParts::BackwardBumpPointer;

	GasLayout layout;
	BootstrapAllocator bootstrap_allocator;
	SuperpageTracker superpage_tracker;
	Chain<ThreadLocalHeap> thread_heaps;

	GasAllocator (const GasLayout & layout_)
	    : layout (layout_), bootstrap_allocator (layout.start), superpage_tracker (layout, bootstrap_allocator) {}

	Block allocate (size_t size, size_t align);
	void deallocate (Block blk);

	// TODO ? extended interface stuff ?
	int allocate_n (size_t size, size_t align, void * ptr_storage, int n, int thread);
	void deallocate_sized (Ptr ptr, size_t size, int thread);
};

/* IMPL SuperpageBlock */

SuperpageBlock::SuperpageBlock (size_t superpage_nb, size_t huge_alloc_page_nb, ThreadLocalHeap * owner) {
	header.superpage_nb = superpage_nb;
	header.owner = owner;

	size_t trailing_huge_alloc_pages = huge_alloc_page_nb % VMem::SuperpagePageNB;
	size_t huge_alloc_first_page = VMem::SuperpagePageNB - trailing_huge_alloc_pages;
	if (huge_alloc_page_nb > 0)
		header.huge_alloc = {Ptr (this).add (huge_alloc_first_page * VMem::PageSize), huge_alloc_page_nb * VMem::PageSize};

	PageBlockHeader::format_unused_list (&header.page_headers[0], VMem::SuperpagePageNB);
	PageBlockHeader::format (&header.page_headers[0], HeaderSpacePages, PageBlockHeader::Type::Invalid);
	PageBlockHeader::format (&header.page_headers[HeaderSpacePages], huge_alloc_first_page - HeaderSpacePages,
	                         PageBlockHeader::Type::Unused);
	PageBlockHeader::format (&header.page_headers[huge_alloc_first_page], VMem::SuperpagePageNB - huge_alloc_first_page,
	                         PageBlockHeader::Type::HugeAlloc);
}

SuperpageBlock & SuperpageBlock::createForHugeAlloc (SuperpageTracker & tracker, size_t size, ThreadLocalHeap * owner) {
	// Compute size of allocation
	size_t page_nb = Math::divide_up (size, VMem::PageSize);
	size_t superpage_nb = Math::divide_up (page_nb + SuperpageBlock::HeaderSpacePages, VMem::SuperpagePageNB);
	// Reserve, map, configure
	Ptr superpage_block_start = tracker.acquire (superpage_nb);
	VMem::map (superpage_block_start, superpage_nb * VMem::SuperpageSize);
	return *new (superpage_block_start) SuperpageBlock (superpage_nb, page_nb, owner);
}

SuperpageBlock & SuperpageBlock::createForContainedAlloc (SuperpageTracker & tracker, ThreadLocalHeap * owner) {
	// Reserve, map, configure
	Ptr superpage_block_start = tracker.acquire (1);
	VMem::map (superpage_block_start, VMem::SuperpageSize);
	return *new (superpage_block_start) SuperpageBlock (1, 0, owner);
}

Block SuperpageBlock::get_page_block (PageBlockHeader * pbh) {
	ASSERT_SAFE (&header.page_headers[0] <= pbh);
	ASSERT_SAFE (pbh < &header.page_headers[VMem::SuperpagePageNB]);

	size_t pb_index = pbh - header.page_headers;
	return {Ptr (this).add (pb_index * VMem::PageSize), pbh->block_page_nb * VMem::PageSize};
}

PageBlockHeader * SuperpageBlock::allocate_page_block (size_t page_nb) {
	ASSERT_SAFE (page_nb > 0);
	ASSERT_SAFE (page_nb < ContainedAllocMaxPages);

	PageBlockHeader * pbh = header.unused.take (page_nb);
	if (pbh) {
		size_t overflow_page_nb = pbh->block_page_nb - page_nb;
		if (overflow_page_nb > 0) {
			// cut overflow, configure and put back in list
			PageBlockHeader * overflow = &pbh[page_nb];
			PageBlockHeader::format (overflow, overflow_page_nb, PageBlockHeader::Type::Unused);
			header.unused.insert (*overflow);
		}
		// configure new alloc
		PageBlockHeader::format (pbh, page_nb, PageBlockHeader::Type::ContainedAlloc);
	}
	return pbh;
}

void SuperpageBlock::free_page_block (PageBlockHeader * pbh) {
	PageBlockHeader * start = pbh;
	PageBlockHeader * end = &pbh[pbh->block_page_nb];
	// Try merge with previous one
	if (header.page_headers <= start - 1 && start[-1].type == PageBlockHeader::Type::Unused) {
		PageBlockHeader * prev = start[-1].head;
		header.unused.remove (*prev);
		start = prev;
	}
	// Try merge with next one
	if (end < &header.page_headers[VMem::SuperpagePageNB] && end[0].type == PageBlockHeader::Type::Unused) {
		PageBlockHeader * next = &end[0];
		header.unused.remove (*next);
		end = &next[next->block_page_nb];
	}
	// Put the merged PB in the list
	PageBlockHeader::format (start, end - start, PageBlockHeader::Type::Unused);
	header.unused.insert (*start);
}

/* IMPL ThreadLocalHeap */

Block ThreadLocalHeap::allocate (size_t size, size_t align) {
	(void) align; // FIXME Alignement unsupported now
	if (size < AllocTypeRange::SmallH) {
		// Small alloc
		// find sizeclass, normalize size
		return {nullptr, 0};
	} else if (size < AllocTypeRange::BigH) {
		// Big alloc
		size_t page_nb = Math::divide_up (size, VMem::PageSize);
		// Try to take from existing superpage blocks
		for (auto & spb : superpage_blocks)
			if (PageBlockHeader * pbh = spb.allocate_page_block (page_nb))
				return spb.get_page_block (pbh);
		// Fail : Take from a new superpage
		SuperpageBlock & spb = SuperpageBlock::createForContainedAlloc (allocator.superpage_tracker, this);
		superpage_blocks.push_back (spb);
		return spb.get_page_block (spb.allocate_page_block (page_nb));
	} else {
		// Huge alloc
		SuperpageBlock & spb = SuperpageBlock::createForHugeAlloc (allocator.superpage_tracker, size, this);
		superpage_blocks.push_back (spb);
		return spb.header.huge_alloc;
	}
}

void ThreadLocalHeap::deallocate (Block blk) {
}

/* IMPL GasAllocator */

Block GasAllocator::allocate (size_t size, size_t align, ThreadLocalHeap * & tls_thread_heap) {
	if (tls_thread_heap == nullptr) {
		// Init TLS variable
		Ptr p = bootstrap_allocator.allocate (sizeof (ThreadLocalHeap), alignof (ThreadLocalHeap)).ptr;
		tls_thread_heap = new (p) ThreadLocalHeap (*this);
		thread_heaps.push_back (*tls_thread_heap);
	}
	return tls_thread_heap->allocate (size, align);
}

void GasAllocator::deallocate (Block blk) {
}

/* Global variable instance */
thread_local ThreadLocalHeap * thread_heap = nullptr;

int main (void) {
	VMem::runtime_asserts ();

	/* Determining program break, placing Givy after (+ some pages in the middle) */
	auto layout = GasLayout (Ptr (sbrk (0)) + 1000 * VMem::PageSize, // start
	                         1000 * VMem::PageSize,                  // space_by_node
	                         4,                                      // nb_node
	                         0);                                     // local node
	GasAllocator allocator (layout);
	return 0;
}
