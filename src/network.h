#ifndef NETWORK_H
#define NETWORK_H

#include <mpi.h>

#include <memory>

#include "reporting.h"

namespace Givy {
class Network {
	// Calls should be serialized
private:
	int comm_rank;
	int comm_size;

public:
	Network (int & argc, char **& argv) {
		int provided = 0;
		MPI_Init_thread (&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
		ASSERT_OPT (provided >= MPI_THREAD_SERIALIZED);
		MPI_Comm_rank (MPI_COMM_WORLD, &comm_rank);
		MPI_Comm_size (MPI_COMM_WORLD, &comm_size);
	}
	~Network () { MPI_Finalize (); }

	size_t node_id (void) const { return static_cast<size_t> (comm_rank); }
	size_t nb_node (void) const { return static_cast<size_t> (comm_size); }

	void send_to (size_t to, void * data, size_t size) {
		MPI_Send (data, size, MPI_BYTE, to, 0 /* tag */, MPI_COMM_WORLD);
	}

	std::unique_ptr<char[]> try_recv (size_t & from) {
		std::unique_ptr<char[]> data;
		int flag = 0;
		MPI_Status status;
		MPI_Iprobe (MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
		if (flag) {
			from = static_cast<size_t> (status.MPI_SOURCE);
			int s;
			MPI_Get_count (&status, MPI_BYTE, &s);
			auto size = static_cast<size_t> (s);
			data.reset (new char[size]);

			MPI_Recv (data.get (), size, MPI_BYTE, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD,
			          MPI_STATUSES_IGNORE);
		}
		return data;
	}
};
}

#endif
