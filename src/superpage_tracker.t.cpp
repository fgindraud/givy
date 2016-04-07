#define ASSERT_LEVEL_SAFE

#include "superpage_tracker.h"
#include "block.h"
#include "tests.h"

using namespace Givy;

void sep (void) {
	printf ("\n---------------------------------------------------------\n");
}

struct SystemAlloc {
	// For testing only...
	Block allocate (size_t size, size_t) { return {new char[size], size}; }
	void deallocate (Block blk) { delete[] static_cast<char*> (blk.ptr); }
};

int main (void) {
	SystemAlloc alloc;
	const size_t nb_node = 3;
	const size_t superpage_by_node = 200;
	const auto local_range = range_from_offset (superpage_by_node, superpage_by_node);
	SuperpageTracker<SystemAlloc> tracker (superpage_by_node * nb_node, alloc);

	auto acq = [&] (size_t n) { return tracker.acquire (n, local_range); };
	auto trim = [&] (auto && r) { tracker.trim (r); };
	auto rel = [&] (auto && r) { tracker.release (r); };
	auto print = [&] (void) { tracker.print (nb_node, superpage_by_node); };

	sep ();
	{
		// Check layouts
		printf ("Mixed sized allocs\n");
		size_t s1 = acq (10);
		size_t s2 = acq (20);
		size_t s3 = acq (70);
		print ();
		printf ("%zu %zu %zu\n", s1, s2, s3);

		printf ("Partial deallocation\n");
		rel (range_from_offset (s2, 20));
		rel (range_from_offset (s1, 10));
		print ();

		// Check allocation in fragmented area
		printf ("Mixed alloc ; will fragment\n");
		size_t s4 = acq (15);
		size_t s5 = acq (20);
		size_t s6 = acq (10);
		size_t s7 = acq (2);
		print ();
		printf ("%zu %zu %zu %zu\n", s4, s5, s6, s7);

		// Check results of header finding
		for (size_t s = 0; s < 100; s += 10)
			printf ("Header of %zu = %zu\n", s, tracker.get_sequence_start_num (s));

		// Test trimming
		printf ("Trimming\n");
		trim (range_from_offset (s7, 2));
		trim (range_from_offset (s5, 20));
		print ();

		// Test clean deallocation
		printf ("Deallocation\n");
		rel (range_from_offset (s3, 70));
		rel (range_from_offset (s4, 15));
		rel (range_from_offset (s5, 1));
		rel (range_from_offset (s6, 10));
		rel (range_from_offset (s7, 1));
		print ();
	}
	sep ();
	{
		// Test parallel modifications (may fail spuriously if too much contention)
		constexpr int nb_th = 4;
		constexpr int nb_alloc = 10;
		std::array<std::array<size_t, nb_alloc>, nb_th> allocs;

		std::array<std::thread, nb_th> threads;
		barrier<nb_th + 1> wait;

		for (int i : range (nb_th))
			threads[i] = std::thread ([&](int thid) {
				wait ();
				for (int j : range (nb_alloc))
					allocs[thid][j] = acq (10);
				wait ();
				wait ();
				for (int j : range (nb_alloc))
					rel (range_from_offset (allocs[thid][j], 10));
			}, i);

		wait ();
		wait ();
		print ();
		for (auto & ath : allocs) {
			for (auto s : ath)
				printf ("%zu ", s);
			printf ("\n");
		}
		wait ();
		for (auto & th : threads)
			th.join ();
		print ();
	}
	sep ();

	return 0;
}
