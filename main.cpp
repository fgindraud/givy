#include "givy.h"

int main (int argc, char * argv[]) {
	init (&argc, &argv);

	auto p1 = allocate (0xF356, 1);
	auto p2 = allocate (53, 1);
	deallocate (p1);
	auto p3 = allocate (4096, 1);
	deallocate (p2);
	deallocate (p3);
	return 0;
}
