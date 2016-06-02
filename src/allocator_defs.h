#pragma once
#ifndef ALLOCATOR_DEFS_H
#define ALLOCATOR_DEFS_H

namespace Givy {
namespace Allocator {

	enum class MemoryType : uint8_t {
		/* Enum type used to represent the usage of a piece of memory.
	   */

		// Allocations
		small,  // Under the page size
		medium, // Between page and superpage size
		huge,   // Above superpage size

		// Internal definitions
		unused,  // Unused space, can be allocated
		reserved // Used internally, not available for allocation
	};
}
}

#endif
