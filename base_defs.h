#ifndef BASE_DEFS_H
#define BASE_DEFS_H

#include <cstdint> // uintN_t uintptr_t
#include <cstddef> // size_t std::nullptr_t
#include <cerrno>
#include <system_error> // mmap/munmap error exceptions
#include <limits>       // numeric_limits
#include <climits>      // CHAR_BIT
#include <utility>      // std::forward

using std::size_t;

// system specific
#include <sys/mman.h>
#include <unistd.h>

#include "assert_level.h"

struct Math {
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
};

/* ------------------------ BitMask management -------------------- */

template <typename IntType> struct BitMask {
	/* We index bits in LSB->MSB order
	 */
	static_assert (std::numeric_limits<IntType>::is_integer, "IntType must be an integer");
	static_assert (!std::numeric_limits<IntType>::is_signed, "IntType must be unsigned");

	static constexpr size_t Bits = std::numeric_limits<IntType>::digits;

	static constexpr IntType zeros (void) noexcept { return 0; }
	static constexpr IntType ones (void) noexcept { return std::numeric_limits<IntType>::max (); }
	static constexpr IntType one (void) noexcept { return 0x1; }

	static constexpr IntType lsb_ones (size_t nb) noexcept {
		// nb 1s followed by 0s
		if (nb == 0)
			return 0;
		else
			return ones () >> (Bits - nb);
	}
	static constexpr IntType msb_ones (size_t nb) noexcept {
		// 0s followed by nb 1s
		if (nb == 0)
			return 0;
		else
			return ones () << (Bits - nb);
	}
	static constexpr IntType window_size (size_t start, size_t size) noexcept {
		// 0s until start, then 1s until end, then 0s
		if (start >= Bits)
			return 0;
		else
			return lsb_ones (size) << start;
	}
	static constexpr IntType window_bound (size_t start, size_t end) noexcept {
		// 0s until start, then 1s until end, then 0s
		return window_size (start, end - start);
	}

	static constexpr bool is_set (IntType i, size_t bit) noexcept {
		if (bit >= Bits)
			return false;
		else
			return (one () << bit) & i;
	}
	static size_t find_zero_subsequence (IntType searched, size_t len, size_t from_bit,
	                                     size_t up_to_bit = Bits) noexcept {
		// return first found offset, or Bits if not found
		size_t window_end = from_bit + len;
		IntType bit_window = window_bound (from_bit, window_end);
		while (window_end <= up_to_bit) {
			if ((searched & bit_window) == zeros ())
				return window_end - len; // found
			bit_window <<= 1;
			window_end++;
		}
		return Bits; // not found
	}
	static constexpr size_t count_msb_zeros (IntType c) noexcept {
		size_t b = Bits;
		for (; c; c >>= 1, --b)
			;
		return b;
	}
	static constexpr size_t count_lsb_zeros (IntType c) noexcept {
		size_t b = Bits;
		for (; c; c <<= 1, --b)
			;
		return b;
	}
	static constexpr size_t count_zeros (IntType c) noexcept {
		size_t b = 0;
		for (; c; c >>= 1)
			if (c & one () == zeros ())
				b++;
		return b;
	}

	static const char * str (IntType c) {
		static char buffer[Bits + 1] = {'\0'};
		for (size_t i = 0; i < Bits; ++i) {
			buffer[i] = (IntType (0x1) & c) ? '1' : '0';
			c <<= 1;
		}
		return buffer;
	}
};

