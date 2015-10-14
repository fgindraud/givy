// DEBUG
#include <cstdio>
#include <cassert>

#include "alloc_parts.h"
#include "base_defs.h"

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

struct GasLayout {
	Ptr start;
	size_t space_by_node;
	int nb_node;

	size_t superpage_by_node;
	size_t superpage_total;

	GasLayout (Ptr start_, size_t space_by_node_, int nb_node_) :
		start (start_),
		space_by_node (space_by_node_),
		nb_node (nb_node_),
		// derived
		superpage_by_node (Math::divide_up (space_by_node, VMem::SuperpageSize)),
		superpage_total (superpage_by_node * nb_node)
	{}

	Ptr superpage (size_t num) const {
		return start + VMem::SuperpageSize * num;
	}
	size_t superpage (Ptr inside) const {
		return inside.sub (start) / VMem::SuperpageSize;
	}
	size_t node_area_start_superpage_num (int node) const {
		return superpage_by_node * node;
	}
	size_t node_area_end_superpage_num (int node) const {
		return node_area_start_superpage_num (node + 1);
	}
};

#include <mutex>
#include <atomic>

struct SuperpageTracker {
	// TODO finer grain of atomics ? right now it is seq_cst
	using IntType = std::uintmax_t;
	using AtomicIntType = std::atomic<IntType>;
	using BitArray = BitMask<IntType>;

	/* mapping_table bits:
	 * 	0: free superpage, not mapped
	 * 	1: used superpage, mapped
	 * sequence_table bits: superpage sequence
	 * 	0: superpages after the first
	 * 	1: first superpage of sequence
	 */
	const GasLayout & layout;
	size_t table_size;
	AtomicIntType * mapping_table;
	AtomicIntType * sequence_table;

	template<typename Alloc>
	SuperpageTracker (const GasLayout & layout_, Alloc & allocator) :
		layout (layout_),
		table_size (Math::divide_up (layout.superpage_total, BitArray::Bits))
	{
		mapping_table = allocator.allocate (table_size * sizeof (AtomicIntType), alignof (AtomicIntType));
		sequence_table = allocator.allocate (table_size * sizeof (AtomicIntType), alignof (AtomicIntType));
		for (size_t it = 0; it < table_size; ++it) {
			new (&mapping_table[it]) AtomicIntType (BitArray::zeros ());
			new (&sequence_table[it]) AtomicIntType (BitArray::zeros ());
		}
	}

	size_t acquire (size_t superpage_nb, int node);
	void release (size_t superpage_num, size_t superpage_nb);

	bool is_mapped (size_t superpage_num);
	bool is_mapped (Ptr ptr) { return is_mapped (layout.superpage (ptr)); }

	struct Index {
		size_t array_idx;
		size_t bit_idx;

		Index (size_t array_idx_, size_t bit_idx_) : array_idx (array_idx_), bit_idx (bit_idx_) {}
		explicit Index (size_t superpage_num_) : Index (superpage_num_ / BitArray::Bits, superpage_num_ % BitArray::Bits) {}
		
		size_t superpage_num (void) const { return array_idx * BitArray::Bits + bit_idx; }
		void next_array_cell (void) { array_idx++; bit_idx = 0; }
		bool operator< (const Index & rhs) const { return array_idx < rhs.array_idx || (array_idx == rhs.array_idx && bit_idx < rhs.bit_idx); }
	};

	bool set_mapping_bits (Index loc_start, IntType expected_start, Index loc_end, IntType expected_end);
	void set_sequence_bits (Index loc_start, Index loc_end);
	bool set_bits (Index loc_start, IntType expected_start, Index loc_end, IntType expected_end);
	void clear_bits (Index loc_start, Index loc_end);
};

