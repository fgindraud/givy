#ifndef UTILITY_H
#define UTILITY_H

#include <system_error> // mmap/munmap error exceptions
#include <cerrno>

// system specific
#include <sys/mman.h>
#include <unistd.h>

#include "base_defs.h"
#include "assert_level.h"

namespace Givy {

/* ------------------------ BitMask management -------------------- */

template <typename IntType> struct BitMask {
	/* Bitmask manipulation functions, parametrized by the integer type used.
	 * We index bits in LSB->MSB order.
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
		// Use with caution, unsafe buffer
		static char buffer[Bits + 1] = {'\0'};
		for (size_t i = 0; i < Bits; ++i) {
			buffer[i] = (IntType (0x1) & c) ? '1' : '0';
			c <<= 1;
		}
		return buffer;
	}
};

#if defined(__GNUC__) || defined(__clang__)
// For gcc and clang, define overloads using faster builtins
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
template <> constexpr size_t BitMask<unsigned long long>::count_msb_zeros (unsigned long long c) noexcept {
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
template <> constexpr size_t BitMask<unsigned long long>::count_lsb_zeros (unsigned long long c) noexcept {
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
template <> constexpr size_t BitMask<unsigned long long>::count_zeros (unsigned long long c) noexcept {
	return Bits - __builtin_popcountll (c);
}
#endif // GCC or Clang

/* ------------------------------ Low level memory management ---------------------- */

namespace VMem {
	static inline int map_noexcept (Ptr page_start, size_t size) noexcept {
		void * p =
		    mmap (page_start, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		if (p == MAP_FAILED || p != page_start)
			return -1;
		else
			return 0;
	}
	static inline int unmap_noexcept (Ptr page_start, size_t size) noexcept { return munmap (page_start, size); }
	static inline int discard_noexcept (Ptr page_start, size_t size) noexcept {
		return madvise (page_start, size, MADV_DONTNEED);
	}

	static inline void map (Ptr page_start, size_t size) {
		if (map_noexcept (page_start, size) != 0)
			throw std::system_error (errno, std::system_category (), "mmap fixed");
	}
	static inline void unmap (Ptr page_start, size_t size) {
		if (unmap_noexcept (page_start, size) != 0)
			throw std::system_error (errno, std::system_category (), "munmap");
	}
	static inline void discard (Ptr page_start, size_t size) {
		if (discard_noexcept (page_start, size) != 0)
			throw std::system_error (errno, std::system_category (), "madvise");
	}
}

} // namespace Givy

#endif
