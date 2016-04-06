#define ASSERT_LEVEL_SAFE

#include <iostream>

#include "intrusive_list.h"

using namespace Givy::Intrusive;

template <typename T, typename Tag>
std::ostream & operator<<(std::ostream & os, List<T, Tag> & list) {
	for (auto & it : list)
		os << it;
	return os;
}

struct List_1;
struct List_2;

struct Blah : public List<Blah, List_1>::Element, public List<Blah, List_2>::Element {
	int x;
	explicit Blah (int i) : x (i) {}
};
std::ostream & operator<<(std::ostream & os, const Blah & b) {
	return os << b.x;
}

int main (void) {
	List<Blah, List_1> list_1;
	List<Blah, List_2> list_2;

	std::cout << "Sizeof struct Blah = " << sizeof (Blah) << "\n";

	Blah a (1), b (2), c (3);

	list_1.push_front (a);
	list_1.push_front (b);
	list_2.push_front (a);
	list_2.push_front (c);

	std::cout << "List_1 = " << list_1 << "\n";
	std::cout << "List_2 = " << list_2 << "\n";

	std::cout << "Pop from List_2 = " << list_2.front () << "\n";
	list_2.pop_front ();
	std::cout << "List_1 = " << list_1 << "\n";
	std::cout << "List_2 = " << list_2 << "\n";
	return 0;
}
