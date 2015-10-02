#ifndef BASE_DEFS_H
#define BASE_DEFS_H

#include <cstdint> // uintN_t uintptr_t
#include <cstddef> // size_t std::nullptr_t
#include <cerrno>
#include <system_error> // mmap/munmap error exceptions

#include <utility> // std::forward

// system specific
#include <sys/mman.h>
#include <unistd.h>

struct Math {
	//static constexpr bool is_power_2 (size_t x) noexcept { return x > 0 && (x & (x - 1)) == 0; }
	static constexpr size_t divide_up (size_t n, size_t div) noexcept { return (n + div - 1) / div; }
};

struct Ptr {
	/* Raw pointer like object ; allows cleaner raw pointer manipulation
	 */
	uintptr_t ptr_;

	explicit constexpr Ptr (uintptr_t ptr) noexcept : ptr_ (ptr) {}
	explicit constexpr Ptr (std::nullptr_t ptr) noexcept : ptr_ (0) {}
	template<typename T> explicit constexpr Ptr (T* ptr) noexcept : ptr_ (reinterpret_cast<uintptr_t> (ptr)) {}
	
	template<typename T> constexpr T as (void) const noexcept { return reinterpret_cast<T> (ptr_); }
	template<typename T> constexpr operator T* (void) const noexcept { return as<T*> (); }

	constexpr Ptr add (size_t off) const noexcept { return Ptr (ptr_ + off); }
	constexpr Ptr sub (size_t off) const noexcept { return Ptr (ptr_ - off); }
	constexpr Ptr lshift (size_t sh) const noexcept { return Ptr (ptr_ << sh); }
	constexpr Ptr rshift (size_t sh) const noexcept { return Ptr (ptr_ >> sh); }

	constexpr Ptr operator+ (size_t off) const noexcept { return add (off); }
	constexpr Ptr & operator+= (size_t off) noexcept { return *this = add (off); }
	constexpr Ptr operator- (size_t off) const noexcept { return sub (off); }
	constexpr Ptr & operator-= (size_t off) noexcept { return *this = sub (off); }

	// align : backward ; align_up : forward	
	constexpr Ptr align (size_t al) const noexcept { return Ptr ((ptr_ / al) * al); }
	constexpr Ptr align_up (size_t al) const noexcept { return add (al - 1).align (al); }
	constexpr bool is_aligned (size_t al) const noexcept { return ptr_ % al == 0; }

	// Compute diff
	size_t operator- (Ptr other) const noexcept { return ptr_ - other.ptr_; }
};

constexpr bool operator< (Ptr lhs, Ptr rhs) noexcept { return lhs.ptr_ < rhs.ptr_; }
constexpr bool operator> (Ptr lhs, Ptr rhs) noexcept { return lhs.ptr_ > rhs.ptr_; }
constexpr bool operator<= (Ptr lhs, Ptr rhs) noexcept { return lhs.ptr_ <= rhs.ptr_; }
constexpr bool operator>= (Ptr lhs, Ptr rhs) noexcept { return lhs.ptr_ >= rhs.ptr_; }
constexpr bool operator== (Ptr lhs, Ptr rhs) noexcept { return lhs.ptr_ == rhs.ptr_; }
constexpr bool operator!= (Ptr lhs, Ptr rhs) noexcept { return lhs.ptr_ != rhs.ptr_; }


struct VMem {
	/* Virtual memory manipulation and info
	 */

	enum: size_t {
		// System basic pages
		PageShift = 12,
		PageSize = 1 << PageShift,
		// Superpage : 2MB
		SuperpageShift = PageShift + 9,
		SuperpageSize = 1 << SuperpageShift,
		SuperpagePageNB = 1 << (SuperpageShift - PageShift)
	};

	static_assert (sizeof (void *) == 8, "64 bit arch required");
	static void runtime_asserts (void) {
		if (sysconf (_SC_PAGESIZE) != PageSize)
			throw std::runtime_error ("invalid pagesize");
	}

	static void map (Ptr page_start, size_t size) {
		void * p = mmap (page_start, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		if (p == MAP_FAILED || p != page_start)
			throw std::system_error (errno, std::system_category (), "mmap fixed");
	}
	static void unmap (Ptr page_start, size_t size) {
		if (munmap (page_start, size) != 0)
			throw std::system_error (errno, std::system_category (), "munmap");
	}
	static void discard (Ptr page_start, size_t size) {
		if (madvise (page_start, size, MADV_DONTNEED) != 0)
			throw std::system_error (errno, std::system_category (), "madvise");
	}
};

#endif
