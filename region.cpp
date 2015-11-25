#include <atomic>

#include "alloc_parts.h"
#include "base_defs.h"
#include "utility.h"
#include "superpage_tracker.h"
#include "chain.h"

namespace Givy {
namespace Allocator {

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

	struct PageBlockHeader;
	struct SuperpageBlock;
	struct ThreadLocalHeap;
	struct SharedHeap;
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
			for (size_t i = 0; i < size; ++i) pbh[i].reset ();
		}
	};

	struct SuperpageBlock : public Chain<SuperpageBlock>::Element {
		struct Header {
			std::atomic<ThreadLocalHeap *> owner{nullptr};

			size_t superpage_nb;
			Block huge_alloc{nullptr, 0};

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

		/* Owner */
		ThreadLocalHeap * owner (void) const { return header.owner; }
		void disown (void) { header.owner = nullptr; }
		bool adopt (ThreadLocalHeap * adopter) {
			ThreadLocalHeap * expected = nullptr;
			return header.owner.compare_exchange_strong (expected, adopter);
		}

		/* Alloc page blocks */
		Block page_block_memory (PageBlockHeader * pbh);
		PageBlockHeader * allocate_page_block (size_t page_nb);
		void free_page_block (PageBlockHeader * pbh);
	};

	struct SharedHeap {
		using BootstrapAllocator = Parts::BackwardBumpPointer;

		GasLayout layout;
		BootstrapAllocator bootstrap_allocator;
		SuperpageTracker<BootstrapAllocator> superpage_tracker;

		Chain<SuperpageBlock> superpage_blocks; // Disowned superpage blocks

		/* Creation / destruction
		 */
		SharedHeap (const GasLayout & layout_);
		~SharedHeap ();

		/* Create superpages
		 */
		SuperpageBlock & create_superpage_block_huge_alloc (size_t size, ThreadLocalHeap * owner);
		SuperpageBlock & create_superpage_block_contained_alloc (ThreadLocalHeap * owner);
	};

	struct ThreadLocalHeap {
		SharedHeap & shared_heap;
		Chain<SuperpageBlock> superpage_blocks;

		/* Constructors and destructors are called on thread creation / destruction due to the use of
		 * ThreadLocalHeap as a
		 * global threal_local variable.
		 */
		ThreadLocalHeap (SharedHeap & shared_heap_);
		~ThreadLocalHeap ();

		Block allocate (size_t size, size_t align);
		void deallocate (Block blk);
		void deallocate (Ptr ptr);
	};

	/* IMPL SuperpageBlock */

	SuperpageBlock::SuperpageBlock (size_t superpage_nb, size_t huge_alloc_page_nb,
	                                ThreadLocalHeap * owner) {
		header.superpage_nb = superpage_nb;
		header.owner = owner;

		size_t trailing_huge_alloc_pages = huge_alloc_page_nb % VMem::SuperpagePageNB;
		size_t huge_alloc_first_page = VMem::SuperpagePageNB - trailing_huge_alloc_pages;
		if (huge_alloc_page_nb > 0)
			header.huge_alloc = {Ptr (this).add (huge_alloc_first_page * VMem::PageSize),
			                     huge_alloc_page_nb * VMem::PageSize};

		PageBlockHeader::format_unused_list (&header.page_headers[0], VMem::SuperpagePageNB);
		PageBlockHeader::format (&header.page_headers[0], HeaderSpacePages,
		                         PageBlockHeader::Type::Invalid);
		PageBlockHeader::format (&header.page_headers[HeaderSpacePages],
		                         huge_alloc_first_page - HeaderSpacePages,
		                         PageBlockHeader::Type::Unused);
		PageBlockHeader::format (&header.page_headers[huge_alloc_first_page],
		                         VMem::SuperpagePageNB - huge_alloc_first_page,
		                         PageBlockHeader::Type::HugeAlloc);
	}

	Block SuperpageBlock::page_block_memory (PageBlockHeader * pbh) {
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
		if (end < &header.page_headers[VMem::SuperpagePageNB] &&
		    end[0].type == PageBlockHeader::Type::Unused) {
			PageBlockHeader * next = &end[0];
			header.unused.remove (*next);
			end = &next[next->block_page_nb];
		}
		// Put the merged PB in the list
		PageBlockHeader::format (start, end - start, PageBlockHeader::Type::Unused);
		header.unused.insert (*start);
	}

	/* IMPL SharedHeap */

	SharedHeap::SharedHeap (const GasLayout & layout_)
	    : layout (layout_),
	      bootstrap_allocator (layout.start),
	      superpage_tracker (layout, bootstrap_allocator) {
		DEBUG_TEXT ("Allocator created\n");
	}
	SharedHeap::~SharedHeap () { DEBUG_TEXT ("Allocator destroyed\n"); }

	SuperpageBlock & SharedHeap::create_superpage_block_huge_alloc (size_t size,
	                                                                ThreadLocalHeap * owner) {
		// Compute size of allocation
		size_t page_nb = Math::divide_up (size, VMem::PageSize);
		size_t superpage_nb =
		    Math::divide_up (page_nb + SuperpageBlock::HeaderSpacePages, VMem::SuperpagePageNB);
		// Reserve, map, configure
		Ptr superpage_block_start = superpage_tracker.acquire (superpage_nb);
		VMem::map (superpage_block_start, superpage_nb * VMem::SuperpageSize);
		return *new (superpage_block_start) SuperpageBlock (superpage_nb, page_nb, owner);
	}

	SuperpageBlock & SharedHeap::create_superpage_block_contained_alloc (ThreadLocalHeap * owner) {
		// Reserve, map, configure
		Ptr superpage_block_start = superpage_tracker.acquire (1);
		VMem::map (superpage_block_start, VMem::SuperpageSize);
		return *new (superpage_block_start) SuperpageBlock (1, 0, owner);
	}

	/* IMPL ThreadLocalHeap */

	ThreadLocalHeap::ThreadLocalHeap (SharedHeap & shared_heap_) : shared_heap (shared_heap_) {
		DEBUG_TEXT ("TLH created = %p\n", this);
	}
	ThreadLocalHeap::~ThreadLocalHeap () {
		DEBUG_TEXT ("TLH destroyed = %p\n", this);

		// Disown pages
		// TODO put in list ?
		for (auto & spb : superpage_blocks) spb.disown ();
	}

	Block ThreadLocalHeap::allocate (size_t size, size_t align) {
		(void) align; // FIXME Alignement unsupported now
		if (size < AllocTypeRange::SmallH) {
			// Small alloc
			// TODO
			// find sizeclass, normalize size
			return {nullptr, 0};
		} else if (size < AllocTypeRange::BigH) {
			// Big alloc
			size_t page_nb = Math::divide_up (size, VMem::PageSize);
			// Try to take from existing superpage blocks
			for (auto & spb : superpage_blocks)
				if (PageBlockHeader * pbh = spb.allocate_page_block (page_nb))
					return spb.page_block_memory (pbh);
			// Fail : Take from a new superpage
			SuperpageBlock & spb = shared_heap.create_superpage_block_contained_alloc (this);
			superpage_blocks.push_back (spb);
			return spb.page_block_memory (spb.allocate_page_block (page_nb));
		} else {
			// Huge alloc
			SuperpageBlock & spb = shared_heap.create_superpage_block_huge_alloc (size, this);
			superpage_blocks.push_back (spb);
			return spb.header.huge_alloc;
		}
	}

	void ThreadLocalHeap::deallocate (Block blk) {}
	void ThreadLocalHeap::deallocate (Ptr ptr) {}
}
}

namespace Givy {
namespace Allocator {
	namespace GlobalInstance {
		// Called at program startup (before main)
		GasLayout init (void) {
			VMem::runtime_asserts ();

			/* Determining program break, placing Givy after (+ some pages in the middle) */
			return {Ptr (sbrk (0)) + 1000 * VMem::PageSize, // start
			        1000 * VMem::PageSize,                  // space_by_node
			        4,                                      // nb_node
			        0};                                     // local node
		}

		/* Due to c++ thread_local semantics, ThreadLocalHeap objects will be constructed/destructed
		 * at every thread start/end
		 */
		SharedHeap shared_heap{init ()};
		thread_local ThreadLocalHeap thread_heap{shared_heap};

		/* Interface
		 */
		Block allocate (size_t size, size_t align) { return thread_heap.allocate (size, align); }
		void deallocate (Block blk) { thread_heap.deallocate (blk); }
		void deallocate (Ptr ptr) { thread_heap.deallocate (ptr); }
	}
}
}

int main (void) {
	auto p = Givy::Allocator::GlobalInstance::allocate (1, 1);
	return 0;
}
