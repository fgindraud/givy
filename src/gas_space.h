#pragma once
#ifndef GIVY_GAS_SPACE_H
#define GIVY_GAS_SPACE_H

#include <cstdio>

#include "pointer.h"
#include "range.h"
#include "reporting.h"
#include "system.h"
#include "memory_mapping.h"
#include "superpage_tracker.h"
#include "allocator_bootstrap.h"

namespace Givy {
namespace Gas {
	class Space {
		/* Gas organisation class.
		 * Manages the virtual address space of the GAS.
		 */
	private:
		const size_t nb_node;
		const size_t local_node;
		const size_t superpage_by_node;

		const Range<Ptr> gas_interval;
		const Range<size_t> local_interval_sp;
		const Range<Ptr> local_interval;

		SuperpageTracker<Allocator::Bootstrap> superpage_tracker;

	public:
		Space (Ptr gas_start_, size_t space_by_node_, size_t nb_node_, size_t local_node_,
		       Allocator::Bootstrap & alloc)
		    : // node info
		      nb_node (nb_node_),
		      local_node (local_node_),
		      // size
		      superpage_by_node (Math::divide_up (space_by_node_, VMem::superpage_size)),
		      // position
		      gas_interval (gas_start_.align_up (VMem::superpage_size) +
		                    VMem::superpage_size * superpage_by_node * range (nb_node)),
		      local_interval_sp (range_from_offset (local_node, 1) * superpage_by_node),
		      local_interval (gas_interval.first () + VMem::superpage_size * local_interval_sp),
		      // spt
		      superpage_tracker (superpage_by_node * nb_node, alloc) {
			ASSERT_STD (nb_node > 0);
			ASSERT_STD (superpage_by_node > 0);
			ASSERT_STD (local_node < nb_node);
		}

		// Position info
		bool in_gas (Ptr p) const { return gas_interval.contains (p); }
		bool in_gas (const Range<Ptr> & r) const { return gas_interval.includes (r); }

		bool in_local_interval (Ptr p) const { return local_interval.contains (p); }
		bool in_local_interval (const Range<Ptr> & r) const { return local_interval.includes (r); }

		size_t node_of_allocation (Ptr p) const {
			ASSERT_SAFE (in_gas (p));
			return (p - gas_interval.first ()) / (superpage_by_node * VMem::superpage_size);
		}

		// Superpage management
		Ptr reserve_local_superpage_sequence (size_t superpage_nb) {
			ASSERT_SAFE (superpage_nb > 0);
			auto base = superpage (superpage_tracker.acquire (superpage_nb, local_interval_sp));
			VMem::map_checked (base, VMem::superpage_size * superpage_nb);
			return base;
		}

		void release_superpage_sequence (Ptr base, size_t superpage_nb) {
			ASSERT_SAFE (in_gas (range_from_offset (base, superpage_nb * VMem::superpage_size)));
			ASSERT_SAFE (superpage_nb > 0);
			superpage_tracker.release (range_from_offset (superpage_num (base), superpage_nb));
			VMem::unmap_checked (base, VMem::superpage_size * superpage_nb);
			// FIXME possible data race if sequence is reserved before
		}

		void trim_superpage_sequence (Ptr base, size_t superpage_nb) {
			ASSERT_SAFE (in_gas (range_from_offset (base, superpage_nb * VMem::superpage_size)));
			ASSERT_SAFE (superpage_nb > 1);
			superpage_tracker.trim (range_from_offset (superpage_num (base), superpage_nb));
			VMem::unmap_checked (base + VMem::superpage_size, VMem::superpage_size * (superpage_nb - 1));
			// FIXME same
		}

		Ptr superpage_sequence_start (Ptr inside) const {
			ASSERT_SAFE (in_gas (inside));
			return superpage (superpage_tracker.get_sequence_start_num (superpage_num (inside)));
		}

		// Conversion between pointer and superpage number
		Ptr superpage (size_t num) const { return gas_interval.first () + VMem::superpage_size * num; }
		size_t superpage_num (Ptr inside) const {
			return inside.sub (gas_interval.first ()) / VMem::superpage_size;
		}

#ifdef ASSERT_SAFE_ENABLED
	public:
		void print (void) const {
			printf ("Layout:\n");
			printf ("\tnodes (local node): %zu (%zu)\n", nb_node, local_node);
			printf ("\tsuperpage by node (total): %zu (%zu)\n", superpage_by_node,
			        superpage_by_node * nb_node);
			printf ("\tnode area limits (sp index): [0");
			for (auto n : range (nb_node))
				printf (",%zu", superpage_by_node * n);
			printf ("]\n");

			printf ("SuperpageTracker:\n");
			superpage_tracker.print (nb_node, superpage_by_node);
		}
#endif
	};
}
}
#endif
