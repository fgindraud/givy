#pragma once
#ifndef ARRAY_H
#define ARRAY_H

#include <array>
#include <utility>
#include <algorithm>
#include <type_traits>

#include "block.h"
#include "range.h"

namespace Givy {

/* C-like constant size array type
 */
template <typename T, size_t N> using StaticArray = std::array<T, N>;

/* Build a StaticArray by taking the result of a Callable f over integers [0, N[
 */
template <typename Func, size_t... I>
constexpr StaticArray<std::decay_t<std::result_of_t<Func (size_t)>>, sizeof...(I)>
static_array_from_generator_aux (Func && f, std::index_sequence<I...>) {
	return {f (I)...};
}
template <size_t N, typename Func>
constexpr decltype (auto) static_array_from_generator (Func && f) {
	return static_array_from_generator_aux (std::forward<Func> (f), std::make_index_sequence<N> ());
}

/* Build an StaticArray by taking the result of a Callable f over a second StaticArray
 */
template <typename T, typename Func, size_t... I>
constexpr StaticArray<std::decay_t<std::result_of_t<Func (T)>>, sizeof...(I)>
static_array_map_aux (const StaticArray<T, sizeof...(I)> & a, Func && f,
                      std::index_sequence<I...>) {
	return {f (std::get<I> (a))...};
}
template <typename T, size_t N, typename Func>
constexpr decltype (auto) static_array_map (const StaticArray<T, N> & a, Func && f) {
	return static_array_map_aux (a, std::forward<Func> (f), std::make_index_sequence<N> ());
}

/* Return the max element of an StaticArray.
 */
template <typename T, size_t... I>
constexpr T static_array_max_aux (const StaticArray<T, sizeof...(I)> & a,
                                  std::index_sequence<I...>) {
	return std::max ({std::get<I> (a)...});
}
template <typename T, size_t N>
constexpr decltype (auto) static_array_max (const StaticArray<T, N> & a) {
	return static_array_max_aux (a, std::make_index_sequence<N> ());
}

/* Dynamic non-resizable heap array supporting custom allocators
 */
template <typename T, typename Alloc> class FixedArray {
private:
	Alloc & allocator;
	size_t length;
	Block memory;

	T * array (void) { return static_cast<T *> (memory.ptr); }
	const T * array (void) const { return static_cast<const T *> (memory.ptr); }

public:
	template <typename... Args>
	FixedArray (size_t size_, Alloc & allocator_, Args &&... args)
	    : allocator (allocator_), length (size_) {
		// Allocate
		ASSERT_SAFE (length > 0);
		memory = allocator.allocate (length * sizeof (T), alignof (T));
		ASSERT_SAFE (memory.ptr != nullptr);

		// Construct
		for (auto i : range (size ()))
			new (&(array ()[i])) T (args...);
	}
	~FixedArray () {
		// Destruct
		for (auto i : range (size ()))
			array ()[i].~T ();

		// Deallocate
		allocator.deallocate (memory);
	}

	// Prevent copy/move
	FixedArray (const FixedArray &) = delete;
	FixedArray & operator=(const FixedArray &) = delete;
	FixedArray (FixedArray &&) = delete;
	FixedArray & operator=(FixedArray &&) = delete;

	// Size and access
	size_t size (void) const { return length; }
	const T & operator[](size_t i) const {
		ASSERT_SAFE (i < size ());
		return array ()[i];
	}
	T & operator[](size_t i) {
		ASSERT_SAFE (i < size ());
		return array ()[i];
	}
};

/* Computing index of elements in arrays from pointers
 */
template <typename T> inline size_t array_index (const T * t, const T * a) {
	return t - a;
}
template <typename T> inline size_t array_index (const T & t, const T * a) {
	return array_index (&t, a);
}
template <typename T, size_t N>
inline size_t array_index (const T * t, const StaticArray<T, N> & a) {
	return array_index (t, a.data ());
}
template <typename T, size_t N>
inline size_t array_index (const T & t, const StaticArray<T, N> & a) {
	return array_index (&t, a);
}
}

#endif
