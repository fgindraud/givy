#ifndef SUPERPAGE_TRACKER_H
#define SUPERPAGE_TRACKER_H

#include <atomic>
#include <tuple>

#include "base_defs.h"
#include "reporting.h"
#include "utility.h"
#include "array.h"

namespace Givy {
namespace Allocator {

	template <typename Alloc> class SuperpageTracker {
		/* Tracks the state of superpages.
		 *
		 * Check if a superpage is used by the allocator.
		 * Find the first superpage of a superpage sequence.
		 * Acquire/release ownership of a superpage sequence.
		 *
		 * Acquire/release/reads can be called concurrently.
		 * Calling release twice for the same block is UB.
		 * Calling release concurrently with a test for the same block is UB.
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
		FixedArray<AtomicIntType, Alloc> mapping_table;
		FixedArray<AtomicIntType, Alloc> sequence_table;

	public:
		SuperpageTracker (const GasLayout & layout_, Alloc & allocator_);
		// No copy/move due to FixedArray

		/* Aquire/Release a superpage block, by superpage number.
		 * Trim will reduce a superpage block to 1 superpage.
		 */
		size_t acquire (size_t superpage_nb);
		void release (size_t superpage_num, size_t superpage_nb);
		void trim (size_t superpage_num, size_t superpage_nb) {
			ASSERT_STD (superpage_nb > 1);
			release (superpage_num + 1, superpage_nb - 1);
		}

		/* Get superpage block start
		 */
		size_t get_block_start_num (size_t superpage_num) const;
		Ptr get_block_start (Ptr inside_block) const {
			return layout.superpage (get_block_start_num (layout.superpage_num (inside_block)));
		}

		bool is_mapped (size_t superpage_num) const {
			auto loc = Index (superpage_num);
			IntType c = mapping_table[loc.array_idx].load (std::memory_order_seq_cst);
			return BitArray::is_set (c, loc.bit_idx);
		}
		bool is_mapped (Ptr ptr) const { return is_mapped (layout.superpage_num (ptr)); }

#ifdef ASSERT_SAFE_ENABLED
		void print (int superpage_by_line = 200) const;
#endif

	private:
		struct Index {
			// Helper type to represent a position in the table
			size_t array_idx;
			size_t bit_idx;

			Index (size_t array_idx_, size_t bit_idx_) : array_idx (array_idx_), bit_idx (bit_idx_) {}

			// Conversion from/to a superpage number
			explicit Index (size_t superpage_num_)
			    : Index (superpage_num_ / BitArray::Bits, superpage_num_ % BitArray::Bits) {}
			size_t superpage_num (void) const { return array_idx * BitArray::Bits + bit_idx; }

			// Movement in the table
			void next_array_cell_first_bit (void) {
				array_idx++;
				bit_idx = 0;
			}
			void prev_array_cell_last_bit (void) {
				ASSERT_STD (array_idx > 0);
				array_idx--;
				bit_idx = BitArray::Bits - 1;
			}

			bool operator<(const Index & rhs) const {
				return std::tie (array_idx, bit_idx) < std::tie (rhs.array_idx, rhs.bit_idx);
			}
		};

		// Bit manipulation helpers
		bool set_mapping_bits (Index loc_start, IntType expected_start, Index loc_end,
		                       IntType expected_end);
		void set_sequence_bits (Index loc_start, Index loc_end);
		bool set_bits (Index loc_start, IntType expected_start, Index loc_end, IntType expected_end);
		void clear_bits (Index loc_start, Index loc_end);
	};

	template <typename Alloc>
	SuperpageTracker<Alloc>::SuperpageTracker (const GasLayout & layout_, Alloc & allocator_)
	    : layout (layout_),
	      table_size (Math::divide_up (layout.superpage_total, BitArray::Bits)),
	      mapping_table (table_size, allocator_, BitArray::zeros ()),
	      sequence_table (table_size, allocator_, BitArray::zeros ()) {}

