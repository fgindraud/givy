#ifndef ALLOCATOR_H
#define ALLOCATOR_H
// no inlined

#include <atomic>
#include <algorithm>

#include "reporting.h"
#include "array.h"
#include "types.h"
#include "intrusive_list.h"
#include "superpage_tracker.h"
#include "allocator_bootstrap.h"
#include "allocator_defs.h"
#include "allocator_page_block_manager.h"

namespace Givy {
namespace Allocator {

	/* ---------------------------- Type declaration ------------------------------ */

	/* Forward declaration of types.
	 * This is used to feed the typedef for templatized data structures (lists from intrusive-list.h)
	 */
	struct UnusedBlock;
	struct PageBlockHeader;
	class SuperpageBlock;
	struct MainHeap;
	class ThreadLocalHeap;

	/* Lists types typedef
	 */
	using BlockFreeList = Intrusive::ForwardList<UnusedBlock>;
	using ThreadRemoteFreeList = BlockFreeList::Atomic;
	using PageBlockUnusedList = Intrusive::QuickList<PageBlockHeader, 10>;
	using SuperpageBlockOwnedList = Intrusive::List<SuperpageBlock>;

	class UnusedBlock : public BlockFreeList::Element {
		/* This type represents a block of memory that is unused by the user.
		 * Its main use is to link unused blocks in a BlockFreeList.
		 *
		 * It can optionally store a SuperpageBlock pointer ; this is used to prevent reaccessing the
		 * superpage_tracker during remote frees as an optimization.
		 */
	private:
		SuperpageBlock * const spb_ptr{nullptr};

	public:
		UnusedBlock () = default; // No associated SuperpageBlock
		explicit UnusedBlock (SuperpageBlock & spb_) : spb_ptr (&spb_) {}

		SuperpageBlock & spb (void) const {
			ASSERT_SAFE (spb_ptr != nullptr);
			return *spb_ptr;
		}
		Ptr ptr (void) const { return Ptr (this); } // Nice raw block ptr access
	};

	namespace Thresholds {
		/* Size constants that delimit the behaviour of the allocator.
		 *
		 * Smallest represents the smallest possible block of allocation.
		 * It cannot be smaller because we need every block to at least fit a UnusedBlock, to avoid
		 * undefined behavior if we thread it in a BlockFreeList.
		 *
		 * SmallMedium is user defined, and is not very constrained.
		 * MediumHigh is constrained by SuperpageBlock structure.
		 */
		constexpr size_t Smallest = Math::round_up_as_power_of_2 (sizeof (UnusedBlock));
		constexpr size_t SmallMedium = Math::round_up_as_power_of_2 (VMem::PageSize);
		// Threshold for MediumHigh is defined later
	}

	namespace SizeClass {
		/* Small allocations are managed by building a set of predefined sizes ("sizeclass").
		 * Each allocation is matched against this set, and the smallest fitting sizeclass is used.
		 * Such allocations will be placed in PageBlocks (sequence of pages), which are cut in equally
		 * sized blocks of size of the sizeclass.
		 *
		 * Currently, sizeclasses area defined as a power of 2 scale.
		 */

		// Determine the number of sizeclasses.
		constexpr size_t min_sizeclass_log = Math::log_2_sup (Thresholds::Smallest);
		constexpr size_t max_sizeclass_log = Math::log_2_sup (Thresholds::SmallMedium);
		constexpr size_t nb_sizeclass = (max_sizeclass_log - min_sizeclass_log) + 1;

		/* Sizeclass Id is choosen as the log2 of the associated sizeclass, shifted so that the smallest
		 * sizeclass has id 0.
		 */
		using Id = BoundUint<max_sizeclass_log>;
		constexpr Id id (size_t size) {
			ASSERT_SAFE (size >= Thresholds::Smallest);
			return Math::log_2_sup (size) - min_sizeclass_log;
		}

		/* Sizeclass configuration (precomputed at compile time).
		 */
		struct Info {
			size_t block_size;      // Size of sizeclass
			size_t page_block_size; // Size of PageBlocks of this sizeclass
			size_t nb_blocks;       // Total number of blocks than can fit in a PageBlock
			Id sc_id;               // sizeclass id
		};

		constexpr Info make_info (size_t nth_sizeclass) {
			// TODO more page blocks on bigger sizeclasses
			size_t bs = size_t (1) << (nth_sizeclass + min_sizeclass_log);
			return {bs, 1, VMem::PageSize / bs, Id (nth_sizeclass)};
		}

		constexpr auto config = static_array_from_generator<nb_sizeclass> (make_info);

		static constexpr size_t get_nb_blocks (const Info & info) { return info.nb_blocks; }
		constexpr size_t max_nb_blocks = static_array_max (static_array_map (config, get_nb_blocks));

