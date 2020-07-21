#include "network_2.h"

#include <mpi.h>

namespace Givy {
	namespace {
	}

Network::Network (int & argc, char **& argv) {
	int provided = 0;
	MPI_Init_thread (&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	ASSERT_OPT (provided >= MPI_THREAD_SERIALIZED);
	int comm_rank;
	int comm_size;
	MPI_Comm_rank (MPI_COMM_WORLD, &comm_rank);
	MPI_Comm_size (MPI_COMM_WORLD, &comm_size);
	ASSERT_SAFE (comm_rank >= 0);
	ASSERT_SAFE (comm_size > 0);
	m_node_id = static_cast<size_t> (comm_rank);
	m_nb_node = static_cast<size_t> (comm_size);
}

Network::~Network () {
	if (m_thread.joinable ()) {
		// TODO Signal that we exit
		m_thread.join ();
	}
	MPI_Finalize ();
}
}
