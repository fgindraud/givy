#pragma once
#ifndef GIVY_TYPES_H
#define GIVY_TYPES_H

#include <cstdint> // uintN_t
#include <utility> // std::forward

#include "math.h"
#include "reporting.h" // for container

namespace Givy {

namespace Impl {
	template <size_t N> struct UintSelector {
		static_assert (N <= 64, "N is above maximum supported int size");
		using Type = typename UintSelector<N + 1>::Type;
	};
	template <> struct UintSelector<8> { using Type = uint8_t; };
	template <> struct UintSelector<16> { using Type = uint16_t; };
	template <> struct UintSelector<32> { using Type = uint32_t; };
	template <> struct UintSelector<64> { using Type = uint64_t; };
}

/* BoundUint<N> provides the unsigned integer type that can represent [0, N]
*/
template <size_t MaxValue>
using BoundUint = typename Impl::UintSelector<Math::representation_bits (MaxValue)>::Type;

/* Manual construction/destruction.
 */
template <typename T, typename... Args> inline void construct (T & t, Args &&... args) {
	new (&t) T (std::forward<Args> (args)...);
}
template <typename T> inline void destruct (T & t) {
	t.~T ();
}
}

#endif