		/* Sizeclass specific page block lists.
		 * All active (non empty & not full) Small page blocks are threaded into their corresponding
		 * sizeclass list.
		 */
		struct ActivePageBlockListTag;
		using ActivePageBlockList = Intrusive::List<PageBlockHeader, ActivePageBlockListTag>;

#ifdef ASSERT_SAFE_ENABLED
		void print (void);
#endif
	};

	struct PageBlockHeader : public PageBlockUnusedList::Element,
	                         public SizeClass::ActivePageBlockList::Element {
		/* PageBlockHeader is used to represent Page blocks.
		 * Page blocks are the unit of memory in a SuperpageBlock.
		 * Page blocks have a type that represents the use of its memory.
		 *
		 * Every page has an associated PageBlockHeader, but only the first PageBlockHeader of a page
		 * block is actually used.
		 * All other PageBlockHeader of a page block will redirect to the first one with the head
		 * pointer.
		 *
		 * Any active PageBlockHeader of type Unused must be in the PageBlockUnusedList of its
		 * SuperpageBlock.
		 *
		 * An active PageBlockHeader of type Small is the only kind of PageBlockHeader that can be in a
		 * SizeClass::ActivePageBlockList.
		 * It will be in the list of its sizeclass if and only if it is neither full nor empty.
		 *
		 * Unused page blocks which are neighbours will be merged.
		 */
	public:
		/* Represent page block layout ; will be set by format()
		 */
		MemoryType type;                          // Type of page block
		BoundUint<VMem::SuperpagePageNB> nb_page; // Size of page block
		PageBlockHeader * head; // pointer to active header representing the page block

		SizeClass::Id sb_sizeclass;
		BoundUint<SizeClass::max_nb_blocks> sb_nb_carved;
		BoundUint<SizeClass::max_nb_blocks> sb_nb_unused;
		BlockFreeList sb_unused;

	public:
		Ptr page_block (void) const;

		size_t size (void) const { return nb_page; }
		void format (MemoryType type_, size_t pb_size, PageBlockHeader * head_);

		// Small blocks
		size_t available_small_blocks (const SizeClass::Info & info) const;
		void configure_small_blocks (const SizeClass::Info & info);

		Ptr take_small_block (const SizeClass::Info & info);
		void put_small_block (Ptr p, const SizeClass::Info & info);

#ifdef ASSERT_SAFE_ENABLED
		void print (void) const;
#endif
	};

	class SuperpageBlock : public SuperpageBlockOwnedList::Element {
		/* Superpage block (SPB) is the basic unit of memory allocation, and are always aligned to
		 * SuperpageSize.
		 * Superpage blocks are sequence of Superpages (size is configurable).
		 * The SuperpageBlock class data is put at each start of Superpage block, and holds metadata for
		 * the entire SPB.
		 *
		 * The first superpage of each SPB can be split into PageBlocks.
		 * All other superpages, if present, form a unique huge alloc.
		 *
		 * Memory structure of a superpage block :
		 * - superpage header (this class), followed by unused space to align to page boundaries,
		 * - sequence of pages for small & medium allocations,
		 * - last pages may be part of a huge_alloc,
		 * - a huge alloc will also span over the next superpages, which have no header (managed by this
		 * superpage).
		 *
		 * In other words :
		 * - a huge alloc SPB spans multiple superpages ; the last pages of the first superpage may be
		 * part of the huge alloc.
		 * - a non huge alloc SPB has one superpage that is completely dedicated to small/medium allocs
		 * (except for the reserved part).
		 * - a huge alloc SPB shrinks to a non huge page SPB if the huge alloc is destroyed.
		 *
		 * An empty SPB is always destroyed.
		 *
		 * ThreadLocalHeap lists:
		 * - SPB owned by a ThreadLocalHeap are always in the owned_superpage_blocks list
		 */
	private:
		/* Superpage block header.
		 *
		 * huge_alloc_pb_index indicates at which page index starts the huge alloc.
		 * If superpage_nb == 1 it must be equal to VMem::SuperpagePageNB.
		 * If equal to VMem::SuperpagePageNB and superpage_nb > 1, it means no pages in the first
		 * superpage are part of the huge alloc.
		 */
		std::atomic<ThreadLocalHeap *> owner;
		size_t superpage_nb;

		size_t huge_alloc_pb_index;
		PageBlockUnusedList unused;
		PageBlockHeader pbh_table[VMem::SuperpagePageNB];

	public:
		/* SuperpageBlock structure offsets */
		static const size_t HeaderSpacePages;
		static const size_t AvailablePages;

		/* Creation / destruction */
		SuperpageBlock (size_t superpage_nb_, size_t huge_alloc_page_nb, ThreadLocalHeap * owner_);
		~SuperpageBlock ();

		/* info */
		size_t size (void) const { return superpage_nb; }
		Ptr ptr (void) const { return Ptr (this); }
		bool completely_unused (void) const; // including huge alloc

