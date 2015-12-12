#define ASSERT_LEVEL_SAFE

#include "allocator.h"

namespace Givy {
namespace Allocator {
	namespace GlobalInstance {
		// Called at program startup (before main)
		GasLayout init (void) {
			VMem::runtime_asserts ();

			/* Determining program break, placing Givy after (+ some pages in the middle) */
			return {Ptr (sbrk (0)) + 1000 * VMem::PageSize, // start
			        10 * VMem::SuperpageSize,               // space_by_node
			        4,                                      // nb_node
			        0};                                     // local node
		}

		/* Due to c++ thread_local semantics, ThreadLocalHeap objects will be constructed/destructed
		 * at every thread start/end
		 */
		MainHeap main_heap{init ()};
		thread_local ThreadLocalHeap thread_heap{main_heap};

		/* Interface
		*/
		Block allocate (size_t size, size_t align) { return thread_heap.allocate (size, align); }
		void deallocate (Ptr ptr) { thread_heap.deallocate (ptr); }
		void deallocate (Block blk) { thread_heap.deallocate (blk); }

		void print (void) { thread_heap.print (false); }
	}
}
}

#include <iostream>

void sep (const char * txt = "") {
	printf ("#################### %s #####################\n", txt);
}

int main (void) {
	namespace G = Givy::Allocator::GlobalInstance;

	auto p1 = G::allocate (0xF356, 1);
	auto p2 = G::allocate (53, 1);
	sep ("A[12]");
	G::print ();
	G::deallocate (p1);
	sep ("A[2]");
	G::print ();
	auto p3 = G::allocate (4096, 1);
	sep ("A[23]");
	G::print ();
	G::deallocate (p2);
	G::deallocate (p3);
	sep ("A[]");
	G::print ();
	return 0;
}
