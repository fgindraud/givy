#define ASSERT_LEVEL_SAFE

#include "givy.h"
#include "tests.h"

#include <array>
#include <thread>

using namespace Givy;
using namespace Givy::Allocator;
namespace G = GlobalInstance;

#define DETERMINISTIC_SMALL_TEST 0
#define DETERMINISTIC_MONOTHREAD_TEST 0
#define MULTITHREAD_SMALL_TEST 1

void show (const char * title, bool b = false) {
	printf ("#################### %s #####################\n", title);
	G::print (b);
}

int main (void) {
#if DETERMINISTIC_SMALL_TEST
	{
		SizeClass::print ();
		auto p1 = G::allocate (0xF356, 1);
		auto p2 = G::allocate (53, 1);
		show ("A[12]");
		G::deallocate (p1);
		show ("A[2]");
		auto p3 = G::allocate (4096, 1);
		show ("A[23]");
		G::deallocate (p2);
		G::deallocate (p3);
		show ("A[]");
	}
#endif
#if DETERMINISTIC_MONOTHREAD_TEST
	{
		constexpr size_t from = 2;
		constexpr size_t to = VMem::SuperpageShift + 1;
		std::array<Block, to + 1> under;
		std::array<Block, to + 1> exact;
		std::array<Block, to + 1> above;
		// Just above SuperpageBlock::AvailablePages, but smaller than superpagesize
		Block small_superpage;

		for (size_t i = from; i <= to; ++i)
			under[i] = G::allocate ((size_t (1) << i) - 1, 1);
		for (size_t i = from; i <= to; ++i)
			exact[i] = G::allocate ((size_t (1) << i), 1);
		for (size_t i = from; i <= to; ++i)
			above[i] = G::allocate ((size_t (1) << i) + 1, 1);
		small_superpage = G::allocate ((SuperpageBlock::AvailablePages + 1) * VMem::PageSize, 1);
		show ("Allocation", true);

		for (size_t i = to; i >= from; --i)
			G::deallocate (under[i]);
		show ("Partial deallocation", true);

		for (size_t i = from; i <= to; ++i)
			under[i] = G::allocate ((size_t (1) << i) - 1, 1);
		show ("Reallocation", true);

		for (size_t i = from; i <= to; ++i)
			G::deallocate (under[i]);
		for (size_t i = from; i <= to; ++i)
			G::deallocate (exact[i]);
		for (size_t i = from; i <= to; ++i)
			G::deallocate (above[i]);
		G::deallocate (small_superpage);
		show ("Deallocation", true);
	}
#endif
#if MULTITHREAD_SMALL_TEST
	{
		constexpr int nb_th = 2;
		constexpr size_t from = 2;
		constexpr size_t to = VMem::SuperpageShift + 1;
		std::array<std::array<Block, to + 1>, nb_th> allocs;

		barrier<nb_th> wait;
		spin_lock io;

		std::array<std::thread, nb_th> threads;
		for (size_t i = 0; i < nb_th; ++i)
			threads[i] = std::thread ([&](int thid) {

				wait ();
				for (size_t i = from; i <= to; ++i)
					allocs[thid][i] = G::allocate (size_t (1) << i, 1);
				wait ();
				io.lock ();
				printf ("[alloc][TH=%d]", thid);
				G::print (thid == 0);
				io.unlock ();
				wait ();
				// Remote dealloc
				for (size_t i = from; i <= to; ++i)
					G::deallocate (allocs[(thid + 1) % nb_th][i]);
				wait ();
				io.lock ();
				printf ("[remote_free][TH=%d]", thid);
				G::print (thid == 0);
				io.unlock ();
				wait ();
				// Allocation then thread death
				for (size_t i = from; i <= to; ++i)
					allocs[thid][i] = G::allocate (size_t (1) << i, 1);
				wait ();
				io.lock ();
				printf ("[realloc][TH=%d]", thid);
				G::print (thid == 0);
				io.unlock ();
				wait ();
				if (thid > 0) {
					return;
				} else {
					for (auto & th_alloc : allocs)
						for (size_t i = from; i <= to; ++i)
							G::deallocate (th_alloc[i]);
					G::print (true);
				}

			}, i);

		for (auto & th : threads)
			th.join ();
	}
#endif
	return 0;
}