		static SuperpageBlock & from_pointer_in_first_superpage (Ptr inside);
		static SuperpageBlock & from_pbh (PageBlockHeader & pbh);
		static const SuperpageBlock & from_pbh (const PageBlockHeader & pbh);

		/* Huge alloc management */
		bool in_huge_alloc (Ptr p) const;
		Block huge_alloc_memory (void) const;
		void destroy_huge_alloc (void);

		/* Page block */
		PageBlockHeader * allocate_page_block (size_t page_nb, MemoryType type);
		void free_page_block (PageBlockHeader & pbh);

		size_t page_block_index (const PageBlockHeader & pbh) const;
		Ptr page_block_ptr (const PageBlockHeader & pbh) const;
		Block page_block_memory (const PageBlockHeader & pbh) const;

		PageBlockHeader & page_block_header (size_t pb_index);
		PageBlockHeader & page_block_header (Ptr p);
		bool all_page_blocks_unused (void) const; // excluding Huge & Reserved
		size_t available_pb_index (void) const;

		/* Owner */
		ThreadLocalHeap * get_owner (void) const;
		void disown (void);
		bool adopt (ThreadLocalHeap * adopter);

	private:
		/* Page block header formatting */
		void format_pbh (PageBlockHeader * from, PageBlockHeader * to, MemoryType type);
		void format_pbh (PageBlockHeader * from, size_t size, MemoryType type);
		void format_pbh (size_t from, size_t to, MemoryType type);

	public:
#ifdef ASSERT_SAFE_ENABLED
		void print (void) const;
#endif
	};

	/* Define class constant out of class, due to use of sizeof() on class SuperpageBlock (which is
	 * invalid inside SuperpageBlock definition as it is not a complete type yet).
	 */
	const size_t SuperpageBlock::HeaderSpacePages =
	    Math::divide_up (sizeof (SuperpageBlock), VMem::PageSize);
	const size_t SuperpageBlock::AvailablePages =
	    VMem::SuperpagePageNB - SuperpageBlock::HeaderSpacePages;

	namespace Thresholds {
		// Complete definition of Thresholds.
		constexpr size_t MediumHigh = SuperpageBlock::AvailablePages * VMem::PageSize;
	}

	struct MainHeap {
		/* Central heap class.
		 * Mainly hosts the superpage_tracker, and is responsible for SPB creation and destruction.
		 */
		const GasLayout layout;
		Bootstrap bootstrap_allocator;
		SuperpageTracker<Bootstrap> superpage_tracker;

		/* Creation / destruction
		 */
		explicit MainHeap (const GasLayout & layout_);
		~MainHeap ();

		/* SuperpageBlock management
		 * - creation / destruction
		 * - finding header from any part of superpage block (requires superpage tracker)
		 */
		SuperpageBlock & create_superpage_block (ThreadLocalHeap * owner, size_t huge_alloc_size);
		void destroy_superpage_block (SuperpageBlock & spb);
		void destroy_superpage_huge_alloc (SuperpageBlock & spb);
		SuperpageBlock & containing_superpage_block (Ptr inside) const;

#ifdef ASSERT_SAFE_ENABLED
		void print (void) const;
#endif
	};

	class ThreadLocalHeap {
		/* Thread (almost) private heap.
		 * This class designed to be used as a threal_local variable.
		 * One instance should be created for each thread, and destroyed when not needed anymore (thread
		 * termination is a good time for that).
		 *
		 * It maintains a list of all privately owned SuperpageBlocks, that will be pushed to the
		 * MainHeap when the thread dies.
		 *
		 * Thread remote frees are managed by pushing them on the remote_freed_blocks list of the owner
		 * ThreadLocalHeap.
		 */
	private:
		MainHeap & main_heap;
		SuperpageBlockOwnedList owned_superpage_blocks;
		ThreadRemoteFreeList remote_freed_blocks;
		SizeClass::ActivePageBlockList active_small_page_blocks[SizeClass::nb_sizeclass];

	public:
		/* Constructors and destructors are called on thread creation / destruction due to the use of
		 * ThreadLocalHeap as a global threal_local variable.
		 * This class is not copy-able.
		 */
		explicit ThreadLocalHeap (MainHeap & main_heap_);
		~ThreadLocalHeap ();

		/* Allocation interface.
		 *
		 * deallocate supports pointers inside the allocation.
		 */
		Block allocate (size_t size, size_t align);
		void deallocate (Ptr ptr);
		void deallocate (Block blk);

	private:
		SuperpageBlock & create_superpage_block (size_t huge_alloc_size = 0);
		void destroy_superpage_block (SuperpageBlock & spb);
		PageBlockHeader & create_page_block (size_t nb_page, MemoryType type);
		void destroy_page_block (PageBlockHeader & pbh, SuperpageBlock & spb);
		Block allocate_small_block (size_t size);
		void destroy_small_block (Ptr ptr, PageBlockHeader & pbh, SuperpageBlock & spb);

		void thread_local_deallocate (Ptr ptr, SuperpageBlock & spb);
		void process_thread_remote_frees (void);

