#ifndef MEMORY_MAPPING_H
#define MEMORY_MAPPING_H

#include <cerrno>
#include <sys/mman.h>
#include <unistd.h>

namespace Givy {
namespace VMem {
	static inline int map (Ptr page_start, size_t size) {
		void * p = mmap (page_start, size, PROT_READ | PROT_WRITE | PROT_EXEC,
		                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		if (p == MAP_FAILED || p != page_start)
			return -1;
		else
			return 0;
	}
	static inline int unmap (Ptr page_start, size_t size) { return munmap (page_start, size); }
	static inline int discard (Ptr page_start, size_t size) {
		return madvise (page_start, size, MADV_DONTNEED);
	}

	static inline void map_checked (Ptr page_start, size_t size) {
		int map_r = map (page_start, size);
		ASSERT_OPT (map_r == 0);
	}
	static inline void unmap_checked (Ptr page_start, size_t size) {
		int unmap_r = unmap (page_start, size);
		ASSERT_OPT (unmap_r == 0);
	}
	static inline void discard_checked (Ptr page_start, size_t size) {
		int discard_r = discard (page_start, size);
		ASSERT_OPT (discard_r == 0);
	}

	static inline Ptr map_anywhere (size_t size) {
		void * p = mmap (nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		ASSERT_OPT (p != MAP_FAILED);
		return Ptr (p);
	}
}
}

#endif
