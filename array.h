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

template <typename T, size_t N> class ConstSizeArray {
	/* Constant sized array (wrapper around C style array), like std::array.
	 * But with support to create iterators out of array cell references/pointers.
	 */
private:
	T storage[N];

public:
	constexpr size_t size (void) const { return N; }
	const T & operator[](size_t i) const {
		ASSERT_SAFE (i < size ());
		return storage[i];
	}
	T & operator[](size_t i) {
		ASSERT_SAFE (i < size ());
		return storage[i];
	}

	T & front (void) { return storage[0]; }
	T & back (void) { return storage[size () - 1]; }

	class iterator : public std::iterator<std::random_access_iterator_tag, T> {
	private:
		T * cell;

	public:
		using offset = typename std::iterator_traits<iterator>::difference_type;

		iterator (T * p = nullptr) : cell (p) {}
		iterator (T & ref) : cell (&ref) {}

		bool operator==(iterator other) const { return cell == other.cell; }
		bool operator!=(iterator other) const { return cell != other.cell; }
		bool operator<(iterator other) const { return cell < other.cell; }
		bool operator>(iterator other) const { return cell > other.cell; }
		bool operator<=(iterator other) const { return cell <= other.cell; }
		bool operator>=(iterator other) const { return cell >= other.cell; }

		iterator operator+(offset n) { return iterator (cell + n); }
		iterator operator-(offset n) { return iterator (cell - n); }
		iterator & operator+=(offset n) { return * this = *this + n; }
		iterator & operator-=(offset n) { return * this = *this - n; }
		iterator & operator++(void) { return cell += 1; }
		iterator & operator--(void) { return cell -= 1; }
		iterator operator++(int) {
			auto cpy = *this;
			++*this;
			return cpy;
		}
		iterator operator--(int) {
			auto cpy = *this;
			--*this;
			return cpy;
		}

		offset operator-(iterator other) { return cell - other.cell; }

		T & operator*(void) const { return *cell; }
		T * operator->(void) const { return cell; }
	};

	iterator begin (void) { return iterator (storage); }
	iterator end (void) { return begin () + size (); }
	size_t index (iterator it) { return it - begin (); }
};
}

#endif