	public:
#ifdef ASSERT_SAFE_ENABLED
		void print (bool print_main_heap) const;
#endif
	};

	/* ---------------------------- SizeClass IMPL -------------------------------- */

	namespace SizeClass {
#ifdef ASSERT_SAFE_ENABLED
		void print (void) {
			printf ("SizeClass config (max_nb_blocks = %zu):\n", max_nb_blocks);
			for (auto & info : config)
				printf ("[%zu] bs=%zu, pb_size=%zu, nb_block=%zu\n", size_t (info.sc_id), info.block_size,
				        info.page_block_size, info.nb_blocks);
		}
#endif
	}

	/* ---------------------------- PageBlockHeader IMPL -------------------------- */

	Ptr PageBlockHeader::page_block (void) const {
		return SuperpageBlock::from_pbh (*this).page_block_ptr (*this);
	}

	void PageBlockHeader::format (MemoryType type_, size_t pb_size, PageBlockHeader * head_) {
		type = type_;
		nb_page = pb_size;
		head = head_;
	}

	size_t PageBlockHeader::available_small_blocks (const SizeClass::Info & info) const {
		return sb_nb_unused + (info.nb_blocks - sb_nb_carved);
	}

	void PageBlockHeader::configure_small_blocks (const SizeClass::Info & info) {
		sb_sizeclass = info.sc_id;
		sb_nb_carved = 0;
		sb_nb_unused = 0;
		sb_unused.clear ();
	}

	Ptr PageBlockHeader::take_small_block (const SizeClass::Info & info) {
		ASSERT_SAFE (available_small_blocks (info) > 0);
		if (!sb_unused.empty ()) {
			// Reuse unused block
			Ptr p = sb_unused.front ().ptr ();
			sb_unused.pop_front ();
			sb_nb_unused--;
			return p;
		} else {
			// Carve new block
			Ptr p = page_block () + info.block_size * sb_nb_carved;
			sb_nb_carved++;
			return p;
		}
	}

	void PageBlockHeader::put_small_block (Ptr p, const SizeClass::Info & info) {
		ASSERT_SAFE (page_block () <= p);
		ASSERT_SAFE (p < page_block () + VMem::PageSize * size ());

		/* Thread block in freelist ; align ptr to block boundary in case p is not at block_start.
		 */
		UnusedBlock * blk = new (p.align (info.block_size)) UnusedBlock;
		sb_unused.push_front (*blk);
		sb_nb_unused++;
	}

#ifdef ASSERT_SAFE_ENABLED
	void PageBlockHeader::print (void) const {
		if (type == MemoryType::Small) {
			auto & info = SizeClass::config[sb_sizeclass];
			printf ("Small [S=%zu,sc=%zu,bs=%zu,cvd=%zu/%zu,un=%zu]\n", size (), size_t (sb_sizeclass),
			        info.block_size, size_t (sb_nb_carved), info.nb_blocks, size_t (sb_nb_unused));
		} else if (type == MemoryType::Medium) {
			printf ("Medium [S=%zu]\n", size ());
		} else if (type == MemoryType::Huge) {
			printf ("Huge (start) [S=%zu]\n", size ());
		} else if (type == MemoryType::Unused) {
			printf ("Unused [S=%zu]\n", size ());
		} else if (type == MemoryType::Reserved) {
			printf ("Reserved [S=%zu]\n", size ());
		} else {
			printf ("<error> [Type=%ld]\n", static_cast<long> (type));
		}
	}
#endif

	/* ---------------------------- SuperpageBlock IMPL --------------------------- */

	SuperpageBlock::SuperpageBlock (size_t superpage_nb_, size_t huge_alloc_page_nb,
	                                ThreadLocalHeap * owner_)
	    : owner (owner_), superpage_nb (superpage_nb_) {
		DEBUG_TEXT ("[%p]SuperpageBlock(%zu)\n", this, size ());

		// Compute huge page limit in first superpage
		ASSERT_SAFE (superpage_nb * VMem::SuperpagePageNB >= huge_alloc_page_nb + HeaderSpacePages);
		huge_alloc_pb_index = superpage_nb * VMem::SuperpagePageNB - huge_alloc_page_nb;

		// Setup initial page blocks
		size_t max_available_pb = available_pb_index ();
		format_pbh (size_t (0), HeaderSpacePages, MemoryType::Reserved);
		format_pbh (HeaderSpacePages, max_available_pb, MemoryType::Unused);
		format_pbh (max_available_pb, VMem::SuperpagePageNB, MemoryType::Huge);
		unused.insert (pbh_table[HeaderSpacePages]); // Add initial unused page block to list
	}

	SuperpageBlock::~SuperpageBlock () {
#ifdef ASSERT_SAFE_ENABLED
		/* TODO for now, in SAFE build, allocator termination is an error if some blocks have not been
		 * deallocated.
		 */
		ASSERT_SAFE (all_page_blocks_unused ());
		unused.take (1); // make sure to empty unused list
#endif
		DEBUG_TEXT ("[%p]~SuperpageBlock()\n", this);
	}

