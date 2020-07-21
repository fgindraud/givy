#pragma once
#ifndef GIVY_NETWORK_H
#define GIVY_NETWORK_H

#include <mpi.h>

#include <memory>
#include <mutex>

#include "reporting.h"

namespace Givy {
class Network {
	// Calls should be serialized
private:
	int comm_rank;
	int comm_size;

	std::mutex mutex;

	static constexpr int protocol_tag{42};

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
		std::lock_guard<std::mutex> lock (mutex);
		DEBUG_TEXT ("[N%d] sending %zu bytes to %zu\n", comm_rank, size, to);
		MPI_Send (data, size, MPI_BYTE, to, protocol_tag, MPI_COMM_WORLD);
	}

	std::unique_ptr<char[]> try_recv (size_t & from) {
		std::lock_guard<std::mutex> lock (mutex);
		std::unique_ptr<char[]> data;
		int flag = 0;
		MPI_Status status;
		MPI_Message message_handle;
		MPI_Improbe (MPI_ANY_SOURCE, protocol_tag, MPI_COMM_WORLD, &flag, &message_handle, &status);
		if (flag) {
			from = static_cast<size_t> (status.MPI_SOURCE);
			int s;
			MPI_Get_count (&status, MPI_BYTE, &s);
			auto size = static_cast<size_t> (s);
			data.reset (new char[size]);
			MPI_Mrecv (data.get (), size, MPI_BYTE, &message_handle, MPI_STATUSES_IGNORE);
		}
		return data;
	}

	// TODO temporary for tests
	std::unique_lock<std::mutex> get_lock (void) {
		std::unique_lock<std::mutex> lock (mutex);
		return lock;
	}
};
}

#endif
