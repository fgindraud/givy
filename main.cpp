#include "givy.h"

int main (int argc, char * argv[]) {
	Givy::init (argc, argv);

	auto p1 = Givy::allocate (0xF356, 1);
	auto p2 = Givy::allocate (53, 1);
	Givy::deallocate (p1);
	auto p3 = Givy::allocate (4096, 1);
	Givy::deallocate (p2);
	Givy::deallocate (p3);
	return 0;
}