	bool SuperpageBlock::completely_unused (void) const {
		return size () == 1 && all_page_blocks_unused ();
	}

	SuperpageBlock & SuperpageBlock::from_pointer_in_first_superpage (Ptr inside) {
		// Just use alignement
		return *inside.align (VMem::SuperpageSize).as<SuperpageBlock *> ();
	}
	SuperpageBlock & SuperpageBlock::from_pbh (PageBlockHeader & pbh) {
		// Pbh are stored in first superpage
		return from_pointer_in_first_superpage (Ptr (&pbh));
	}
	const SuperpageBlock & SuperpageBlock::from_pbh (const PageBlockHeader & pbh) {
		// Pbh are stored in first superpage
		return from_pointer_in_first_superpage (Ptr (&pbh));
	}

	/* Huge Alloc */

	bool SuperpageBlock::in_huge_alloc (Ptr p) const {
		ASSERT_SAFE (ptr () <= p);
		ASSERT_SAFE (p < ptr () + size () * VMem::SuperpageSize);
		return ptr () + huge_alloc_pb_index * VMem::PageSize <= p;
	}

	Block SuperpageBlock::huge_alloc_memory (void) const {
		ASSERT_SAFE (superpage_nb > 1);
		size_t huge_alloc_page_nb = superpage_nb * VMem::SuperpagePageNB - huge_alloc_pb_index;
		return {ptr () + huge_alloc_pb_index * VMem::PageSize, huge_alloc_page_nb * VMem::PageSize};
	}

	void SuperpageBlock::destroy_huge_alloc (void) {
		/* Convert huge_alloc part of first superpage to unused space, if there is any.
		 * As the huge alloc trailing page block was formatted to a valid page block (but with Huge
		 * type), we can use free_page_block directly.
		 */
		if (huge_alloc_pb_index < VMem::SuperpagePageNB)
			free_page_block (pbh_table[huge_alloc_pb_index]);

		// Trim
		superpage_nb = 1;
	}

	/* Page block */

	PageBlockHeader * SuperpageBlock::allocate_page_block (size_t page_nb, MemoryType type) {
		ASSERT_SAFE (page_nb > 0);
		ASSERT_SAFE (page_nb < AvailablePages);

		PageBlockHeader * pbh = unused.take (page_nb);
		if (pbh) {
			size_t overflow_page_nb = pbh->size () - page_nb;
			if (overflow_page_nb > 0) {
				// cut overflow, configure and put back in list
				PageBlockHeader * overflow = pbh + page_nb;
				format_pbh (overflow, overflow_page_nb, MemoryType::Unused);
				unused.insert (*overflow);
			}
			// configure new alloc
			format_pbh (pbh, page_nb, type);
		}
		return pbh;
	}

	void SuperpageBlock::free_page_block (PageBlockHeader & pbh) {
		PageBlockHeader * start = &pbh;
		PageBlockHeader * end = start + pbh.size ();
		PageBlockHeader * const table_start = pbh_table;
		PageBlockHeader * const table_end = pbh_table + VMem::SuperpagePageNB;
		// Try merge with previous one
		if (start > table_start && start[-1].type == MemoryType::Unused) {
			PageBlockHeader * prev = start[-1].head;
			unused.remove (*prev);
			start = prev;
		}
		// Try merge with next one
		if (end < table_end && end->type == MemoryType::Unused) {
			PageBlockHeader * next = end;
			unused.remove (*next);
			end = next + next->size ();
		}
		// Put the merged PB in the list
		format_pbh (start, end, MemoryType::Unused);
		unused.insert (*start);
	}

	size_t SuperpageBlock::page_block_index (const PageBlockHeader & pbh) const {
		return array_index (pbh, pbh_table);
	}

	Ptr SuperpageBlock::page_block_ptr (const PageBlockHeader & pbh) const {
		return ptr () + page_block_index (pbh) * VMem::PageSize;
	}

	Block SuperpageBlock::page_block_memory (const PageBlockHeader & pbh) const {
		return {page_block_ptr (pbh), pbh.size () * VMem::PageSize};
	}

	PageBlockHeader & SuperpageBlock::page_block_header (size_t pb_index) {
		return pbh_table[pb_index];
	}

	PageBlockHeader & SuperpageBlock::page_block_header (Ptr p) {
		ASSERT_SAFE (ptr () <= p);
		ASSERT_SAFE (p < ptr () + VMem::SuperpageSize);
		size_t pb_index = (p - ptr ()) / VMem::PageSize;
		return *page_block_header (pb_index).head;
	}

	bool SuperpageBlock::all_page_blocks_unused (void) const {
		// Test if unused quicklist contains every page (except Reserved and Huge ones)
		return unused.size () == available_pb_index () - HeaderSpacePages;
	}

