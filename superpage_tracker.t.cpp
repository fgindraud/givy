#define ASSERT_LEVEL_SAFE

#include "superpage_tracker.h"
#include "tests.h"

#include <thread>
#include <array>

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
		size_t s1 = tracker.acquire (10);
		size_t s2 = tracker.acquire (20);
		size_t s3 = tracker.acquire (70);
		tracker.print ();
		printf ("%zu %zu %zu\n", s1, s2, s3);

		printf ("Partial deallocation\n");
		tracker.release (s2, 20);
		tracker.release (s1, 10);
		tracker.print ();

		// Check allocation in fragmented area
		printf ("Mixed alloc ; will fragment\n");
		size_t s4 = tracker.acquire (15);
		size_t s5 = tracker.acquire (20);
		size_t s6 = tracker.acquire (10);
		size_t s7 = tracker.acquire (2);
		tracker.print ();
		printf ("%zu %zu %zu %zu\n", s4, s5, s6, s7);

		// Check results of header finding
		for (size_t s = 0; s < 100; s += 10)
			printf ("Header of %zu = %zu\n", s, tracker.get_block_start_num (s));

		// Test trimming
		printf ("Trimming\n");
		tracker.trim (s7, 2);
		tracker.trim (s5, 20);
		tracker.print ();

		// Test clean deallocation
		printf ("Deallocation\n");
		tracker.release (s3, 70);
		tracker.release (s4, 15);
		tracker.release (s5, 1);
		tracker.release (s6, 10);
		tracker.release (s7, 1);
		tracker.print ();
	}
	sep ();
	{
		// Test parallel modifications (may fail spuriously if too much contention)
		constexpr int nb_th = 4;
		constexpr int nb_alloc = 10;
		std::array<std::array<size_t, nb_alloc>, nb_th> allocs;

		std::array<std::thread, nb_th> threads;
		barrier<nb_th + 1> wait;

		for (int i = 0; i < nb_th; ++i)
			threads[i] = std::thread ([&](int thid) {
				wait ();
				for (int j = 0; j < nb_alloc; ++j)
					allocs[thid][j] = tracker.acquire (10);
				wait ();
				wait ();
				for (int j = 0; j < nb_alloc; ++j)
					tracker.release (allocs[thid][j], 10);
			}, i);

		wait ();
		wait ();
		tracker.print ();
		for (auto & ath : allocs) {
			for (auto s : ath)
				printf ("%zu ", s);
			printf ("\n");
		}
		wait ();
		for (auto & th : threads)
			th.join ();
		tracker.print ();
	}
	sep ();

	return 0;
}