bool SuperpageTracker::set_mapping_bits (Index loc_start, IntType expected_start, Index loc_end, IntType expected_end) {
	using std::memory_order::memory_order_seq_cst;

	if (loc_start.array_idx == loc_end.array_idx) {
		// One cell span
		return mapping_table[loc_start.array_idx].compare_exchange_strong (
				expected_start,
				expected_start | BitArray::window_bound (loc_start.bit_idx, loc_end.bit_idx),
				memory_order_seq_cst);
	} else {
		/* Multicell span
		 * Try to set bits (start, then middle, then end), revert to previous state on failure
		 */
		IntType loc_start_bits = BitArray::window_bound (loc_start.bit_idx, BitArray::Bits);
		if (mapping_table[loc_start.array_idx].compare_exchange_strong (expected_start, expected_start | loc_start_bits, memory_order_seq_cst)) {
			size_t idx;
			for (idx = loc_start.array_idx + 1; idx < loc_end.array_idx; ++idx) {
				IntType expected = BitArray::zeros ();
				if (!mapping_table[idx].compare_exchange_strong (expected, BitArray::ones (), memory_order_seq_cst))
					break;
			}
			if (idx == loc_end.array_idx) {
				IntType loc_end_bits = BitArray::window_bound (0, loc_end.bit_idx);
				if (loc_end_bits == BitArray::zeros ()) {
					return true; // Nothing to do, setting bits is already a success
				} else {
					if (mapping_table[loc_end.array_idx].compare_exchange_strong (expected_end, expected_end | loc_end_bits, memory_order_seq_cst))
						return true; // Success too
				}
			}
			// Cleanup on failure
			for (size_t clean_idx = loc_start.array_idx + 1; clean_idx < idx; ++clean_idx)
				mapping_table[clean_idx].store (BitArray::zeros (), memory_order_seq_cst);
			mapping_table[loc_start.array_idx].fetch_and (~loc_start_bits, memory_order_seq_cst);
		}
		return false;
	}
}

void SuperpageTracker::set_sequence_bits (Index loc_start, Index loc_end) {
	using std::memory_order::memory_order_seq_cst;
	// No need to compare_exchange ; we are supposed to own the sequence bits as we reserved the area through mapping bits
	if (loc_start.array_idx == loc_end.array_idx) {
		// One cell span
		IntType bits = BitArray::window_bound (loc_start.bit_idx + 1, loc_end.bit_idx);
		if (bits != BitArray::zeros ())
			sequence_table[loc_start.array_idx].fetch_or (bits, memory_order_seq_cst);
	} else {
		// Multiple cell span
		IntType first_cell_bits = BitArray::window_bound (loc_start.bit_idx, BitArray::Bits);
		IntType last_cell_bits = BitArray::window_bound (0, loc_end.bit_idx);

		sequence_table[loc_start.array_idx].fetch_or (first_cell_bits, memory_order_seq_cst);
		for (size_t i = loc_start.array_idx + 1; i < loc_end.array_idx; i++)
			sequence_table[i].store (BitArray::ones (), memory_order_seq_cst);
		if (last_cell_bits != BitArray::zeros ())
			sequence_table[loc_end.array_idx].fetch_or (last_cell_bits, memory_order_seq_cst);
	}
}

bool SuperpageTracker::set_bits (Index loc_start, IntType expected_start, Index loc_end, IntType expected_end = BitArray::zeros ()) {
	if (set_mapping_bits (loc_start, expected_start, loc_end, expected_end)) {
		// Mapping bits are always set first
		set_sequence_bits (loc_start, loc_end);
		return true;
	} else {
		return false;
	}
}

void SuperpageTracker::clear_bits (Index loc_start, Index loc_end) {
	/* Clears bits to release a superpage sequence.
	 * sequence_table is cleared first, then mapping_table.
	 *
	 * The mapping pattern is used to clear both mapping/sequence bits because sequence bits
	 * are a subset of mapping bits:
	 * mapping:  111..11
	 * sequence: 011..11
	 */
	using std::memory_order::memory_order_seq_cst;
	
	if (loc_start.array_idx == loc_end.array_idx) {
		// One cell span
		IntType bits = BitArray::window_bound (loc_start.bit_idx, loc_end.bit_idx);
		if (loc_end.bit_idx - loc_start.bit_idx > 1)
			sequence_table[loc_start.array_idx].fetch_and (~bits, memory_order_seq_cst);
		mapping_table[loc_start.array_idx].fetch_and (~bits, memory_order_seq_cst);
	} else {
		// Multiple cells span
		IntType first_cell_bits = BitArray::window_bound (loc_start.bit_idx, BitArray::Bits);
		IntType last_cell_bits = BitArray::window_bound (0, loc_end.bit_idx);

		sequence_table[loc_start.array_idx].fetch_and (~first_cell_bits, memory_order_seq_cst);
		for (size_t i = loc_start.array_idx + 1; i < loc_end.array_idx; i++)
			sequence_table[i].store (BitArray::zeros (), memory_order_seq_cst);
		if (last_cell_bits != BitArray::zeros ())
			sequence_table[loc_end.array_idx].fetch_and (~last_cell_bits, memory_order_seq_cst);
		
		mapping_table[loc_start.array_idx].fetch_and (~first_cell_bits, memory_order_seq_cst);
		for (size_t i = loc_start.array_idx + 1; i < loc_end.array_idx; i++)
			mapping_table[i].store (BitArray::zeros (), memory_order_seq_cst);
		if (last_cell_bits != BitArray::zeros ())
			mapping_table[loc_end.array_idx].fetch_and (~last_cell_bits, memory_order_seq_cst);
	}
}

