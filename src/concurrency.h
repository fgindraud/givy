#pragma once
#ifndef CONCURRENCY_H
#define CONCURRENCY_H

#include <atomic>

/* TODO ?
 * #include <thread>
 * std::thread::id
 * Have recursive spin lock
 */

#include "reporting.h"

namespace Givy {

class SpinLock {
private:
	std::atomic_bool locked{false};

public:
	void lock (void) {
		bool expected;
		do {
			expected = false;
		} while (!locked.compare_exchange_weak (expected, true, std::memory_order_acquire,
		                                        std::memory_order_relaxed));
	}
	void unlock (void) {
		ASSERT_SAFE (locked.load (std::memory_order_seq_cst) == true);
		locked.store (false, std::memory_order_release);
	}
};
}

#endif