	size_t SuperpageBlock::available_pb_index (void) const {
		/* huge_alloc_pb_index can be above SuperpagePageNB, if a huge alloc is smaller than a
		 * superpage.
		 * available_pb_index () gives the max pb_index that can be used.
		 */
		return std::min (huge_alloc_pb_index, VMem::SuperpagePageNB);
	}

	/* Ownership */

	ThreadLocalHeap * SuperpageBlock::get_owner (void) const {
		return owner.load (std::memory_order_acquire);
	}

	void SuperpageBlock::disown (void) { owner.store (nullptr, std::memory_order_release); }

	bool SuperpageBlock::adopt (ThreadLocalHeap * adopter) {
		ThreadLocalHeap * expected = nullptr;
		return owner.compare_exchange_strong (expected, adopter, std::memory_order_acq_rel,
		                                      std::memory_order_relaxed);
	}

	/* Private */

	void SuperpageBlock::format_pbh (PageBlockHeader * from, PageBlockHeader * to, MemoryType type) {
		size_t s = to - from;
		for (auto pbh = from; pbh < to; ++pbh)
			pbh->format (type, s, from);
	}
	void SuperpageBlock::format_pbh (PageBlockHeader * from, size_t size, MemoryType type) {
		format_pbh (from, from + size, type);
	}
	void SuperpageBlock::format_pbh (size_t from, size_t to, MemoryType type) {
		format_pbh (pbh_table + from, pbh_table + to, type);
	}

#ifdef ASSERT_SAFE_ENABLED
	void SuperpageBlock::print (void) const {
		printf ("S=%zu, P=%p", size (), ptr ().as<void *> ());
		if (size () > 1)
			printf (" (huge alloc=%zu pages)", VMem::SuperpagePageNB * size () - huge_alloc_pb_index);
		printf ("\n");

		for (size_t i = 0; i < VMem::SuperpagePageNB; i += pbh_table[i].size ()) {
			printf ("\t[%zu-%zu]", i, i + pbh_table[i].size ());
			pbh_table[i].print ();
		}
	}
#endif

	/* ---------------------------- MainHeap IMPL --------------------------------- */

	MainHeap::MainHeap (const GasLayout & layout_)
	    : layout (layout_),
	      bootstrap_allocator (layout.start),
	      superpage_tracker (layout, bootstrap_allocator) {
		DEBUG_TEXT ("MainHeap()\n");
	}
	MainHeap::~MainHeap () { DEBUG_TEXT ("~MainHeap()\n"); }

	SuperpageBlock & MainHeap::create_superpage_block (ThreadLocalHeap * owner,
	                                                   size_t huge_alloc_size) {
		/* Compute sizes
		 * If huge_alloc_size is 0, allocates just one superpage
		 */
		size_t huge_alloc_page_nb = Math::divide_up (huge_alloc_size, VMem::PageSize);
		size_t superpage_nb = Math::divide_up (huge_alloc_page_nb + SuperpageBlock::HeaderSpacePages,
		                                       VMem::SuperpagePageNB);
		// Reserve, map, configure
		Ptr spb_start = layout.superpage (superpage_tracker.acquire (superpage_nb));
		VMem::map_checked (spb_start, superpage_nb * VMem::SuperpageSize);
		return *new (spb_start) SuperpageBlock (superpage_nb, huge_alloc_page_nb, owner);
	}

	void MainHeap::destroy_superpage_block (SuperpageBlock & spb) {
		Ptr spb_start = spb.ptr ();
		spb.~SuperpageBlock (); // manual call due to placement new construction
		superpage_tracker.release (layout.superpage_num (spb_start), spb.size ());
		VMem::unmap_checked (spb_start, spb.size () * VMem::SuperpageSize);
	}

	void MainHeap::destroy_superpage_huge_alloc (SuperpageBlock & spb) {
		DEBUG_TEXT ("[%p] SuperpageBlock trim (%zu->1)\n", spb.ptr ().as<void *> (), spb.size ());
		// Destroy the trailing superpages
		size_t spb_size = spb.size ();
		ASSERT_STD (spb_size > 1);
		Ptr spb_start = spb.ptr ();
		superpage_tracker.trim (layout.superpage_num (spb_start), spb_size);
		VMem::unmap_checked (spb_start + VMem::SuperpageSize, (spb_size - 1) * VMem::SuperpageSize);

		// Update SPB header
		spb.destroy_huge_alloc ();
	}

	SuperpageBlock & MainHeap::containing_superpage_block (Ptr inside) const {
		return *superpage_tracker.get_block_start (inside).as<SuperpageBlock *> ();
	}

#ifdef ASSERT_SAFE_ENABLED
	void MainHeap::print (void) const {
		printf ("Layout:\n");
		printf ("\tnodes (local node): %zu (%zu)\n", layout.nb_node, layout.local_node);
		printf ("\tsuperpage by node (total): %zu (%zu)\n", layout.superpage_by_node,
		        layout.superpage_total);
		printf ("\tnode area limits (sp index): [0");
		for (auto n : range (layout.nb_node))
			printf (",%zu", layout.node_area_end_superpage_num (n));
		printf ("]\n");

		printf ("SuperpageTracker:\n");
		superpage_tracker.print ();
	}
#endif

