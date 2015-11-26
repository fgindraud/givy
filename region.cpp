#include <atomic>

#include "alloc_parts.h"
#include "base_defs.h"
#include "utility.h"
#include "superpage_tracker.h"
#include "chain.h"

namespace Givy {
namespace Allocator {

	enum class MemoryType {
		Small,  // Under page size
		Medium, // Between page and superpage size
		Huge,   // Above superpage size
		// Contained Alloc means either Small of Medium

		// Internal definitions
		Unused,  // Unused space, can be allocated
		Reserved // Used internally, not available for allocation
	};

	struct PageBlockHeader;
	struct SuperpageBlock;
	struct ThreadLocalHeap;
	struct MainHeap;
	using PageBlockUnusedList = QuickList<PageBlockHeader, 10>;

	struct PageBlockHeader : public PageBlockUnusedList::Element {
		MemoryType type;
		size_t block_page_nb = 0;
		PageBlockHeader * head = nullptr;

		size_t length (void) noexcept { return block_page_nb; }

		static void format (PageBlockHeader * pbh, size_t size, MemoryType type) {
			for (size_t i = 0; i < size; ++i) {
				pbh[i].type = type;
				pbh[i].block_page_nb = size;
				pbh[i].head = pbh;
			}
		}
	};

	struct SuperpageBlock : public Chain<SuperpageBlock>::Element {
		struct Header {
			std::atomic<ThreadLocalHeap *> owner{nullptr};

			size_t superpage_nb;
			Block huge_alloc;

			PageBlockUnusedList unused;
			PageBlockHeader page_headers[VMem::SuperpagePageNB];
		};

		Header header;

		/* Structure of a superpage
		 * - superpage header, followed by unused space to align to page boundaries
		 * - sequence of pages for small & medium allocations
		 * - last pages may be part of a huge_alloc
		 * - a huge alloc will also span over the next superpages, which have no header (managed by this
		 * superpage)
		 */
		static constexpr size_t HeaderSpacePages = Math::divide_up (sizeof (Header), VMem::PageSize);
		static constexpr size_t ContainedAllocMaxPages = VMem::SuperpagePageNB - HeaderSpacePages;

		/* Creation / destruction */
		SuperpageBlock (size_t superpage_nb, size_t huge_alloc_page_nb, ThreadLocalHeap * owner);

		void destroy_huge_alloc (void);

		/* Owner */
		ThreadLocalHeap * owner (void) const { return header.owner; }
		void disown (void) { header.owner = nullptr; }
		bool adopt (ThreadLocalHeap * adopter) {
			ThreadLocalHeap * expected = nullptr;
			return header.owner.compare_exchange_strong (expected, adopter);
		}

		/* Page blocks */
		Block page_block_memory (PageBlockHeader & pbh);
		PageBlockHeader & page_block_header (Ptr p);

		PageBlockHeader * allocate_page_block (size_t page_nb, MemoryType type);
		void free_page_block (PageBlockHeader & pbh);
	};

	struct MainHeap {
		using BootstrapAllocator = Parts::BackwardBumpPointer;

		GasLayout layout;
		BootstrapAllocator bootstrap_allocator;
		SuperpageTracker<BootstrapAllocator> superpage_tracker;

		Chain<SuperpageBlock> superpage_blocks; // Disowned superpage blocks

		/* Creation / destruction
		 */
		MainHeap (const GasLayout & layout_);
		~MainHeap ();

		/* Superpage management
		 * - creation
		 * - finding header
		 */
		SuperpageBlock & create_superpage_block_huge_alloc (size_t size, ThreadLocalHeap * owner);
		SuperpageBlock & create_superpage_block_contained_alloc (ThreadLocalHeap * owner);
		void destroy_superpage_huge_alloc (SuperpageBlock & spb);
		SuperpageBlock & containing_superpage_block (Ptr inside);
	};

	class ThreadLocalHeap {
	private:
		MainHeap & main_heap;
		Chain<SuperpageBlock> superpage_blocks;

	public:
		/* Constructors and destructors are called on thread creation / destruction due to the use of
		 * ThreadLocalHeap as a
		 * global threal_local variable.
		 */
		ThreadLocalHeap (MainHeap & main_heap_);
		~ThreadLocalHeap ();

		/* Interface
		 */
		Block allocate (size_t size, size_t align);
		void deallocate (Block blk); // Optimized, TODO
		void deallocate (Ptr ptr);

	private:
		void thread_local_deallocate (Ptr ptr, SuperpageBlock & spb);
	};

	namespace Thresholds {
		const size_t Smallest = sizeof (void *); // TODO ?
		const size_t SmallMedium = VMem::PageSize; // TODO
		const size_t MediumHigh = SuperpageBlock::ContainedAllocMaxPages * VMem::PageSize;
	}

	/* IMPL SuperpageBlock */

