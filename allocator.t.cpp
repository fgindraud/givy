#define ASSERT_LEVEL_SAFE

#include "givy.h"

#include <array>
#include <thread>

using namespace Givy;
using namespace Givy::Allocator;
namespace G = GlobalInstance;

#define DETERMINISTIC_SMALL_TEST 0
#define DETERMINISTIC_MONOTHREAD_TEST 1
#define MULTITHREAD_SMALL_TEST 0

void show (const char * title, bool b = false) {
	printf ("#################### %s #####################\n", title);
	G::print (b);
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

struct spin_lock {
	std::atomic<bool> taken {false};
	void lock (void) {
		bool expected = false;
		while (!taken.compare_exchange_weak (expected, true))
			expected = false;
	}
	void unlock (void) { taken = false; }
};

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
		constexpr int nb_th = 3;
		constexpr size_t from = 2;
		constexpr size_t to = VMem::SuperpageShift + 1;
		std::array<std::array<Block, to + 1>, nb_th> allocs;

		one_shot_barrier start (nb_th), p1 (nb_th);
		spin_lock io_lock;

		std::array<std::thread, nb_th> threads;
		for (size_t i = 0; i < nb_th; ++i)
			threads[i] = std::thread ([&](int thid) {
				//
				start.wait ();
				for (size_t i = from; i <= to; ++i)
					allocs[thid][i] = G::allocate (size_t (1) << i, 1);
				p1.wait ();
				io_lock.lock ();
				printf ("TH %d\n", thid);
				G::print (false);
				io_lock.unlock ();

				for (size_t i = from; i <= to; ++i)
					G::deallocate (allocs[thid][i]);
			}, i);

		start.wait_master ();
		p1.wait_master ();

		for (auto & th : threads)
			th.join ();
	}
#endif
	return 0;
}
