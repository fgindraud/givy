#define ASSERT_LEVEL_SAFE

#include "allocator.h"
#include "tests.h"

namespace {
using namespace Givy;

Allocator::Bootstrap boostrap_allocator;
thread_local Allocator::ThreadLocalHeap thread_heap;
Gas::Space space{Ptr (0x4000'0000'0000),     // start
                 100 * VMem::superpage_size, // space_by_node
                 4,                          // nb_node
                 0,                          // local node
                 boostrap_allocator};

Block allocate (size_t size, size_t align) {
	return thread_heap.allocate (size, align, space);
}
void deallocate (Block blk) {
	thread_heap.deallocate (blk, space);
}
void print (bool print_space = true) {
	if (print_space) {
		printf ("========== Space ===========\n");
		space.print ();
	}
	thread_heap.print (space);
}
}

#define DETERMINISTIC_SMALL_TEST 1
#define DETERMINISTIC_MONOTHREAD_TEST 1
#define MULTITHREAD_SMALL_TEST 1

void show (const char * title, bool b = false) {
	printf ("#################### %s #####################\n", title);
	print (b);
}

int main (void) {
#if DETERMINISTIC_SMALL_TEST
	{
		Givy::Allocator::SizeClass::print ();
		auto p1 = allocate (0xF356, 1);
		auto p2 = allocate (53, 1);
		show ("A[12]");
		deallocate (p1);
		show ("A[2]");
		auto p3 = allocate (4096, 1);
		show ("A[23]");
		deallocate (p2);
		deallocate (p3);
		show ("A[]");
	}
#endif
#if DETERMINISTIC_MONOTHREAD_TEST
	{
		constexpr size_t from = 2;
		constexpr size_t to = VMem::superpage_shift + 1;
		std::array<Block, to + 1> under;
		std::array<Block, to + 1> exact;
		std::array<Block, to + 1> above;
		// Just above SuperpageBlock::available_pages, but smaller than superpagesize
		Block small_superpage;

		for (size_t i = from; i <= to; ++i)
			under[i] = allocate ((size_t (1) << i) - 1, 1);
		for (size_t i = from; i <= to; ++i)
			exact[i] = allocate ((size_t (1) << i), 1);
		for (size_t i = from; i <= to; ++i)
			above[i] = allocate ((size_t (1) << i) + 1, 1);
		small_superpage = allocate (
		    (Givy::Allocator::SuperpageBlock::available_pages + 1) * Givy::VMem::page_size, 1);
		show ("Allocation", true);

		for (size_t i = to; i >= from; --i)
			deallocate (under[i]);
		show ("Partial deallocation", true);

		for (size_t i = from; i <= to; ++i)
			under[i] = allocate ((size_t (1) << i) - 1, 1);
		show ("Reallocation", true);

		for (size_t i = from; i <= to; ++i)
			deallocate (under[i]);
		for (size_t i = from; i <= to; ++i)
			deallocate (exact[i]);
		for (size_t i = from; i <= to; ++i)
			deallocate (above[i]);
		deallocate (small_superpage);
		show ("Deallocation", true);
	}
#endif
#if MULTITHREAD_SMALL_TEST
	{
		constexpr int nb_th = 2;
		constexpr size_t from = 2;
		constexpr size_t to = VMem::superpage_shift + 1;
		std::array<std::array<Block, to + 1>, nb_th> allocs;

		barrier<nb_th> wait;
		spin_lock io;

		std::array<std::thread, nb_th> threads;
		for (size_t i = 0; i < nb_th; ++i)
			threads[i] = std::thread ([&](int thid) {

				wait ();
				for (size_t i = from; i <= to; ++i)
					allocs[thid][i] = allocate (size_t (1) << i, 1);
				wait ();
				io.lock ();
				printf ("[alloc][TH=%d]", thid);
				print (thid == 0);
				io.unlock ();
				wait ();
				// Remote dealloc
				for (size_t i = from; i <= to; ++i)
					deallocate (allocs[(thid + 1) % nb_th][i]);
				wait ();
				io.lock ();
				printf ("[remote_free][TH=%d]", thid);
				print (thid == 0);
				io.unlock ();
				wait ();
				// Allocation then thread death
				for (size_t i = from; i <= to; ++i)
					allocs[thid][i] = allocate (size_t (1) << i, 1);
				wait ();
				io.lock ();
				printf ("[realloc][TH=%d]", thid);
				print (thid == 0);
				io.unlock ();
				wait ();
				if (thid > 0) {
					return;
				} else {
					for (auto & th_alloc : allocs)
						for (size_t i = from; i <= to; ++i)
							deallocate (th_alloc[i]);
					print (true);
				}

			}, i);

		for (auto & th : threads)
			th.join ();
	}
#endif
	return 0;
}
