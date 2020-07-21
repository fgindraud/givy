#pragma once
#ifndef GIVY_BITMASK_H
#define GIVY_BITMASK_H

#include <limits> // numeric_limits
#include <cstdint> // uintN_t

#include "reporting.h"
#include "range.h"

namespace Givy {

template <typename IntType> struct BitMask {
	/* Bitmask manipulation functions, parametrized by the integer type used.
	 * We index bits in LSB->MSB order.
	 */
	static_assert (std::numeric_limits<IntType>::is_integer, "IntType must be an integer");
	static_assert (!std::numeric_limits<IntType>::is_signed, "IntType must be unsigned");

	static constexpr size_t bits = std::numeric_limits<IntType>::digits;

	static constexpr IntType zeros (void) { return 0; }
	static constexpr IntType ones (void) { return std::numeric_limits<IntType>::max (); }
	static constexpr IntType one (void) { return 0x1; }

	static constexpr IntType lsb_ones (size_t nb) {
		// require : 0 <= nb <= bits
		ASSERT_SAFE (nb <= bits);
		// return : 1s in [0, nb[ - 0s in [nb, bits[
		return (nb == 0) ? 0 : (ones () >> (bits - nb));
	}
	static constexpr IntType msb_ones (size_t nb) {
		// require : 0 <= nb <= bits
		ASSERT_SAFE (nb <= bits);
		// return : 0s in [0, nb[ - 1s in [nb, bits[
		return (nb == 0) ? 0 : (ones () << (bits - nb));
	}
	static constexpr IntType window_size (size_t start, size_t size) {
		// require : 0 <= start && start + size <= bits
		ASSERT_SAFE (start + size <= bits);
		// return : 0s in [0, start[ - 1s in [start, start + size[ - 0s in [start + size, bits[
		return (start == bits) ? 0 : (lsb_ones (size) << start);
	}
	static constexpr IntType window_bound (size_t start, size_t end) {
		// require : 0 <= start <= end <= bits
		ASSERT_SAFE (start <= end);
		ASSERT_SAFE (end <= bits);
		// return : 0s in [0, start[ - 1s in [start, end[ - 0s in [end, bits[
		return window_size (start, end - start);
	}

	static constexpr bool is_set (IntType i, size_t bit) {
		// require : 0 <= bit < bits
		ASSERT_SAFE (bit < bits);
		return (one () << bit) & i;
	}
	static constexpr size_t count_msb_zeros (IntType c) {
		size_t b = bits;
		for (; c; c >>= 1, --b)
			;
		return b;
	}
	static constexpr size_t count_lsb_zeros (IntType c) {
		size_t b = bits;
		for (; c; c <<= 1, --b)
			;
		return b;
	}
	static constexpr size_t count_zeros (IntType c) {
		size_t b = 0;
		for (; c; c >>= 1)
			if (c & one () == zeros ())
				b++;
		return b;
	}
	static constexpr size_t count_msb_ones (IntType c) { return count_msb_zeros (~c); }

	static inline size_t find_zero_subsequence (IntType searched, size_t len, size_t from_bit,
	                                     size_t up_to_bit) {
		// require : 0 <= from_bit <= up_to_bit <= bits
		ASSERT_SAFE (from_bit <= up_to_bit);
		ASSERT_SAFE (up_to_bit <= bits);
		// require : from_bit + len <= up_to_bit
		ASSERT_SAFE (from_bit + len <= up_to_bit);
		// return : offset of first 0s sequence of length 'len' in 'searched' (in [from_bit, up_to_bit[)
		size_t window_end = from_bit + len;
		IntType bit_window = window_bound (from_bit, window_end);
		while (window_end <= up_to_bit) {
			if ((searched & bit_window) == zeros ())
				return window_end - len; // found
			bit_window <<= 1;
			window_end++;
		}
		return bits; // not found
	}

	static constexpr size_t find_previous_zero (IntType c, size_t pos) {
		// require : 0 <= pos < bits
		ASSERT_SAFE (pos < bits);
		// return : offset of last 0 in c:[0, pos], or bits if none was found
		c <<= (bits - 1) - pos; // Shift so that 'pos' bit is now the msb
		size_t distance_to_prev_zero = count_msb_ones (c);
		if (distance_to_prev_zero > pos)
			return bits;
		else
			return pos - distance_to_prev_zero;
	}

	static inline const char * str (IntType c) {
		// return : static buffer to string representing c (bits)
		static char buffer[bits + 1] = {'\0'};
		for (auto i : range (bits)) {
			buffer[i] = (one () & c) ? '1' : '0';
			c >>= 1;
		}
		return buffer;
	}
};

#if defined(__GNUC__) || defined(__clang__)
// For gcc and clang, define overloads using faster builtins
template <> constexpr size_t BitMask<unsigned int>::count_msb_zeros (unsigned int c) {
	return (c > 0) ? __builtin_clz (c) : bits;
}
template <> constexpr size_t BitMask<unsigned long>::count_msb_zeros (unsigned long c) {
	return (c > 0) ? __builtin_clzl (c) : bits;
}
template <> constexpr size_t BitMask<unsigned long long>::count_msb_zeros (unsigned long long c) {
	return (c > 0) ? __builtin_clzll (c) : bits;
}
template <> constexpr size_t BitMask<unsigned int>::count_lsb_zeros (unsigned int c) {
	return (c > 0) ? __builtin_ctz (c) : bits;
}
template <> constexpr size_t BitMask<unsigned long>::count_lsb_zeros (unsigned long c) {
	return (c > 0) ? __builtin_ctzl (c) : bits;
}
template <> constexpr size_t BitMask<unsigned long long>::count_lsb_zeros (unsigned long long c) {
	return (c > 0) ? __builtin_ctzll (c) : bits;
}
template <> constexpr size_t BitMask<unsigned int>::count_zeros (unsigned int c) {
	return bits - __builtin_popcount (c);
}
template <> constexpr size_t BitMask<unsigned long>::count_zeros (unsigned long c) {
	return bits - __builtin_popcountl (c);
}
template <> constexpr size_t BitMask<unsigned long long>::count_zeros (unsigned long long c) {
	return bits - __builtin_popcountll (c);
}
#endif // GCC or Clang
}

#endif
