#pragma once
#ifndef POINTER_H
#define POINTER_H

#include <cstdint> // uintN_t uintptr_t
#include <cstddef> // size_t std::nullptr_t

#include "math.h"
#include "reporting.h"

namespace Givy {

using std::size_t;

struct Ptr {
	/* Raw pointer like object ; allows cleaner raw pointer manipulation
	 */
	uintptr_t p;

	Ptr () = default; // Unitialized
	Ptr (std::nullptr_t) : p (0) {}
	explicit Ptr (uintptr_t ptr) : p (ptr) {}
	Ptr (const void * ptr) : p (reinterpret_cast<uintptr_t> (ptr)) {}

	template <typename T> T as (void) const { return reinterpret_cast<T> (p); }
	template <typename T> T & as_ref (void) const {
		ASSERT_SAFE (p != 0);
		return *as<T *> ();
	}
	operator void *(void) const { return as<void *> (); }

	Ptr add (size_t off) const { return Ptr (p + off); }
	Ptr sub (size_t off) const { return Ptr (p - off); }
	size_t sub (Ptr ptr) const { return p - ptr.p; }

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

inline Ptr operator+(Ptr lhs, size_t off) {
	return lhs.add (off);
}
inline Ptr operator+(size_t off, Ptr rhs) {
	return rhs.add (off);
}

inline bool operator<(Ptr lhs, Ptr rhs) {
	return lhs.p < rhs.p;
}
inline bool operator>(Ptr lhs, Ptr rhs) {
	return lhs.p > rhs.p;
}
inline bool operator<=(Ptr lhs, Ptr rhs) {
	return lhs.p <= rhs.p;
}
inline bool operator>=(Ptr lhs, Ptr rhs) {
	return lhs.p >= rhs.p;
}
inline bool operator==(Ptr lhs, Ptr rhs) {
	return lhs.p == rhs.p;
}
inline bool operator!=(Ptr lhs, Ptr rhs) {
	return lhs.p != rhs.p;
}
}

#endif
