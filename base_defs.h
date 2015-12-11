#ifndef BASE_DEFS_H
#define BASE_DEFS_H

#include <cstdint> // uintN_t uintptr_t
#include <cstddef> // size_t std::nullptr_t
#include <limits>  // numeric_limits
#include <climits> // CHAR_BIT

// system specific
#include <unistd.h>

#include "reporting.h"

namespace Givy {

using std::size_t;

/* ------------------------ Integer math utils -------------------- */

namespace Math {
	template <typename T> constexpr T divide_up (T n, T div) {
		static_assert (std::numeric_limits<T>::is_integer, "T must be an integer");
		return (n + div - 1) / div;
	}
	template <typename T> constexpr T align (T n, T align) {
		static_assert (std::numeric_limits<T>::is_integer, "T must be an integer");
		return (n / align) * align;
	}
	template <typename T> constexpr T align_up (T n, T align) {
		static_assert (std::numeric_limits<T>::is_integer, "T must be an integer");
		return divide_up (n, align) * align;
	}
}

/* ------------------------------ Raw Ptr type ---------------------- */

struct Ptr {
	/* Raw pointer like object ; allows cleaner raw pointer manipulation
	 */
	uintptr_t p;

	Ptr () = default; // Unitialized
	Ptr (std::nullptr_t) : p (0) {}
	explicit Ptr (uintptr_t ptr) : p (ptr) {}
	template <typename T> explicit Ptr (T * ptr) : p (reinterpret_cast<uintptr_t> (ptr)) {}

	template <typename T> T as (void) const { return reinterpret_cast<T> (p); }
	template <typename T> operator T *(void) const { return as<T *> (); }

	Ptr add (size_t off) const { return Ptr (p + off); }
	Ptr sub (size_t off) const { return Ptr (p - off); }
	size_t sub (Ptr ptr) const { return p - ptr.p; }
	Ptr lshift (size_t sh) const { return Ptr (p << sh); }
	Ptr rshift (size_t sh) const { return Ptr (p >> sh); }

	Ptr operator+(size_t off) const { return add (off); }
	Ptr & operator+=(size_t off) { return * this = add (off); }
	Ptr operator-(size_t off) const { return sub (off); }
	Ptr & operator-=(size_t off) { return * this = sub (off); }

	// align : backward ; align_up : forward
	Ptr align (size_t al) const { return Ptr (Math::align (p, al)); }
	Ptr align_up (size_t al) const { return Ptr (Math::align_up (p, al)); }
	bool is_aligned (size_t al) const { return p % al == 0; }

	// Compute diff
	size_t operator-(Ptr other) const { return p - other.p; }
};

bool operator<(Ptr lhs, Ptr rhs) {
	return lhs.p < rhs.p;
}
bool operator>(Ptr lhs, Ptr rhs) {
	return lhs.p > rhs.p;
}
bool operator<=(Ptr lhs, Ptr rhs) {
	return lhs.p <= rhs.p;
}
bool operator>=(Ptr lhs, Ptr rhs) {
	return lhs.p >= rhs.p;
}
bool operator==(Ptr lhs, Ptr rhs) {
	return lhs.p == rhs.p;
}
bool operator!=(Ptr lhs, Ptr rhs) {
	return lhs.p != rhs.p;
}

/* ----------------------------------- Memory block ------------------------------- */

/* Basic type to represent a piece of memory (ptr and size).
 */
struct Block {
	Ptr ptr{nullptr};
	size_t size{0};
};

/* ------------------------------ Low level memory management ---------------------- */

namespace VMem {
	/* Virtual memory info
	 */

	// System basic pages
	constexpr size_t PageShift = 12;
	constexpr size_t PageSize = 1 << PageShift;
	// Superpage : 2MB
	constexpr size_t SuperpageShift = PageShift + 9;
	constexpr size_t SuperpageSize = 1 << SuperpageShift;
	constexpr size_t SuperpagePageNB = 1 << (SuperpageShift - PageShift);
	// Some checks
	static_assert (sizeof (void *) == 8, "64 bit arch required");
	static_assert (SuperpageSize > PageSize, "SuperpageSize <= PageSize");
	inline void runtime_asserts (void) { ASSERT_STD (sysconf (_SC_PAGESIZE) == PageSize); }
}

/* --------------------------- Global address space memory layout --------------------- */

struct GasLayout {
	/* Layout of GAS :
	 * - before start pointer, growing backward, small basic dynaùmic structures (bootstrap allocator)
	 * - starting from start pointer, nb_node areas of superpage_by_node superpages
	 */
	const Ptr start;
	const size_t space_by_node;
	const int nb_node;
	const int local_node;

	const size_t superpage_by_node;
	const size_t superpage_total;

	const Ptr local_area_start;
	const Ptr local_area_end;

	GasLayout (Ptr start_, size_t space_by_node_, int nb_node_, int local_node_)
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
	size_t node_area_start_superpage_num (int node) const { return superpage_by_node * node; }
	size_t node_area_end_superpage_num (int node) const {
		return node_area_start_superpage_num (node + 1);
	}
	size_t local_area_start_superpage_num () const {
		return node_area_start_superpage_num (local_node);
	}
	size_t local_area_end_superpage_num () const { return node_area_end_superpage_num (local_node); }
	int area_index (Ptr ptr) const { return superpage_num (ptr) / superpage_by_node; }
	bool in_local_area (Ptr ptr) const { return local_area_start <= ptr && ptr < local_area_end; }

	/* Conversion between pointer and superpage number.
	 */
	Ptr superpage (size_t num) const { return start + VMem::SuperpageSize * num; }
	size_t superpage_num (Ptr inside) const { return inside.sub (start) / VMem::SuperpageSize; }
};
}

#endif
