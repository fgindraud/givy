#define ASSERT_LEVEL_SAFE

#include "utility.h"

#include <iostream>
#include <limits>

using namespace Givy;

template <size_t N = 1> void test_bound_uint (void) {
	std::cout << "BoundUint<" << N << "> [-1,0,+1]: " << std::numeric_limits<BoundUint<N - 1>>::digits
	          << "," << std::numeric_limits<BoundUint<N>>::digits << ","
	          << std::numeric_limits<BoundUint<N + 1>>::digits << "\n";
	test_bound_uint<2 * N> ();
}
template <> void test_bound_uint<0> (void) {}
template <> void test_bound_uint<(size_t (1) << 33)> (void) {}

int main (void) {
	test_bound_uint ();

	return 0;
}
