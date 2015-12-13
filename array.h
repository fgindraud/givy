#ifndef ARRAY_H
#define ARRAY_H

#include <array>
#include <utility>
#include <type_traits>
#include <iterator>

namespace Givy {

/* C-like constant size array type
 */
template <typename T, size_t N> using Array = std::array<T, N>;

/* Build a Array by taking the result of a Callable f over integers [0, N[
 */
template <typename Func, size_t... I>
constexpr Array<std::decay_t<std::result_of_t<Func (size_t)>>, sizeof...(I)>
array_from_generator_aux (Func && f, std::index_sequence<I...>) {
	return {std::forward<Func> (f) (I)...};
}
template <size_t N, typename Func> constexpr decltype (auto) array_from_generator (Func && f) {
	return array_from_generator_aux (std::forward<Func> (f), std::make_index_sequence<N> ());
}

/* Build an Array by taking the result of a Callable f over a second Array
 */
template <typename T, typename Func, size_t... I>
constexpr Array<std::decay_t<std::result_of_t<Func (T)>>, sizeof...(I)>
array_map_aux (const Array<T, sizeof...(I)> & a, Func && f, std::index_sequence<I...>) {
	return {std::forward<Func> (f) (std::get<I> (a))...};
}
template <typename T, size_t N, typename Func>
constexpr decltype (auto) array_map (const Array<T, N> & a, Func && f) {
	return array_map_aux (a, std::forward<Func> (f), std::make_index_sequence<N> ());
}

/* Return the max element of an Array.
 */
template <typename T, size_t... I>
constexpr T array_max_aux (const Array<T, sizeof...(I)> & a, std::index_sequence<I...>) {
	return std::max ({std::get<I> (a)...});
}
template <typename T, size_t N> constexpr decltype (auto) array_max (const Array<T, N> & a) {
	return array_max_aux (a, std::make_index_sequence<N> ());
}

/* Dynamic non-resizable heap array supporting custom allocators
 */
template <typename T, typename Alloc> class FixedArray {
private:
	Alloc & allocator;
	size_t length;
	Block memory;

	T * array (void) { return memory.ptr; }
	const T * array (void) const { return memory.ptr; }

public:
	template <typename... Args>
	FixedArray (size_t size_, Alloc & allocator_, Args &&... args)
	    : allocator (allocator_), length (size_) {
		// Allocate
		ASSERT_STD (size_ > 0);
		memory = allocator.allocate (length * sizeof (T), alignof (T));
		ASSERT_STD (memory.ptr != nullptr);

		// Construct
		for (size_t i = 0; i < size (); ++i)
			new (&(array ()[i])) T (args...);
	}
	~FixedArray () {
		// Destruct
		for (size_t i = 0; i < size (); ++i)
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
}

#endif
