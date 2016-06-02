#include <cblas.h>
#include <mpi.h> // tmp

// for convenience
#include "range.h"
using Givy::range;
//

#include "givy.h"

template <typename T> T * givy_malloc (size_t nb_elem = 1) {
	return static_cast<T *> (Givy::allocate (sizeof (T) * nb_elem, std::max (alignof (T), 64ul)).ptr);
}

// using RowMajor
void dense_matrix_fma (size_t size, double * acc, const double * lhs, const double * rhs) {
	cblas_dgemm (CblasRowMajor, CblasNoTrans, CblasNoTrans, size, size, size, 1.0, lhs, size, rhs,
	             size, 1.0, acc, size);
}

using SpmBlock = double *;
using SpmRow = SpmBlock *;

SpmRow * create_sparse_matrix (size_t nb_blocks) {
	auto rows = givy_malloc<SpmRow> (nb_blocks);
	for (auto i : range (nb_blocks))
		rows[i] = nullptr;
	for (auto i : range (nb_blocks)) {
		rows[i] = givy_malloc<SpmBlock> (nb_blocks);
		for (auto j : range (nb_blocks))
			rows[i][j] = nullptr;
	}
	return rows;
}
void destroy_sparse_matrix (SpmRow * rows, size_t nb_blocks) {
	if (rows != nullptr) {
		for (auto i : range (nb_blocks))
			if (rows[i] != nullptr) {
				for (auto j : range (nb_blocks))
					if (rows[i][j] != nullptr)
						Givy::deallocate (rows[i][j]);
				Givy::deallocate (rows[i]);
			}
		Givy::deallocate (rows);
	}
}

decltype (auto) make_matrix_test (const size_t size, const double diag = 1.0) {
	auto m = givy_malloc<double> (size * size);
	for (auto i : range (size)) {
		auto * row = &m[i * size];
		for (auto j : range (size))
			row[j] = 0.0;
		row[i] = diag;
	}
	return m;
}

int main (int argc, char * argv[]) {
	Givy::init (argc, argv);

	int nb_node, node_id;
	MPI_Comm_rank (MPI_COMM_WORLD, &node_id);
	MPI_Comm_size (MPI_COMM_WORLD, &nb_node);
	constexpr int tag = 4;


	size_t nb_blocks = 64;
	size_t block_size = 64;

	using Block = double *;
	using Row = Block *;
	Row * rows = nullptr;

	if (node_id == 0) {
		// Create matrix on node 0
		rows = create_sparse_matrix (nb_blocks);
		for (auto to : range (1, nb_node)) {
			auto lock = Givy::network_lock ();
			MPI_Send (&rows, sizeof (rows), MPI_BYTE, to, tag, MPI_COMM_WORLD);
		}
	} else {
		auto lock = Givy::network_lock ();
		MPI_Recv (&rows, sizeof (rows), MPI_BYTE, 0, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}

	DEBUG_TEXT ("[N%d] rows = %p\n", node_id, rows);
	Givy::require_read_only (rows);

	if (node_id == 0) {
		//destroy_sparse_matrix (rows, nb_blocks);
	}

	return 0;
}
