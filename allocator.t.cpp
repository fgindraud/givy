#define ASSERT_LEVEL_SAFE

#include "givy.h"

namespace G = Givy::GlobalInstance;

void show (const char * title) {
	printf ("#################### %s #####################\n", title);
	G::print (false);
}

int main (void) {
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
	return 0;
}