	/* ---------------------------- ThreadLocalHeap IMPL -------------------------- */

	ThreadLocalHeap::ThreadLocalHeap (MainHeap & main_heap_) : main_heap (main_heap_) {
		DEBUG_TEXT ("[%p]ThreadLocalHeap()\n", this);
	}

	ThreadLocalHeap::~ThreadLocalHeap () {
		DEBUG_TEXT ("[%p]~ThreadLocalHeap()\n", this);

		process_thread_remote_frees ();

		// Disown pages to let them be picked up by another ThreadLocalHeap
		while (!owned_superpage_blocks.empty ()) {
			auto & spb = owned_superpage_blocks.front ();
			owned_superpage_blocks.pop_front ();

			// remove page blocks from active sizeclass list
			for (size_t i = 0; i < VMem::SuperpagePageNB; i += spb.page_block_header (i).size ())
				if (spb.page_block_header (i).type == MemoryType::Small)
					SizeClass::ActivePageBlockList::unlink (spb.page_block_header (i));

			spb.disown ();
		}
	}

	Block ThreadLocalHeap::allocate (size_t size, size_t align) {
		process_thread_remote_frees ();

		/* Alignment support.
		 * Small allocations are aligned to the size of the sizeclass.
		 * All other allocations are aligned to pages.
		 * So we allow a simple support of up to page-size alignement, by using a size of at least
		 * requested alignement to answer the allocate request.
		 */
		ASSERT_STD (align <= VMem::PageSize);
		ASSERT_STD (Math::is_power_of_2 (align));
		size = std::max (size, align);

		if (size < Thresholds::SmallMedium) {
			// Small alloc
			return allocate_small_block (size);
		} else if (size < Thresholds::MediumHigh) {
			// Big alloc
			size_t page_nb = Math::divide_up (size, VMem::PageSize);
			PageBlockHeader & pbh = create_page_block (page_nb, MemoryType::Medium);
			return SuperpageBlock::from_pbh (pbh).page_block_memory (pbh);
		} else {
			// Huge alloc
			return create_superpage_block (size).huge_alloc_memory ();
		}
	}

	void ThreadLocalHeap::deallocate (Ptr ptr) {
		process_thread_remote_frees ();

		if (!main_heap.layout.in_local_area (ptr))
			return; // TODO node_remote free

		SuperpageBlock & spb = main_heap.containing_superpage_block (ptr);
		ThreadLocalHeap * owner = spb.get_owner ();

		/* Adopt orphan superpage block.
		 * If it was orphan but adoption fails, it means another TLH adopted it, and we fall into the
		 * thread remote deallocation case.
		 */
		if (owner == nullptr && spb.adopt (this)) {
			owned_superpage_blocks.push_back (spb);

			// Add active page blocks to sizeclass active lists
			for (size_t i = 0; i < VMem::SuperpagePageNB; i += spb.page_block_header (i).size ()) {
				auto & pbh = spb.page_block_header (i);
				if (pbh.type == MemoryType::Small)
					active_small_page_blocks[pbh.sb_sizeclass].push_back (pbh);
			}

			owner = this;
		}

		if (owner == this) {
			// Thread local deallocation
			thread_local_deallocate (ptr, spb);
		} else {
			// Node local Remote Thread deallocation

			/* Push the block on the remote TLH freelist
			 * Also store the spb reference to avoid accessing the superpage_tracker again.
			 *
			 * All allocations have alignement and size at least those of UnusedBlock.
			 * So all allocations boundaries are on a "grid" made of UnusedBlock.
			 * ptr.align (sizeof (UnusedBlock)) will ensure that the temporary UnusedBlock is not
			 * crossing
			 * any Block boundary, so constructing one is ok.
			 */
			UnusedBlock * blk = new (ptr.align (sizeof (UnusedBlock))) UnusedBlock (spb);
			owner->remote_freed_blocks.push_front (*blk);
		}
	}

	void ThreadLocalHeap::deallocate (Block blk) {
		// TODO optimize for small/medium alloc, no call to SPT
		// DEBUG_TEXT ("free {%p,%zu}\n", blk.ptr.as<void* >(), blk.size);
		deallocate (blk.ptr);
	}

	SuperpageBlock & ThreadLocalHeap::create_superpage_block (size_t huge_alloc_size) {
		SuperpageBlock & spb = main_heap.create_superpage_block (this, huge_alloc_size);
		owned_superpage_blocks.push_back (spb);
		return spb;
	}