#if defined(__GNUC__) || defined(__clang__)
template <> constexpr size_t BitMask<unsigned int>::count_msb_zeros (unsigned int c) noexcept {
	size_t b = Bits;
	if (c != 0)
		b = __builtin_clz (c);
	return b;
}
template <> constexpr size_t BitMask<unsigned long>::count_msb_zeros (unsigned long c) noexcept {
	size_t b = Bits;
	if (c != 0)
		b = __builtin_clzl (c);
	return b;
}
template <>
constexpr size_t BitMask<unsigned long long>::count_msb_zeros (unsigned long long c) noexcept {
	size_t b = Bits;
	if (c != 0)
		b = __builtin_clzll (c);
	return b;
}
template <> constexpr size_t BitMask<unsigned int>::count_lsb_zeros (unsigned int c) noexcept {
	size_t b = Bits;
	if (c != 0)
		b = __builtin_ctz (c);
	return b;
}
template <> constexpr size_t BitMask<unsigned long>::count_lsb_zeros (unsigned long c) noexcept {
	size_t b = Bits;
	if (c != 0)
		b = __builtin_ctzl (c);
	return b;
}
template <>
constexpr size_t BitMask<unsigned long long>::count_lsb_zeros (unsigned long long c) noexcept {
	size_t b = Bits;
	if (c != 0)
		b = __builtin_ctzll (c);
	return b;
}
template <> constexpr size_t BitMask<unsigned int>::count_zeros (unsigned int c) noexcept {
	return Bits - __builtin_popcount (c);
}
template <> constexpr size_t BitMask<unsigned long>::count_zeros (unsigned long c) noexcept {
	return Bits - __builtin_popcountl (c);
}
template <>
constexpr size_t BitMask<unsigned long long>::count_zeros (unsigned long long c) noexcept {
	return Bits - __builtin_popcountll (c);
}
#endif

/* ------------------------------ Raw Ptr type ---------------------- */

struct Ptr {
	/* Raw pointer like object ; allows cleaner raw pointer manipulation
	 */
	uintptr_t p;

	explicit constexpr Ptr (uintptr_t ptr) noexcept : p (ptr) {}
	constexpr Ptr (std::nullptr_t) noexcept : p (0) {}
	template <typename T>
	explicit constexpr Ptr (T * ptr) noexcept : p (reinterpret_cast<uintptr_t> (ptr)) {}

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

/* ------------------------------ Low level memory management ---------------------- */

struct VMem {
	/* Virtual memory manipulation and info
	 */

	// System basic pages
	static const size_t PageShift = 12;
	static const size_t PageSize = 1 << PageShift;
	// Superpage : 2MB
	static const size_t SuperpageShift = PageShift + 9;
	static const size_t SuperpageSize = 1 << SuperpageShift;
	static const size_t SuperpagePageNB = 1 << (SuperpageShift - PageShift);

	static_assert (sizeof (void *) == 8, "64 bit arch required");
	static_assert (SuperpageSize > PageSize, "SuperpageSize <= PageSize");
	static void runtime_asserts (void) { ASSERT_STD (sysconf (_SC_PAGESIZE) == PageSize); }

	static int map_noexcept (Ptr page_start, size_t size) noexcept {
		void * p = mmap (page_start, size, PROT_READ | PROT_WRITE | PROT_EXEC,
		                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		if (p == MAP_FAILED || p != page_start)
			return -1;
		else
			return 0;
	}
	static int unmap_noexcept (Ptr page_start, size_t size) noexcept {
		return munmap (page_start, size);
	}
	static int discard_noexcept (Ptr page_start, size_t size) noexcept {
		return madvise (page_start, size, MADV_DONTNEED);
	}

	static void map (Ptr page_start, size_t size) {
		if (map_noexcept (page_start, size) != 0)
			throw std::system_error (errno, std::system_category (), "mmap fixed");
	}
	static void unmap (Ptr page_start, size_t size) {
		if (unmap_noexcept (page_start, size) != 0)
			throw std::system_error (errno, std::system_category (), "munmap");
	}
	static void discard (Ptr page_start, size_t size) {
		if (discard_noexcept (page_start, size) != 0)
			throw std::system_error (errno, std::system_category (), "madvise");
	}
};

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
	size_t node_area_end_superpage_num (int node) const {
		return node_area_start_superpage_num (node + 1);
	}
	size_t local_area_start_superpage_num () const {
		return node_area_start_superpage_num (local_node);
	}
	size_t local_area_end_superpage_num () const { return node_area_end_superpage_num (local_node); }
};

#endif
