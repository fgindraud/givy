#ifndef ARRAY_H
#define ARRAY_H

#include <array>
#include <iterator>

namespace Givy {

template <typename T, typename Alloc> class FixedArray {
	/* Dynamic non-resizable array supporting custom allocators
	 */
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
