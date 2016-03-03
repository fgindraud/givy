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
 * Doesn't check for anything, so unsafe.
 */
template <typename T> class ManualConstruct {
private:
	union {
		// Using union to have uninitialized class
		T stored;
	};

public:
	ManualConstruct () {}
	ManualConstruct (const ManualConstruct &) = delete;
	ManualConstruct (ManualConstruct &&) = delete;
	ManualConstruct & operator=(const ManualConstruct &) = delete;
	ManualConstruct & operator=(ManualConstruct &&) = delete;
	~ManualConstruct () {}

	template <typename... Args> void construct (Args &&... args) {
		::new (&stored) T (std::forward<Args> (args)...);
	}
	void destruct (void) { stored.~T (); }

	T & object (void) { return stored; }
	const T & object (void) const { return stored; }
};

/* Optional : manually constructed object, with boolean to track state.
 */
template <typename T> class Optional {
private:
	bool constructed{false};
	ManualConstruct<T> storage;

public:
	Optional () = default;
	~Optional () {
		if (constructed)
			storage.destruct ();
	}

	template <typename... Args> void construct (Args &&... args) {
		ASSERT_SAFE (!constructed);
		storage.construct (std::forward<Args> (args)...);
		constructed = true;
	}
	void destruct (void) {
		ASSERT_SAFE (constructed);
		storage.destruct ();
		constructed = false;
	}

	bool is_constructed (void) const { return constructed; }
	operator bool (void) const { return is_constructed (); }

	T & object (void) {
		ASSERT_SAFE (constructed);
		return storage.object ();
	}
	const T & object (void) const {
		ASSERT_SAFE (constructed);
		return storage.object ();
	}
	T & operator*(void) { return object (); }
	const T & operator*(void) const { return object (); }
	T * operator->(void) { return &object (); }
	const T * operator->(void) const { return &object (); }
};
}

#endif
