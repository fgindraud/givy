#ifndef GAS_lAYOUT_H
#define GAS_lAYOUT_H

#include <unistd.h> // sysconf

#include "pointer.h"
#include "reporting.h"

namespace Givy {

namespace VMem {
	/* Virtual memory constants
	 */

	// System basic pages
	constexpr size_t PageShift = 12;
	constexpr size_t PageSize = 1 << PageShift;
	// Superpage : 2MB
	constexpr size_t SuperpageShift = PageShift + 9;
	constexpr size_t SuperpageSize = 1 << SuperpageShift;
	constexpr size_t SuperpagePageNB = 1 << (SuperpageShift - PageShift);
	// Some checks
	static_assert (SuperpageSize > PageSize, "SuperpageSize <= PageSize");
	inline void runtime_asserts (void) { ASSERT_STD (sysconf (_SC_PAGESIZE) == PageSize); }
}

struct GasLayout {
	/* Layout of GAS :
	 * - before start pointer, growing backward, small basic dynaùmic structures (bootstrap allocator)
	 * - starting from start pointer, nb_node areas of superpage_by_node superpages
	 */
	const Ptr start;
	const size_t space_by_node;
	const size_t nb_node;
	const size_t local_node;

	const size_t superpage_by_node;
	const size_t superpage_total;

	const Ptr local_area_start;
	const Ptr local_area_end;

	GasLayout (Ptr start_, size_t space_by_node_, size_t nb_node_, size_t local_node_)
	    : start (Ptr (start_).align_up (VMem::SuperpageSize)),
	      space_by_node (Math::align_up (space_by_node_, VMem::SuperpageSize)),
	      nb_node (nb_node_),
	      local_node (local_node_),
	      // derived
	      superpage_by_node (Math::divide_up (space_by_node_, VMem::SuperpageSize)),
	      superpage_total (superpage_by_node * nb_node),
	      local_area_start (superpage (local_area_start_superpage_num ())),
	      local_area_end (superpage (local_area_end_superpage_num ())) {
		ASSERT_STD (nb_node_ > 0);
	}

	/* Get boundaries of node memory areas.
	 * This is in superpage number (index from the first superpage at start pointer).
	 */
	size_t node_area_start_superpage_num (size_t node) const { return superpage_by_node * node; }
	size_t node_area_end_superpage_num (size_t node) const {
		return node_area_start_superpage_num (node + 1);
	}
	size_t local_area_start_superpage_num () const {
		return node_area_start_superpage_num (local_node);
	}
	size_t local_area_end_superpage_num () const { return node_area_end_superpage_num (local_node); }
	size_t area_index (Ptr ptr) const { return superpage_num (ptr) / superpage_by_node; }
	bool in_local_area (Ptr ptr) const { return local_area_start <= ptr && ptr < local_area_end; }

	/* Conversion between pointer and superpage number.
	 */
	Ptr superpage (size_t num) const { return start + VMem::SuperpageSize * num; }
	size_t superpage_num (Ptr inside) const { return inside.sub (start) / VMem::SuperpageSize; }
};
}

#endif
