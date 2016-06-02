#pragma once
#ifndef TYPES_H
#define TYPES_H

#include <cstdint> // uintN_t
#include <utility> // std::forward

#include "math.h"
#include "reporting.h" // for container

namespace Givy {

namespace Detail {
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
using BoundUint = typename Detail::UintSelector<Math::representation_bits (MaxValue)>::Type;

/* Class container, to allow manual build/destruction.
 * Doesn't track built state.
 */
template <typename T> class Constructible {
private:
	union {
		// Using union to have uninitialized class
		T stored;
	};

public:
	Constructible () {}
	Constructible (const Constructible &) = delete;
	Constructible (Constructible &&) = delete;
	Constructible & operator=(const Constructible &) = delete;
	Constructible & operator=(Constructible &&) = delete;
	~Constructible () {}

	template <typename... Args> void construct (Args &&... args) {
		::new (&stored) T (std::forward<Args> (args)...);
	}
	void destruct (void) { stored.~T (); }

	T & object (void) { return stored; }
	const T & object (void) const { return stored; }
	T & operator*(void) { return object (); }
	const T & operator*(void) const { return object (); }
	T * operator->(void) { return &object (); }
	const T * operator->(void) const { return &object (); }
};
}

#endif