	SuperpageBlock::SuperpageBlock (size_t superpage_nb, size_t huge_alloc_page_nb,
	                                ThreadLocalHeap * owner) {
		header.superpage_nb = superpage_nb;
		header.owner = owner;

		// Initialize huge alloc
		size_t trailing_huge_alloc_pages = huge_alloc_page_nb % VMem::SuperpagePageNB;
		size_t huge_alloc_first_page = VMem::SuperpagePageNB - trailing_huge_alloc_pages;
		if (huge_alloc_page_nb > 0)
			header.huge_alloc = {Ptr (this).add (huge_alloc_first_page * VMem::PageSize),
			                     huge_alloc_page_nb * VMem::PageSize};

		// PBHs : init chain links
		for (size_t i = 0; i < VMem::SuperpagePageNB; ++i)
			header.page_headers[i].reset ();

		// PBH : init page block info
		PageBlockHeader::format (&header.page_headers[0], HeaderSpacePages, MemoryType::Reserved);
		PageBlockHeader::format (&header.page_headers[HeaderSpacePages],
		                         huge_alloc_first_page - HeaderSpacePages, MemoryType::Unused);
		PageBlockHeader::format (&header.page_headers[huge_alloc_first_page],
		                         VMem::SuperpagePageNB - huge_alloc_first_page, MemoryType::Huge);
		header.unused.insert (header.page_headers[HeaderSpacePages]);
	}

	void SuperpageBlock::destroy_huge_alloc (void) {
		/* Convert huge_alloc part of first superpage to unused space, if there is any.
		 * As the huge alloc trailing page block was formatted to a valid page block (but with Huge
		 * type), we can use free_page_block directly.
		 */
		if (header.page_headers[VMem::SuperpagePageNB - 1].type == MemoryType::Huge)
			free_page_block (page_block_header (header.huge_alloc.ptr));

		// Trim
		header.superpage_nb = 1;
		header.huge_alloc = Block ();
	}

	Block SuperpageBlock::page_block_memory (PageBlockHeader & pbh) {
		size_t pb_index = &pbh - &header.page_headers[0];
		return {Ptr (this) + pb_index * VMem::PageSize, pbh.block_page_nb * VMem::PageSize};
	}

	PageBlockHeader & SuperpageBlock::page_block_header (Ptr p) {
		ASSERT_SAFE (Ptr (this) <= p);
		ASSERT_SAFE (p < Ptr (this) + VMem::SuperpageSize);
		size_t pb_index = (p - Ptr (this)) / VMem::PageSize;
		return *header.page_headers[pb_index].head;
	}

	PageBlockHeader * SuperpageBlock::allocate_page_block (size_t page_nb, MemoryType type) {
		ASSERT_SAFE (page_nb > 0);
		ASSERT_SAFE (page_nb < ContainedAllocMaxPages);

		PageBlockHeader * pbh = header.unused.take (page_nb);
		if (pbh) {
			size_t overflow_page_nb = pbh->block_page_nb - page_nb;
			if (overflow_page_nb > 0) {
				// cut overflow, configure and put back in list
				PageBlockHeader * overflow = &pbh[page_nb];
				PageBlockHeader::format (overflow, overflow_page_nb, MemoryType::Unused);
				header.unused.insert (*overflow);
			}
			// configure new alloc
			PageBlockHeader::format (pbh, page_nb, type);
		}
		return pbh;
	}

	void SuperpageBlock::free_page_block (PageBlockHeader & pbh) {
		PageBlockHeader * start = &pbh;
		PageBlockHeader * end = &start[pbh.block_page_nb];
		// Try merge with previous one
		if (&header.page_headers[0] <= start - 1 && start[-1].type == MemoryType::Unused) {
			PageBlockHeader * prev = start[-1].head;
			header.unused.remove (*prev);
			start = prev;
		}
		// Try merge with next one
		if (end < &header.page_headers[VMem::SuperpagePageNB] && end[0].type == MemoryType::Unused) {
			PageBlockHeader * next = &end[0];
			header.unused.remove (*next);
			end = &next[next->block_page_nb];
		}
		// Put the merged PB in the list
		PageBlockHeader::format (start, end - start, MemoryType::Unused);
		header.unused.insert (*start);
	}

	/* IMPL MainHeap */

	MainHeap::MainHeap (const GasLayout & layout_)
	    : layout (layout_),
	      bootstrap_allocator (layout.start),
	      superpage_tracker (layout, bootstrap_allocator) {
		DEBUG_TEXT ("Allocator created\n");
	}
	MainHeap::~MainHeap () { DEBUG_TEXT ("Allocator destroyed\n"); }

