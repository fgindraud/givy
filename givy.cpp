/* Givy lib cpp file
 *
 * defines interface functions (C-like)
 */
#include <cstdlib>

#include "reporting.h"
#include "types.h"
#include "gas_space.h"
#include "allocator.h"
#include "network.h"
#include "givy.h"

namespace Givy {
namespace {
	// Bootstrap allocator
	Allocator::Bootstrap bootstrap_allocator;

	// Main heap is default initialized before main
	Allocator::MainHeap main_heap;

	/* Thread local heaps.
	 * One will be constructed for each thread, referencing the main heap.
	 * It will be destroyed at thread exit.
	 */
	thread_local Allocator::ThreadLocalHeap thread_heap{main_heap};

	//
	Optional<Gas::Space> gas_space;
	// Network network_interface{layout};
}

void init (int & argc, char **& argv) {
	// Givy::init_network (argc, argv);
	(void) argc;
	(void) argv;

	size_t nb_node = 1;
	size_t local_node = 0;

	auto base_ptr = Ptr (0x4000'0000'0000);
	gas_space.construct (base_ptr, 100 * VMem::superpage_size, nb_node, local_node, bootstrap_allocator);

	main_heap.enable_gas (gas_space.value ());
}

Block allocate (size_t size, size_t align) {
	return thread_heap.allocate (size, align);
}
void deallocate (Block blk) {
	thread_heap.deallocate (blk);
}
void deallocate (Ptr ptr) {
	thread_heap.deallocate (ptr);
}

}
