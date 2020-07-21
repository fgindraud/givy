#pragma once
#ifndef GIVY_MATH_H
#define GIVY_MATH_H

#include "bitmask.h"

#include <limits> // numeric_limits
#include <cstdint> // size_t

namespace Givy {

using std::size_t;

namespace Math {

	// Integer division and alignement

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
	
	/* Power of 2 manipulation
	 */

	template <typename T> constexpr bool is_power_of_2 (T x) {
		static_assert (std::numeric_limits<T>::is_integer, "T must be an integer");
		return x > 0 && (x & (x - 1)) == 0;
	}

	constexpr size_t log_2_inf (size_t x) {
		// require : 0 < x
		ASSERT_SAFE (0 < x);
		// return : log2(x) (rounded to lower)
		using B = BitMask<size_t>;
		return (B::bits - 1) - B::count_msb_zeros (x);
	}
	constexpr size_t log_2_sup (size_t x) {
		// require : 1 < x (due to implem)
		ASSERT_SAFE (1 < x);
		// return : log2(x) (rounded to upper)
		return log_2_inf (x - 1) + 1;
	}

	constexpr size_t round_up_as_power_of_2 (size_t x) { return size_t (1) << log_2_sup (x); }

	// Binary representation

	constexpr size_t representation_bits (size_t x) {
		/* Give the number of bits needed to represent x.
		 * By convention, if x is 0, answer 1.
		 */
		if (x == 0)
			return 1;
		else
			return log_2_inf (x) + 1;
	}

	template <typename IntType> constexpr bool can_represent (size_t n) {
		static_assert (std::numeric_limits<IntType>::is_integer, "IntType must be an integer");
		static_assert (!std::numeric_limits<IntType>::is_signed, "IntType must be unsigned");
		return n <= std::numeric_limits<IntType>::max ();
	}
}
}

#endif
