#pragma once
#ifndef TESTS_H
#define TESTS_H

#include <thread>
#include <array>
#include <atomic>

template <int N> struct barrier {
	// Thread uid from 0
	static std::atomic<int> next_uid;
	static int new_uid (void) {
		int id = next_uid.fetch_add (1);
		ASSERT_STD (id < N);
		return id;
	}
	static thread_local int uid;

	// Reversing sense barrier
	std::atomic<bool> reversing_flag{true};
	std::atomic<int> wait_count{1};
	std::array<bool, N> local_flags;
	barrier () { local_flags.fill (true); }
	void wait (void) {
		int th_id = uid;
		if (th_id == -1)
			th_id = uid = barrier<N>::new_uid ();
		if (th_id == 0) {
			// Master
			if (local_flags[th_id])
				while (wait_count < N)
					;
			else
				while (wait_count > 1)
					;
			local_flags[th_id] = !local_flags[th_id];
			reversing_flag = local_flags[th_id];
		} else {
			// Others
			wait_count += local_flags[th_id] ? 1 : -1;
			local_flags[th_id] = !local_flags[th_id];
			while (reversing_flag != local_flags[th_id])
				;
		}
	}
	void operator() (void) { wait (); }
};
template <int N> std::atomic<int> barrier<N>::next_uid{0};
template <int N> thread_local int barrier<N>::uid = -1;

struct spin_lock {
	std::atomic<bool> taken{false};
	void lock (void) {
		bool expected = false;
		while (!taken.compare_exchange_weak (expected, true))
			expected = false;
	}
	void unlock (void) { taken = false; }
};

#endif