size_t SuperpageTracker::acquire (size_t superpage_nb, int node) {
	/* I need to find a sequence of superpage_nb consecutive 0s anywhere in the table.
	 * For now, I perform a linear search of the table, with some optimisation to prevent
	 * using an atomic load twice on the same integer array cell if possible.
	 *
	 * The linear search will scan the table integer by integer for speed.
	 * Search starts at the start of the node-local virtual addresses segment.
	 * search_at.bit_idx is taken into account if search starts in the middle of a cell.
	 */
	using std::memory_order::memory_order_seq_cst;
	assert (superpage_nb > 0);

	auto search_at = Index (layout.node_area_start_superpage_num (node));
	auto search_end = Index (layout.node_area_end_superpage_num (node));
	IntType c;
	while (search_at < search_end) {
		c = mapping_table[search_at.array_idx].load (memory_order_seq_cst);
continue_no_load:

		if (c == BitArray::ones ()) {
			// Completely full cell, skip
			search_at.next_array_cell ();
			continue;
		}

		size_t pos = BitArray::find_zero_subsequence (c, superpage_nb, search_at.bit_idx);
		if (pos < BitArray::Bits) {
			/* A sequence has been found, and it is contained in one array cell.
			 * I only need one atomic operation to reserve it.
			 *
			 * On bit flip failure, restart the search at the same cell.
			 * It is unlikely that previous bits have been freed, so no need to go back from the start.
			 */
			auto loc_start = Index (search_at.array_idx, pos);
			auto loc_end = Index (search_at.array_idx, pos + superpage_nb);
			if (!set_bits (loc_start, c, loc_end))
				continue;
			return loc_start.superpage_num ();
		}

		// Look for the start of a multicell sequence
		size_t msb_zeros = std::min (BitArray::count_msb_zeros (c), BitArray::Bits - search_at.bit_idx);
		if (msb_zeros > 0) {
			/* A possible start of sequence has been found.
			 * I will search the next cells to see if there is enough consecutive 0s to be a valid sequence.
			 * In general, a valid sequence will look like :
			 * |xxxxx000|00000000|00000000|00xxxxxx|
			 * msb_zeros describes the first cell ; I need to check the middle and last cells.
			 */
			IntType first_cell_expected_value = c;
			auto loc_start = Index (search_at.array_idx, BitArray::Bits - msb_zeros);
			auto loc_end = Index (loc_start.superpage_num () + superpage_nb);
			IntType last_cell_bits = BitArray::window_bound (0, loc_end.bit_idx);
			if (!(loc_end < search_end))
				break;
			for (size_t idx = loc_start.array_idx + 1; idx < loc_end.array_idx ; ++idx) {
				c = mapping_table[idx].load (memory_order_seq_cst);
				if (c != BitArray::zeros ()) {
					// Zero sequence is not big enough ; restart search from this cell, no need to reload
					search_at = Index (idx, 0);
					goto continue_no_load;
				}
			}
			if (last_cell_bits != BitArray::zeros ()) {
				c = mapping_table[loc_end.array_idx].load (memory_order_seq_cst);
				if ((c & last_cell_bits) != BitArray::zeros ()) {
					// Sequence not big enough, restart from this cell, no need to reload
					search_at = loc_end;
					goto continue_no_load;
				}
			}
			if (set_bits (loc_start, first_cell_expected_value, loc_end, c)) {
				return loc_start.superpage_num ();
			} else {
				// Sequence cells have changed ; restart search at start of sequence
				search_at = loc_start;
				continue;
			}
		}

		// Not found, go to next cell
		search_at.next_array_cell ();
	}
	throw std::runtime_error ("SuperpageTracker: no superpage sequence found");
}

void SuperpageTracker::release (size_t superpage_num, size_t superpage_nb) {
	auto loc_start = Index (superpage_num);
	auto loc_end = Index (superpage_num + superpage_nb);
	clear_bits (loc_start, loc_end);
}

bool SuperpageTracker::is_mapped (size_t superpage_num) {
	using std::memory_order::memory_order_seq_cst;
	auto loc = Index (superpage_num);
	IntType c = mapping_table[loc.array_idx].load (memory_order_seq_cst);
	return BitArray::is_set (c, loc.bit_idx);
}

////////////////////////////

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
	using B = BitMask<unsigned long>;
	unsigned long a;
	scanf ("%lx", &a);
	printf ("%ld %ld\n", B::count_msb_zeros (a), B::count_lsb_zeros (a));
	B::print (a, std::cout);
	return 0;
}