	SuperpageBlock & MainHeap::create_superpage_block_huge_alloc (size_t size,
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

	SuperpageBlock & MainHeap::create_superpage_block_contained_alloc (ThreadLocalHeap * owner) {
		// Reserve, map, configure
		Ptr superpage_block_start = superpage_tracker.acquire (1);
		VMem::map (superpage_block_start, VMem::SuperpageSize);
		return *new (superpage_block_start) SuperpageBlock (1, 0, owner);
	}

	void MainHeap::destroy_superpage_huge_alloc (SuperpageBlock & spb) {
		// Destroy the trailing superpages
		size_t spb_size = spb.header.superpage_nb;
		if (spb_size > 1) {
			Ptr spb_start (&spb);
			superpage_tracker.trim_num (layout.superpage_num (spb_start), spb_size);
			VMem::unmap (spb_start + VMem::SuperpageSize, (spb_size - 1) * VMem::SuperpageSize);
		}

		// Update SPB header
		spb.destroy_huge_alloc ();
	}

	SuperpageBlock & MainHeap::containing_superpage_block (Ptr inside) {
		return *superpage_tracker.get_block_start (inside).as<SuperpageBlock *> ();
	}

	/* IMPL ThreadLocalHeap */

	ThreadLocalHeap::ThreadLocalHeap (MainHeap & main_heap_) : main_heap (main_heap_) {
		DEBUG_TEXT ("TLH created = %p\n", this);
	}
	ThreadLocalHeap::~ThreadLocalHeap () {
		DEBUG_TEXT ("TLH destroyed = %p\n", this);

		// Disown pages
		// TODO put in list ?
		for (auto & spb : superpage_blocks)
			spb.disown ();
	}

	Block ThreadLocalHeap::allocate (size_t size, size_t align) {
		(void) align; // FIXME Alignement unsupported now
		if (size < Thresholds::SmallMedium) {
			// Small alloc
			// TODO
			// find sizeclass, normalize size
			return {nullptr, 0};
		} else if (size < Thresholds::MediumHigh) {
			// Big alloc
			size_t page_nb = Math::divide_up (size, VMem::PageSize);
			// Try to take from existing superpage blocks
			for (auto & spb : superpage_blocks)
				if (PageBlockHeader * pbh = spb.allocate_page_block (page_nb, MemoryType::Medium)) {
					// TODO opt : move spb to top of list
					return spb.page_block_memory (*pbh);
				}
			// If failed : Take from a new superpage
			SuperpageBlock & spb = main_heap.create_superpage_block_contained_alloc (this);
			superpage_blocks.push_back (spb);

			PageBlockHeader * pbh = spb.allocate_page_block (page_nb, MemoryType::Medium);
			ASSERT_STD (pbh != nullptr);
			return spb.page_block_memory (*pbh);
		} else {
			// Huge alloc
			SuperpageBlock & spb = main_heap.create_superpage_block_huge_alloc (size, this);
			superpage_blocks.push_back (spb);
			return spb.header.huge_alloc;
		}
	}

	void ThreadLocalHeap::deallocate (Ptr ptr) {
		if (!main_heap.layout.in_local_area (ptr))
			return; // TODO node_remote free

		SuperpageBlock & spb = main_heap.containing_superpage_block (ptr);
		ThreadLocalHeap * owner = spb.owner ();

		/* Adopt orphan superpage block.
		 * If it was orphan but adoption fails, it means another TLH adopted it, and we fall into the
		 * thread remote deallocation case.
		 */
		if (owner == nullptr && spb.adopt (this)) {
			// TODO add to TLH list ?
		}

		if (owner == this) {
			// Thread local deallocation
			thread_local_deallocate (ptr, spb);
		} else {
			// Node local Remote Thread deallocation
			// TODO
		}
	}

	void ThreadLocalHeap::thread_local_deallocate (Ptr ptr, SuperpageBlock & spb) {
		if (spb.header.huge_alloc.contains (ptr)) {
			// Huge alloc
			main_heap.destroy_superpage_huge_alloc (spb);
		} else {
			PageBlockHeader & pbh = spb.page_block_header (ptr);
			if (pbh.type == MemoryType::Small) {
				// Small alloc
			} else if (pbh.type == MemoryType::Medium) {
				// Medium alloc
				spb.free_page_block (pbh);
			} else {
				// Unreachable
			}
		}
	}
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
		MainHeap main_heap{init ()};
		thread_local ThreadLocalHeap thread_heap{main_heap};

		/* Interface
		 */
		Block allocate (size_t size, size_t align) { return thread_heap.allocate (size, align); }
		void deallocate (Ptr ptr) { thread_heap.deallocate (ptr); }
	}
}
}

#include <iostream>

int main (void) {
	namespace G = Givy::Allocator::GlobalInstance;

	auto p = G::allocate (0xF356, 1);
	std::cout << p.ptr.as<void*> () << " " << p.size << std::endl;
	*p.ptr.as<int *> () = 42;
	G::deallocate (p.ptr);
	return 0;
}
