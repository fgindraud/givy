#ifndef NETWORK_H
#define NETWORK_H

#include <mpi.h>
#include <mutex>

#include "reporting.h"

namespace Givy {
class Network {
private:
	std::mutex mutex;

public:
	Network (int & argc, char **& argv) {
		int provided = 0;
		MPI_Init_thread (&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
		ASSERT_OPT (provided >= MPI_THREAD_SERIALIZED);
	}
	~Network () { MPI_Finalize (); }

	size_t node_id (void) const {
		int rank;
		MPI_Comm_rank (MPI_COMM_WORLD, &rank);
		return static_cast<size_t> (rank);
	}
	size_t nb_node (void) const {
		int size;
		MPI_Comm_size (MPI_COMM_WORLD, &size);
		return static_cast<size_t> (size);
	}

	void send_to (size_t to, Ptr data, size_t size) {
		std::lock_guard<std::mutex> lock (mutex);
		MPI_Send (data, size, MPI_BYTE, to, 0 /* tag */, MPI_COMM_WORLD);
	}

	bool try_recv (size_t & from, void *& data, size_t & size) {
		std::lock_guard<std::mutex> lock (mutex);
		int flag = 0;
		MPI_Status status;
		MPI_Iprobe (MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
		if (flag) {
			from = static_cast<size_t> (status.MPI_SOURCE);
			int s;
			MPI_Get_count (&status, MPI_BYTE, &s);
			size = static_cast<size_t> (s);
			data = new char[size];

			MPI_Recv (data, size, MPI_BYTE, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD,
			          MPI_STATUSES_IGNORE);
		}
		return flag;
	}
};
}

#endif
