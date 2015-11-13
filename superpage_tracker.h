#ifndef SUPERPAGE_TRACKER_H
#define SUPERPAGE_TRACKER_H

#include <atomic>
#include <tuple>

#include "base_defs.h"

class SuperpageTracker {
	/* Tracks the state of superpages.
	 *
	 * Check if a superpage is used by the allocator.
	 * Find the first superpage of a superpage sequence.
	 * Acquire/release ownership of a superpage sequence.
	 */
private:
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

public:
	template <typename Alloc>
	SuperpageTracker (const GasLayout & layout_, Alloc & allocator)
	    : layout (layout_), table_size (Math::divide_up (layout.superpage_total, BitArray::Bits)) {
		mapping_table =
		    allocator.allocate (table_size * sizeof (AtomicIntType), alignof (AtomicIntType)).ptr;
		sequence_table =
		    allocator.allocate (table_size * sizeof (AtomicIntType), alignof (AtomicIntType)).ptr;
		for (size_t it = 0; it < table_size; ++it) {
			new (&mapping_table[it]) AtomicIntType (BitArray::zeros ());
			new (&sequence_table[it]) AtomicIntType (BitArray::zeros ());
		}
	}

	Ptr acquire (size_t superpage_nb) { return layout.superpage (acquire_num (superpage_nb)); }

	size_t acquire_num (size_t superpage_nb);
	void release_num (size_t superpage_num, size_t superpage_nb);

	bool is_mapped (size_t superpage_num) {
		using std::memory_order::memory_order_seq_cst;
		auto loc = Index (superpage_num);
		IntType c = mapping_table[loc.array_idx].load (memory_order_seq_cst);
		return BitArray::is_set (c, loc.bit_idx);
	}
	bool is_mapped (Ptr ptr) { return is_mapped (layout.superpage (ptr)); }

	void print (int superpage_by_line = 200) const;

private:
	struct Index {
		size_t array_idx;
		size_t bit_idx;

		Index (size_t array_idx_, size_t bit_idx_) : array_idx (array_idx_), bit_idx (bit_idx_) {}
		explicit Index (size_t superpage_num_)
		    : Index (superpage_num_ / BitArray::Bits, superpage_num_ % BitArray::Bits) {}

		size_t superpage_num (void) const { return array_idx * BitArray::Bits + bit_idx; }
		void next_array_cell (void) {
			array_idx++;
			bit_idx = 0;
		}
		bool operator<(const Index & rhs) const {
			return std::tie (array_idx, bit_idx) < std::tie (rhs.array_idx, rhs.bit_idx);
		}
	};

	bool set_mapping_bits (Index loc_start, IntType expected_start, Index loc_end,
	                       IntType expected_end);
	void set_sequence_bits (Index loc_start, Index loc_end);
	bool set_bits (Index loc_start, IntType expected_start, Index loc_end, IntType expected_end);
	void clear_bits (Index loc_start, Index loc_end);
};

#endif
