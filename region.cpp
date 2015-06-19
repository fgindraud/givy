#include <cstdint> // uintN_t uintptr_t
#include <cstddef> // size_t
#include <climits> // CHAR_BIT
#include <system_error>
#include <cerrno>

#include <atomic>

// system specific
#include <sys/mman.h>
#include <unistd.h>

// DEBUG
#include <iostream> 
#include <cstdio>

/* Utils
 */

struct Math {
	static constexpr bool is_power_2 (size_t x) noexcept { return x > 0 && (x & (x - 1)) == 0; }
	static constexpr size_t divide_up (size_t n, size_t div) noexcept { return (n + div - 1) / div; }
};

struct Ptr {
	uintptr_t ptr_;

	constexpr Ptr (uintptr_t ptr) noexcept : ptr_ (ptr) {}
	template<typename T> constexpr Ptr (T* ptr) noexcept : ptr_ (reinterpret_cast<uintptr_t> (ptr)) {}
	template<typename T> constexpr T as (void) const noexcept { return reinterpret_cast<T> (ptr_); }
	template<typename T> constexpr operator T* (void) const noexcept { return as<T*> (); }

	constexpr Ptr add (size_t off) const noexcept { return Ptr (ptr_ + off); }
	constexpr Ptr sub (size_t off) const noexcept { return Ptr (ptr_ - off); }
	constexpr Ptr lshift (size_t sh) const noexcept { return Ptr (ptr_ << sh); }
	constexpr Ptr rshift (size_t sh) const noexcept { return Ptr (ptr_ >> sh); }

	// align : backward ; align_up : forward	
	constexpr Ptr align (size_t al) const noexcept { return Ptr ((ptr_ / al) * al); }
	constexpr Ptr align_up (size_t al) const noexcept { return add (al - 1).align (al); }
	constexpr bool is_aligned (size_t al) const noexcept { return ptr_ % al == 0; }

	// align for power of 2 alignments
	constexpr Ptr align2 (size_t al) const noexcept { return Ptr (ptr_ & ~(al - 1)); }
	constexpr Ptr align2_up (size_t al) const noexcept { return add (al - 1).align2 (al); }
};

/* Mapping
 */
struct VMem {
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

template< typename Alloc >
struct SuperpageTracker {
	using IntType = std::intmax_t;
	using AtomicIntType = std::atomic< IntType >;
	enum {
		BitsPerInt = sizeof (IntType) * CHAR_BIT,
		BitsPerSuperpage = 2,
	};

	Alloc & allocator_; 
	size_t array_size_;
	AtomicIntType * int_array_;

	SuperpageTracker (size_t superpage_nb, Alloc & allocator) :
		allocator_ (allocator),
		array_size_ (Math::divide_up (superpage_nb, BitsPerInt / BitsPerSuperpage))
	{
		int_array_ = allocator_.allocate (array_size_ * sizeof (AtomicIntType));
		for (size_t it = 0; it < array_size_; ++it)
			new (&int_array_[it]) AtomicIntType (0);
	}
	~SuperpageTracker () {
		allocator_.deallocate (int_array_);
	}
};

struct Superpage {
	/* Aligned to 2MB : kernel will use hugepage if it can */
};

struct RegionHeader {
	// defs
	enum class Type {
		Blah = 0
	};
	struct Splitted {
	};
};

struct Gas {
	int nb_nodes_;
	int node_id_;
	int nb_threads_;

	// gas 
	Ptr * gas_start_;
	size_t gas_space_by_node_;

	/***** Interface *******/

	/* Region creation/destruction (equivalent to malloc / posix_memalign / free)
	 * Take a thread_id parameter, will be initialised if -1
	 */
	void * region_create (size_t size, int & tid); // local
	void * region_create_aligned (size_t size, size_t alignment, int & tid); // local
	void region_destroy (void * ptr, int & tid); // may call gas if remote copies

	/* Split/merge
	 * always take the pointer to the block itself
	 */
	void split (void * ptr, size_t nb_piece); // will notify remotes
	void merge (void * ptr); // will notify remotes

	/* Extended interface: "lazy_create"
	 * Will map to normal create for small blocks.
	 * Delayed malloc for bigger ones (multi superpages)
	 */
	void * region_create_noalloc (size_t size, int & tid); // local
	void region_delayed_alloc (void * ptr); //

	/* Threaded model coherence interface
	 * Assumes DRF accesses
	 */
	enum AccessMode { NoAccess, ReadOnly, ReadWrite };
	void region_ensure_accesses (void ** regions, AccessMode * modes, int nb_region);

	/***** Internals ******/

	RegionHeader * region_header (void * ptr);
};


thread_local int tid = -1;

struct Allocator {
	Ptr allocate (size_t size) { return new uint8_t[size]; }
	void deallocate (Ptr ptr) { delete[] ptr.as< uint8_t * > (); }
};

struct BumpPointerAlloc {
	Ptr start_;
	Ptr end_;
	Ptr end_mapped_;

	BumpPointerAlloc (Ptr start) :
		start_ (start),
		end_ (start),
		end_mapped_ (start)
  	{
		if (not start.is_aligned (VMem::PageSize))
			throw std::runtime_error ("BumpPointerAlloc(): start not aligned");
	}
	~BumpPointerAlloc () {

	}

	template<typename T>
	T * allocate (size_t nb) {
		return nullptr; alignof (T); sizeof (T);
	}
	void deallocate (Ptr ptr) { /* nothing to do */ }
};

struct BackwardBumpPointerAlloc {
	

	Ptr allocate (size_t size, size_t align) { return new uint8_t[size]; }
	void deallocate (Ptr ptr) { delete[] ptr.as< uint8_t * > (); }
};

void wtf (Ptr p) __attribute__ ((noinline));
void wtf (Ptr p) {
	printf ("%p\n", p.as<void*> ());
}

int main (void) {
	VMem::runtime_asserts ();

	Allocator std_alloc;

	SuperpageTracker<Allocator> tracker (200, std_alloc);

	char blah;
	wtf (&blah);

	return 0;
}

