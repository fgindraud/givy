/* Givy lib cpp file
 *
 * defines interface functions (C-like)
 */
#include <cstdlib>

#include "reporting.h"
#include "types.h"
#include "pointer.h"
#include "gas_space.h"
#include "allocator.h"
#include "network.h"
#include "coherence.h"
#include "givy.h"

namespace Givy {
namespace {
	// Bootstrap allocator
	Allocator::Bootstrap bootstrap_allocator;

	/* Thread local heaps.
	 * One will be constructed for each thread, and will be destroyed at thread exit.
	 */
	thread_local Allocator::ThreadLocalHeap thread_heap;

	//
	Optional<Gas::Space> gas_space;
	Optional<Network> network_interface;
}

void init (int & argc, char **& argv) {
	ASSERT_STD (!gas_space);
	network_interface.construct (argc, argv);

	auto nb_node = network_interface->nb_node ();
	auto node_id = network_interface->node_id ();
	ASSERT_STD (nb_node <= Coherence::max_supported_node);
	DEBUG_TEXT ("[init] nb_node=%zu, node_id=%zu\n", nb_node, node_id);

	// TODO get size & start from env or args
	auto base_ptr = Ptr (0x4000'0000'0000);
	gas_space.construct (base_ptr, 100 * VMem::superpage_size, nb_node, node_id, bootstrap_allocator);
}

Block allocate (size_t size, size_t align) {
	if (gas_space) {
		return thread_heap.allocate (size, align, *gas_space);
	} else {
		// TODO improve default mode allocator
		return {Ptr (malloc (size)), size};
	}
}

void deallocate (Block blk) {
	if (!gas_space || !gas_space->in_gas (blk.ptr)) {
		// TODO improve default mode allocator
		free (blk.ptr);
		return;
	}

	if (!gas_space->in_local_interval (blk.ptr)) {
		// TODO node remote free
		return;
	}

	thread_heap.deallocate (blk, *gas_space);
}

void deallocate (Ptr ptr) {
	// TODO
	ASSERT_STD_FAIL ("not implemented");
	(void) ptr;
}
}