	void ThreadLocalHeap::destroy_superpage_block (SuperpageBlock & spb) {
		owned_superpage_blocks.remove (spb);
		main_heap.destroy_superpage_block (spb);
	}

	PageBlockHeader & ThreadLocalHeap::create_page_block (size_t nb_page, MemoryType type) {
		// Try to take from existing superpage blocks
		for (auto & spb : owned_superpage_blocks)
			if (PageBlockHeader * pbh = spb.allocate_page_block (nb_page, type)) {
				return *pbh;
			}
		// If failed : Take from a new superpage
		SuperpageBlock & spb = create_superpage_block ();
		PageBlockHeader * pbh = spb.allocate_page_block (nb_page, type);
		ASSERT_SAFE (pbh != nullptr);
		return *pbh;
	}

	void ThreadLocalHeap::destroy_page_block (PageBlockHeader & pbh, SuperpageBlock & spb) {
		spb.free_page_block (pbh);
		if (spb.completely_unused ())
			destroy_superpage_block (spb);
	}

	Block ThreadLocalHeap::allocate_small_block (size_t size) {
		auto & info = SizeClass::config[SizeClass::id (std::max (size, Thresholds::Smallest))];
		auto & pb_list = active_small_page_blocks[info.sc_id];

		// Create new page block if there is none available.
		if (pb_list.empty ()) {
			auto & new_pbh = create_page_block (info.page_block_size, MemoryType::Small);
			new_pbh.configure_small_blocks (info);
			pb_list.push_front (new_pbh);
		}

		// Pick block from current page block
		auto & pbh = pb_list.front ();
		Ptr p = pbh.take_small_block (info);

		// Remove from list if empty
		if (pbh.available_small_blocks (info) == 0)
			pb_list.pop_front ();

		return {p, info.block_size};
	}

	void ThreadLocalHeap::destroy_small_block (Ptr ptr, PageBlockHeader & pbh, SuperpageBlock & spb) {
		auto & info = SizeClass::config[pbh.sb_sizeclass];

		pbh.put_small_block (ptr, info);

		size_t available_blocks = pbh.available_small_blocks (info);
		if (available_blocks == info.nb_blocks) {
			// Destroy page block if unused
			SizeClass::ActivePageBlockList::unlink (pbh);
			destroy_page_block (pbh, spb);
		} else if (available_blocks == 1) {
			// Was fully used before ; put back in active list
			active_small_page_blocks[info.sc_id].push_front (pbh);
		}
	}

	void ThreadLocalHeap::thread_local_deallocate (Ptr ptr, SuperpageBlock & spb) {
		ASSERT_SAFE (spb.ptr () <= ptr);
		ASSERT_SAFE (ptr < spb.ptr () + spb.size () * VMem::SuperpageSize);
		if (spb.in_huge_alloc (ptr)) {
			// Huge alloc
			if (spb.all_page_blocks_unused ())
				destroy_superpage_block (spb);
			else
				main_heap.destroy_superpage_huge_alloc (spb);
		} else {
			PageBlockHeader & pbh = spb.page_block_header (ptr);
			if (pbh.type == MemoryType::Small) {
				// Small alloc
				destroy_small_block (ptr, pbh, spb);
			} else if (pbh.type == MemoryType::Medium) {
				// Medium alloc
				destroy_page_block (pbh, spb);
			} else {
				// Unreachable
				ASSERT_STD_FAIL ("PageBlockHeader is neither Small nor Medium");
			}
		}
	}

	void ThreadLocalHeap::process_thread_remote_frees (void) {
		BlockFreeList unused_blocks = remote_freed_blocks.take_all ();
		for (auto it = unused_blocks.begin (); it != unused_blocks.end ();) {
			Ptr p = it->ptr ();
			SuperpageBlock & spb = it->spb ();
			it++; // Get next element before destroying the current element
			thread_local_deallocate (p, spb);
		}
	}

#ifdef ASSERT_SAFE_ENABLED
	void ThreadLocalHeap::print (bool print_main_heap) const {

		if (print_main_heap) {
			printf ("====== MainHeap ======\n");
			main_heap.print ();
		}

		printf ("====== ThreadLocalHeap [%p] ======\n", this);
		printf ("Owned SuperpageBlocks:\n");
		for (auto & spb : owned_superpage_blocks) {
			printf ("[%zu]", main_heap.layout.superpage_num (spb.ptr ()));
			spb.print ();
		}

		printf ("SizeClass lists:\n");
		for (size_t i = 0; i < SizeClass::nb_sizeclass; ++i) {
			printf ("[%zu,bs=%zu]", i, SizeClass::config[i].block_size);
			for (auto & pbh : active_small_page_blocks[i]) {
				auto & spb = SuperpageBlock::from_pbh (pbh);
				printf (" (%zu,%zu)", main_heap.layout.superpage_num (spb.ptr ()),
				        spb.page_block_index (pbh));
			}
			printf ("\n");
		}
	}
#endif
}
}

#endif
