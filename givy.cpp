/* Givy lib cpp file
 *
 * defines interface functions (C-like)
 */
#define ASSERT_LEVEL_SAFE

#include <cstdlib>

#include "givy.h"
#include "allocator.h"
#include "network.h"

namespace {

size_t env_num (const char * env_var) {
	const char * s = std::getenv (env_var);
	if (s == nullptr)
		return 0;
	else
		return static_cast<size_t> (std::strtoull (s, nullptr, 10));
}

Givy::GasLayout main_heap_init (void) {
	Givy::VMem::runtime_asserts ();

	// Node indication
	size_t nb_node = env_num ("GIVY_NB_NODE");
	ASSERT_STD (nb_node > 0);
	size_t node_id = env_num ("GIVY_NODE_ID");
	ASSERT_STD (node_id < nb_node);
	DEBUG_TEXT ("[givy][%zu/%zu] Started\n", node_id, nb_node);

	/* Arbitrary GAS placement (must be constant across nodes).
	 * Linux prevents allocation in upper x86_64 address space, so only [0, 0x7fff'ffff'ffff] is
	 * usable.
	 * Even with ASLR, heap & custom memory mapping seems always placed near the start, and shared
	 * library + stack near the end.
	 * I choose to place the GAS in the middle, which should avoid collisions.
	 */
	auto gas_place = Ptr (0x4000'0000'0000);

	return {gas_place,                       // start
	        100 * Givy::VMem::SuperpageSize, // space_by_node (~200M)
	        nb_node,                         // nb_node
	        node_id};                        // local node
}

/* Due to c++ thread_local semantics, ThreadLocalHeap objects will be constructed/destructed
 * at every thread start/end
 */
Givy::GasLayout layout{main_heap_init ()};
Givy::Allocator::MainHeap main_heap{layout};
thread_local Givy::Allocator::ThreadLocalHeap thread_heap{main_heap};
Givy::Network network_interface{layout};
}

void init (int * argc, char ** argv[]) {
	(void) argc;
	(void) argv;
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
