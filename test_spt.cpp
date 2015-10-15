#include <thread>
#include <atomic>

#include "alloc_parts.h"
#include "superpage_tracker.h"

void sep (void) { printf ("\n---------------------------------------------------------\n"); }



int main (void) {
	AllocParts::System alloc;
	GasLayout layout (nullptr, 400 * VMem::SuperpageSize, 2);

	SuperpageTracker tracker (layout, alloc);

	sep ();
	{
		printf ("1 superpage_nb alloc on 2 nodes\n");
		size_t s2 = tracker.acquire (1, 1);
		size_t s3 = tracker.acquire (1, 1);
		size_t s1 = tracker.acquire (1, 0);
		tracker.print ();
		printf ("%zu %zu %zu\n", s1, s2, s3);

		printf ("Deallocation\n");
		tracker.release (s1, 1);
		tracker.release (s2, 1);
		tracker.release (s3, 1);
		tracker.print ();
	}
	sep ();
	{
		printf ("Mixed sized allocs\n");
		size_t s1 = tracker.acquire (10, 0);
		size_t s2 = tracker.acquire (20, 0);
		size_t s3 = tracker.acquire (70, 0);
		tracker.print ();
		printf ("%zu %zu %zu\n", s1, s2, s3);

		printf ("Partial deallocation\n");
		tracker.release (s2, 20);
		tracker.release (s1, 10);
		tracker.print ();

		printf ("Mixed alloc ; will fragment\n");
		size_t s4 = tracker.acquire (15, 0);
		size_t s5 = tracker.acquire (20, 0);
		size_t s6 = tracker.acquire (10, 0);
		tracker.print ();
		printf ("%zu %zu %zu\n", s4, s5, s6);
		
		printf ("Deallocation\n");
		tracker.release (s3, 70);
		tracker.release (s4, 15);
		tracker.release (s5, 20);
		tracker.release (s6, 10);
		tracker.print ();
	}
	sep ();
	{
		int nb_th = 4;
		int nb_alloc = 10;
		size_t allocs[nb_th * nb_alloc];
		
		std::thread threads[nb_th];
		std::atomic<int> start (0), wait_count (nb_th);

		for (int i = 0; i < nb_th; ++i)
			threads[i] = std::thread (
				[&] (size_t *r) {
					wait_count--;
					while (start == 0);
					for (int j = 0; j < nb_alloc; ++j)
						r[j] = tracker.acquire (10, 0);
				},
				&allocs[i * nb_alloc]);
		while (wait_count);
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

