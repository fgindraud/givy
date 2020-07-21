/* Givy lib cpp file
 *
 * Defines interface functions
 */
#include <cstdlib>

#include "allocator.h"
#include "coherence.h"
#include "gas_space.h"
#include "givy.h"
#include "givy_c.h"
#include "network.h"
#include "pointer.h"
#include "reporting.h"
#include "types.h"

#include "instance.h"

namespace Givy {
	

namespace {
	// Global static storage structures
	struct StaticStuff {
		Allocator::Bootstrap bootstrap_allocator;
	};

	// Structures for the GAS mode, inited afterwards
	struct GasStuff {
		Constructible<Gas::Space> space;
		Constructible<Network> network;
		Constructible<Coherence::Manager> coherence;
		bool inited = false;

		GasStuff () = default;
		void init (int & argc, char **& argv);
		~GasStuff ();
	};

	// Structures per thread, crated and destructed with them
	struct ThreadStuff {
		Allocator::ThreadLocalHeap heap;
	};

	/* Global structures of the runtime
	 */
	StaticStuff global;
	GasStuff gas;
	thread_local ThreadStuff thread;

	/* Impl */

	void GasStuff::init (int & argc, char **& argv) {
		ASSERT_STD (!inited);
		network.construct (argc, argv);
		auto nb_node = network->nb_node ();
		auto node_id = network->node_id ();
		ASSERT_STD (nb_node <= Coherence::max_supported_node);
		DEBUG_TEXT ("[N%zu] Init nb_node=%zu\n", node_id, nb_node);

		// TODO get size & start from env or args
		auto base_ptr = Ptr (0x4000'0000'0000);
		space.construct (base_ptr, 100 * VMem::superpage_size, nb_node, node_id,
		                 global.bootstrap_allocator);
		coherence.construct (space.object (), network.object ());

		inited = true;
	}

	GasStuff::~GasStuff () {
		if (inited) {
			coherence.destruct ();
			network.destruct ();
			space.destruct ();
		}
	}
}

void init (int & argc, char **& argv) {
	gas.init (argc, argv);
}

Block allocate (size_t size, size_t align) {
	if (gas.inited) {
		return thread.heap.allocate (size, align, gas.space.object ());
	} else {
		return {malloc (size), size};
	}
}

void deallocate (void * ptr) {
	if (!gas.inited || !gas.space->in_gas (ptr)) {
		free (ptr);
	} else {
		// gas.coherence->deallocate (blk, thread.heap);
	}
}

void require_read_only (void * ptr) {
	ASSERT_SAFE (gas.inited);
	gas.coherence->request_region_valid (ptr);
}

void require_read_write (void * ptr) {
	//
}

// TODO temporary
std::unique_lock<std::mutex> network_lock (void) {
	return gas.network->get_lock ();
}
}

/***************
 * C interface *
 ***************/

void givy_init (int * argc, char ** argv[]) {
	ASSERT_STD (argc != nullptr);
	ASSERT_STD (argv != nullptr);
	Givy::init (*argc, *argv);
}

struct givy_block givy_allocate (size_t size, size_t align) {
	return Givy::allocate (size, align);
}
void givy_deallocate (void * ptr) {
	Givy::deallocate (ptr);
}

void givy_require_read_only (void * ptr) {
	Givy::require_read_only (ptr);
}
void givy_require_read_write (void * ptr) {
	Givy::require_read_write (ptr);
}