	template <typename Alloc> size_t SuperpageTracker<Alloc>::acquire (size_t superpage_nb) {
		/* I need to find a sequence of superpage_nb consecutive 0s anywhere in the table.
		 * For now, I perform a linear search of the table, with some optimisation to prevent
		 * using an atomic load twice on the same integer array cell if possible.
		 *
		 * The linear search will scan the table integer by integer for speed.
		 * Search starts at the start of the node-local virtual addresses segment.
		 * search_at.bit_idx is taken into account if search starts in the middle of a cell.
		 */
		ASSERT_STD (superpage_nb > 0);

		auto search_at = Index (layout.local_area_start_superpage_num ());
		auto search_end = Index (layout.local_area_end_superpage_num ());
		IntType c;
		while (search_at < search_end) {
			c = mapping_table[search_at.array_idx].load (std::memory_order_seq_cst);
		continue_no_load:

			if (c == BitArray::ones ()) {
				// Completely full cell, skip
				search_at.next_array_cell_first_bit ();
				continue;
			}

			size_t limit =
			    (search_at.array_idx == search_end.array_idx) ? search_end.bit_idx : BitArray::Bits;
			if (search_at.bit_idx + superpage_nb <= limit) {
				size_t pos = BitArray::find_zero_subsequence (c, superpage_nb, search_at.bit_idx, limit);
				if (pos < BitArray::Bits) {
					/* A sequence has been found, and it is contained in one array cell.
					 * I only need one atomic operation to reserve it.
					 *
					 * On bit flip failure, restart the search at the same cell.
					 * It is unlikely that previous bits have been freed, so no need to go back from the
					 * start.
					 */
					auto loc_start = Index (search_at.array_idx, pos);
					auto loc_end = Index (search_at.array_idx, pos + superpage_nb);
					if (!set_bits (loc_start, c, loc_end, BitArray::zeros ()))
						continue;
					return loc_start.superpage_num ();
				}
			}

			// Look for the start of a multicell sequence
			size_t msb_zeros =
			    std::min (BitArray::count_msb_zeros (c), BitArray::Bits - search_at.bit_idx);
			if (msb_zeros > 0) {
				/* A possible start of sequence has been found.
				 * I will search the next cells to see if there is enough consecutive 0s to be a valid
				 * sequence.
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
				for (size_t idx = loc_start.array_idx + 1; idx < loc_end.array_idx; ++idx) {
					c = mapping_table[idx].load (std::memory_order_seq_cst);
					if (c != BitArray::zeros ()) {
						// Zero sequence is not big enough ; restart search from this cell, no need to reload
						search_at = Index (idx, 0);
						goto continue_no_load;
					}
				}
				if (last_cell_bits != BitArray::zeros ()) {
					c = mapping_table[loc_end.array_idx].load (std::memory_order_seq_cst);
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
			search_at.next_array_cell_first_bit ();
		}
		ASSERT_FAIL ("SuperpageTracker: no superpage sequence found");
		return 0;
	}

	template <typename Alloc>
	void SuperpageTracker<Alloc>::release (size_t superpage_num, size_t superpage_nb) {
		/* Just clear the bits in the two tables.
		 */
		auto loc_start = Index (superpage_num);
		auto loc_end = Index (superpage_num + superpage_nb);
		ASSERT_STD (loc_end.array_idx < table_size);
		clear_bits (loc_start, loc_end);
	}

	template <typename Alloc>
	size_t SuperpageTracker<Alloc>::get_block_start_num (size_t superpage_num) const {
		/* Find the first 0 in the sequence_table, looking backward from the starting position.
		 * Note that it won't check if the superpages are in the mapped table.
		 */
		auto loc = Index (superpage_num);
		ASSERT_STD (loc.array_idx < table_size);
		while (true) {
			IntType c = sequence_table[loc.array_idx].load (std::memory_order_seq_cst);
			// Try to find preceeding zero
			size_t prev_zero = BitArray::find_previous_zero (c, loc.bit_idx);
			if (prev_zero != BitArray::Bits)
				return Index (loc.array_idx, prev_zero).superpage_num ();
			// Continue search in previous word
			loc.prev_array_cell_last_bit ();
		}
	}

	template <typename Alloc>
	bool SuperpageTracker<Alloc>::set_mapping_bits (Index loc_start, IntType expected_start,
	                                                Index loc_end, IntType expected_end) {
		if (loc_start.array_idx == loc_end.array_idx) {
			// One cell span
			return mapping_table[loc_start.array_idx].compare_exchange_strong (
			    expected_start,
			    expected_start | BitArray::window_bound (loc_start.bit_idx, loc_end.bit_idx),
			    std::memory_order_seq_cst);
		} else {
			/* Multicell span
			 * Try to set bits (start, then middle, then end), revert to previous state on failure
			 */
			IntType loc_start_bits = BitArray::window_bound (loc_start.bit_idx, BitArray::Bits);
			if (mapping_table[loc_start.array_idx].compare_exchange_strong (
			        expected_start, expected_start | loc_start_bits, std::memory_order_seq_cst)) {
				size_t idx;
				for (idx = loc_start.array_idx + 1; idx < loc_end.array_idx; ++idx) {
					IntType expected = BitArray::zeros ();
					if (!mapping_table[idx].compare_exchange_strong (expected, BitArray::ones (),
					                                                 std::memory_order_seq_cst))
						break;
				}
				if (idx == loc_end.array_idx) {
					IntType loc_end_bits = BitArray::window_bound (0, loc_end.bit_idx);
					if (loc_end_bits == BitArray::zeros ()) {
						return true; // Nothing to do, setting bits is already a success
					} else {
						if (mapping_table[loc_end.array_idx].compare_exchange_strong (
						        expected_end, expected_end | loc_end_bits, std::memory_order_seq_cst))
							return true; // Success too
					}
				}
				// Cleanup on failure
				for (size_t clean_idx = loc_start.array_idx + 1; clean_idx < idx; ++clean_idx)
					mapping_table[clean_idx].store (BitArray::zeros (), std::memory_order_seq_cst);
				mapping_table[loc_start.array_idx].fetch_and (~loc_start_bits, std::memory_order_seq_cst);
			}
			return false;
		}
	}

	template <typename Alloc>
	void SuperpageTracker<Alloc>::set_sequence_bits (Index loc_start, Index loc_end) {
		// No need to compare_exchange ; we are supposed to own the sequence bits as we reserved the
		// area through mapping bits
		if (loc_start.array_idx == loc_end.array_idx) {
			// One cell span
			IntType bits = BitArray::window_bound (loc_start.bit_idx + 1, loc_end.bit_idx);
			if (bits != BitArray::zeros ())
				sequence_table[loc_start.array_idx].fetch_or (bits, std::memory_order_seq_cst);
		} else {
			// Multiple cell span
			IntType first_cell_bits = BitArray::window_bound (loc_start.bit_idx + 1, BitArray::Bits);
			IntType last_cell_bits = BitArray::window_bound (0, loc_end.bit_idx);

			sequence_table[loc_start.array_idx].fetch_or (first_cell_bits, std::memory_order_seq_cst);
			for (size_t i = loc_start.array_idx + 1; i < loc_end.array_idx; i++)
				sequence_table[i].store (BitArray::ones (), std::memory_order_seq_cst);
			if (last_cell_bits != BitArray::zeros ())
				sequence_table[loc_end.array_idx].fetch_or (last_cell_bits, std::memory_order_seq_cst);
		}
	}

	template <typename Alloc>
	bool SuperpageTracker<Alloc>::set_bits (Index loc_start, IntType expected_start, Index loc_end,
	                                        IntType expected_end) {
		if (set_mapping_bits (loc_start, expected_start, loc_end, expected_end)) {
			// Mapping bits are always set first
			set_sequence_bits (loc_start, loc_end);
			return true;
		} else {
			return false;
		}
	}

	template <typename Alloc>
	void SuperpageTracker<Alloc>::clear_bits (Index loc_start, Index loc_end) {
		/* Clears bits to release a superpage sequence.
		 * sequence_table is cleared first, then mapping_table.
		 *
		 * The mapping pattern is used to clear both mapping/sequence bits because sequence bits
		 * are a subset of mapping bits:
		 * mapping:  111..11
		 * sequence: 011..11
		 */
		if (loc_start.array_idx == loc_end.array_idx) {
			// One cell span
			IntType bits = BitArray::window_bound (loc_start.bit_idx, loc_end.bit_idx);
			if (loc_end.bit_idx - loc_start.bit_idx > 1)
				sequence_table[loc_start.array_idx].fetch_and (~bits, std::memory_order_seq_cst);
			mapping_table[loc_start.array_idx].fetch_and (~bits, std::memory_order_seq_cst);
		} else {
			// Multiple cells span
			IntType first_cell_bits = BitArray::window_bound (loc_start.bit_idx, BitArray::Bits);
			IntType last_cell_bits = BitArray::window_bound (0, loc_end.bit_idx);

			sequence_table[loc_start.array_idx].fetch_and (~first_cell_bits, std::memory_order_seq_cst);
			for (size_t i = loc_start.array_idx + 1; i < loc_end.array_idx; i++)
				sequence_table[i].store (BitArray::zeros (), std::memory_order_seq_cst);
			if (last_cell_bits != BitArray::zeros ())
				sequence_table[loc_end.array_idx].fetch_and (~last_cell_bits, std::memory_order_seq_cst);

			mapping_table[loc_start.array_idx].fetch_and (~first_cell_bits, std::memory_order_seq_cst);
			for (size_t i = loc_start.array_idx + 1; i < loc_end.array_idx; i++)
				mapping_table[i].store (BitArray::zeros (), std::memory_order_seq_cst);
			if (last_cell_bits != BitArray::zeros ())
				mapping_table[loc_end.array_idx].fetch_and (~last_cell_bits, std::memory_order_seq_cst);
		}
	}

#ifdef ASSERT_SAFE_ENABLED
	template <typename Alloc> void SuperpageTracker<Alloc>::print (int superpage_by_line) const {
		const int indicator_interval = 10;
		const int line_prefix_size = 10;
		ASSERT_STD (superpage_by_line > 0);

		// Indicators
		size_t nb_indicator = Math::divide_up (superpage_by_line, indicator_interval) + 1;
		printf ("%*c", line_prefix_size, ' ');
		for (size_t i = 0; i < nb_indicator; ++i)
			printf ("%-*zu", indicator_interval, i * indicator_interval);
		printf ("\n%*c", line_prefix_size, ' ');
		for (size_t i = 0; i < nb_indicator; ++i)
			printf ("/%*c", indicator_interval - 1, ' ');

		// Data
		IntType m = 0;
		IntType s = 0;
		for (int node = 0; node < layout.nb_node; ++node) {
			size_t start = layout.node_area_start_superpage_num (node);
			for (size_t sp = start; sp < layout.node_area_end_superpage_num (node); ++sp) {
				if ((sp - start) % superpage_by_line == 0)
					printf ("\n%-*zu", line_prefix_size, sp);

				auto idx = Index (sp);
				if (idx.bit_idx == 0) {
					m = mapping_table[idx.array_idx];
					s = sequence_table[idx.array_idx];
				}

				int c;
				if (BitArray::is_set (m, idx.bit_idx)) {
					if (BitArray::is_set (s, idx.bit_idx))
						c = '=';
					else
						c = '#';
				} else {
					if (BitArray::is_set (s, idx.bit_idx))
						c = '?';
					else
						c = '_';
				}
				printf ("%c", c);
			}
		}
		printf ("\n");
	}
#endif
}
}

#endif
