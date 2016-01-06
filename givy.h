#ifndef GIVY_H
#define GIVY_H

#include "reporting.h"
#include "gas_layout.h"
#include "allocator.h"

namespace Givy {
	namespace GlobalInstance {
		// Called at program startup (before main)
		GasLayout init (void) {
			VMem::runtime_asserts ();

			/* Determining program break, placing Givy after (+ some pages in the middle) */
			return {Ptr (sbrk (0)) + 1000 * VMem::PageSize, // start
			        100 * VMem::SuperpageSize,               // space_by_node
			        4,                                      // nb_node
			        0};                                     // local node
		}

		/* Due to c++ thread_local semantics, ThreadLocalHeap objects will be constructed/destructed
		 * at every thread start/end
		 */
		Allocator::MainHeap main_heap{init ()};
		thread_local Allocator::ThreadLocalHeap thread_heap{main_heap};

		/* Interface
		*/
		Block allocate (size_t size, size_t align) { return thread_heap.allocate (size, align); }
		void deallocate (Ptr ptr) { thread_heap.deallocate (ptr); }
		void deallocate (Block blk) { thread_heap.deallocate (blk); }

#ifdef ASSERT_LEVEL_SAFE
		void print (bool print_main_heap = true) { thread_heap.print (print_main_heap); }
#endif
	}
}

#endif
