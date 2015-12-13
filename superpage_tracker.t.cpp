#define ASSERT_LEVEL_SAFE

#include <thread>
#include <atomic>

#include "superpage_tracker.h"

using namespace Givy;

void sep (void) {
	printf ("\n---------------------------------------------------------\n");
}

struct one_shot_barrier {
	std::atomic<bool> start_flag;
	std::atomic<int> wait_count;
	one_shot_barrier (int n) : start_flag (false), wait_count (n) {}
	void wait (void) {
		wait_count--;
		while (!start_flag)
			;
	}
	void wait_master (void) {
		while (wait_count > 0)
			;
		start_flag = true;
	}
};

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
		int nb_th = 4;
		int nb_alloc = 10;
		size_t allocs[nb_th * nb_alloc];

		std::thread threads[nb_th];
		one_shot_barrier b (nb_th);

		for (int i = 0; i < nb_th; ++i)
			threads[i] = std::thread ([&](size_t * r) {
				b.wait ();
				for (int j = 0; j < nb_alloc; ++j)
					r[j] = tracker.acquire (10);
			}, &allocs[i * nb_alloc]);

		b.wait_master ();
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
