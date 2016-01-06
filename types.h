#ifndef TYPES_H
#define TYPES_H

#include <cstdint> // uintN_t

#include "math.h"

namespace Givy {

namespace BoundIntDetail {
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
using BoundUint = typename BoundIntDetail::UintSelector<Math::representation_bits (MaxValue)>::Type;
}

#endif
