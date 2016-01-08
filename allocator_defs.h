#ifndef ALLOCATOR_DEFS_H
#define ALLOCATOR_DEFS_H

namespace Givy {
namespace Allocator {

	enum class MemoryType : uint8_t {
		/* Enum type used to represent the usage of a piece of memory.
	   */

		// Allocations
		Small,  // Under the page size
		Medium, // Between page and superpage size
		Huge,   // Above superpage size

		// Internal definitions
		Unused,  // Unused space, can be allocated
		Reserved // Used internally, not available for allocation
	};
}
}

#endif
