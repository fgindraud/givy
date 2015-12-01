#define ASSERT_LEVEL_SAFE

#include <thread>
#include <atomic>

#include "alloc_parts.h"
#include "superpage_tracker.h"

using namespace Givy;

void sep (void) {
	printf ("\n---------------------------------------------------------\n");
}

struct SystemAlloc {
	// For testing only...
	Block allocate (size_t size, size_t) { return {Ptr (new char[size]), size}; }
	void deallocate (Block blk) { delete[] blk.ptr.as<char *> (); }
};

int main (void) {
	SystemAlloc alloc;
	GasLayout layout (nullptr, 500 * VMem::SuperpageSize, 1, 0);
	Allocator::SuperpageTracker<SystemAlloc> tracker (layout, alloc);

	sep ();
	{
		// Check layouts
		printf ("Mixed sized allocs\n");
		size_t s1 = tracker.acquire_num (10);
		size_t s2 = tracker.acquire_num (20);
		size_t s3 = tracker.acquire_num (70);
		tracker.print ();
		printf ("%zu %zu %zu\n", s1, s2, s3);

		printf ("Partial deallocation\n");
		tracker.release_num (s2, 20);
		tracker.release_num (s1, 10);
		tracker.print ();

		// Check allocation in fragmented area
		printf ("Mixed alloc ; will fragment\n");
		size_t s4 = tracker.acquire_num (15);
		size_t s5 = tracker.acquire_num (20);
		size_t s6 = tracker.acquire_num (10);
		tracker.print ();
		printf ("%zu %zu %zu\n", s4, s5, s6);

		// Check results of header finding
		for (size_t s = 0; s < 100; s += 10)
			printf ("Header of %zu = %zu\n", s, tracker.get_block_start_num (s));

		// Test clean deallocation
		printf ("Deallocation\n");
		tracker.release_num (s3, 70);
		tracker.release_num (s4, 15);
		tracker.release_num (s5, 20);
		tracker.release_num (s6, 10);
		tracker.print ();
	}
	sep ();
	{
		// Test parallel modifications (may fail spuriously if too much contention)
		int nb_th = 4;
		int nb_alloc = 10;
		size_t allocs[nb_th * nb_alloc];

		std::thread threads[nb_th];
		std::atomic<int> start (0), wait_count (nb_th);

		for (int i = 0; i < nb_th; ++i)
			threads[i] = std::thread ([&](size_t * r) {
				wait_count--;
				while (start == 0)
					;
				for (int j = 0; j < nb_alloc; ++j)
					r[j] = tracker.acquire_num (10);
			}, &allocs[i * nb_alloc]);
		while (wait_count)
			;
		start = 1;
		for (int i = 0; i < nb_th; ++i)
			threads[i].join ();
		for (int j = 0; j < nb_th; ++j) {
			for (int i = 0; i < nb_alloc; ++i)
				printf ("%zu ", allocs[j * nb_alloc + i]);
			printf ("\n");
		}
		tracker.print ();
	}
	sep ();

	return 0;
}
