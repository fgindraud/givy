#ifndef BASE_DEFS_H
#define BASE_DEFS_H

#include <cstdint> // uintN_t uintptr_t
#include <cstddef> // size_t std::nullptr_t
#include <limits>  // numeric_limits
#include <climits> // CHAR_BIT

// system specific
#include <unistd.h>

#include "assert_level.h"

namespace Givy {

using std::size_t;

/* ------------------------ Integer math utils -------------------- */

namespace Math {
	template <typename T> static constexpr bool is_power_2 (T x) noexcept {
		static_assert (std::numeric_limits<T>::is_integer, "T must be an integer");
		return x > 0 && (x & (x - 1)) == 0;
	}
	template <typename T> static constexpr T divide_up (T n, T div) noexcept {
		static_assert (std::numeric_limits<T>::is_integer, "T must be an integer");
		return (n + div - 1) / div;
	}
	template <typename T> static constexpr T align (T n, T align) noexcept {
		static_assert (std::numeric_limits<T>::is_integer, "T must be an integer");
		return (n / align) * align;
	}
	template <typename T> static constexpr T align_up (T n, T align) noexcept {
		static_assert (std::numeric_limits<T>::is_integer, "T must be an integer");
		return divide_up (n, align) * align;
	}
}

/* ------------------------------ Raw Ptr type ---------------------- */

struct Ptr {
	/* Raw pointer like object ; allows cleaner raw pointer manipulation
	 */
	uintptr_t p;

	explicit constexpr Ptr (uintptr_t ptr) noexcept : p (ptr) {}
	constexpr Ptr (std::nullptr_t) noexcept : p (0) {}
	template <typename T> explicit constexpr Ptr (T * ptr) noexcept : p (reinterpret_cast<uintptr_t> (ptr)) {}

	template <typename T> constexpr T as (void) const noexcept { return reinterpret_cast<T> (p); }
	template <typename T> constexpr operator T *(void) const noexcept { return as<T *> (); }

	constexpr Ptr add (size_t off) const noexcept { return Ptr (p + off); }
	constexpr Ptr sub (size_t off) const noexcept { return Ptr (p - off); }
	constexpr size_t sub (Ptr ptr) const noexcept { return p - ptr.p; }
	constexpr Ptr lshift (size_t sh) const noexcept { return Ptr (p << sh); }
	constexpr Ptr rshift (size_t sh) const noexcept { return Ptr (p >> sh); }

	constexpr Ptr operator+(size_t off) const noexcept { return add (off); }
	constexpr Ptr & operator+=(size_t off) noexcept { return * this = add (off); }
	constexpr Ptr operator-(size_t off) const noexcept { return sub (off); }
	constexpr Ptr & operator-=(size_t off) noexcept { return * this = sub (off); }

	// align : backward ; align_up : forward
	constexpr Ptr align (size_t al) const noexcept { return Ptr (Math::align (p, al)); }
	constexpr Ptr align_up (size_t al) const noexcept { return Ptr (Math::align_up (p, al)); }
	constexpr bool is_aligned (size_t al) const noexcept { return p % al == 0; }

	// Compute diff
	size_t operator-(Ptr other) const noexcept { return p - other.p; }
};

constexpr bool operator<(Ptr lhs, Ptr rhs) noexcept {
	return lhs.p < rhs.p;
}
constexpr bool operator>(Ptr lhs, Ptr rhs) noexcept {
	return lhs.p > rhs.p;
}
constexpr bool operator<=(Ptr lhs, Ptr rhs) noexcept {
	return lhs.p <= rhs.p;
}
constexpr bool operator>=(Ptr lhs, Ptr rhs) noexcept {
	return lhs.p >= rhs.p;
}
constexpr bool operator==(Ptr lhs, Ptr rhs) noexcept {
	return lhs.p == rhs.p;
}
constexpr bool operator!=(Ptr lhs, Ptr rhs) noexcept {
	return lhs.p != rhs.p;
}

/* ----------------------------------- Memory block ------------------------------- */

struct Block {
	Ptr ptr;
	size_t size;
};

/* ------------------------------ Low level memory management ---------------------- */

namespace VMem {
	/* Virtual memory info
	 */

	// System basic pages
	static const size_t PageShift = 12;
	static const size_t PageSize = 1 << PageShift;
	// Superpage : 2MB
	static const size_t SuperpageShift = PageShift + 9;
	static const size_t SuperpageSize = 1 << SuperpageShift;
	static const size_t SuperpagePageNB = 1 << (SuperpageShift - PageShift);
	// Some checks
	static_assert (sizeof (void *) == 8, "64 bit arch required");
	static_assert (SuperpageSize > PageSize, "SuperpageSize <= PageSize");
	static inline void runtime_asserts (void) { ASSERT_STD (sysconf (_SC_PAGESIZE) == PageSize); }
}

/* --------------------------- Global address space memory layout --------------------- */

struct GasLayout {
	const Ptr start;
	const size_t space_by_node;
	const int nb_node;
	const int local_node;

	const size_t superpage_by_node;
	const size_t superpage_total;

	GasLayout (Ptr start_, size_t space_by_node_, int nb_node_, int local_node_)
	    : start (Ptr (start_).align_up (VMem::SuperpageSize)),
	      space_by_node (Math::align_up (space_by_node_, VMem::SuperpageSize)), nb_node (nb_node_),
	      local_node (local_node_),
	      // derived
	      superpage_by_node (Math::divide_up (space_by_node_, VMem::SuperpageSize)),
	      superpage_total (superpage_by_node * nb_node) {
		ASSERT_STD (nb_node_ > 0);
	}

	Ptr superpage (size_t num) const { return start + VMem::SuperpageSize * num; }
	size_t superpage (Ptr inside) const { return inside.sub (start) / VMem::SuperpageSize; }

	size_t node_area_start_superpage_num (int node) const { return superpage_by_node * node; }
	size_t node_area_end_superpage_num (int node) const { return node_area_start_superpage_num (node + 1); }
	size_t local_area_start_superpage_num () const { return node_area_start_superpage_num (local_node); }
	size_t local_area_end_superpage_num () const { return node_area_end_superpage_num (local_node); }
};
}

#endif
