#pragma once
#ifndef GIVY_SYSTEM_H
#define GIVY_SYSTEM_H

#include <unistd.h> // sysconf

#include "reporting.h"

namespace Givy {

namespace VMem {
	/* Virtual memory constants
	 */

	// System basic pages
	constexpr size_t page_shift = 12;
	constexpr size_t page_size = 1 << page_shift;
	// Superpage : 2MB
	constexpr size_t superpage_shift = page_shift + 9;
	constexpr size_t superpage_size = 1 << superpage_shift;
	constexpr size_t superpage_page_nb = 1 << (superpage_shift - page_shift);
	// Some checks
	static_assert (superpage_size > page_size, "superpage_size <= page_size");
	inline void runtime_asserts (void) { ASSERT_STD (sysconf (_SC_PAGESIZE) == page_size); }
}

}

#endif
