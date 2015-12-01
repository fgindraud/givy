#include <iostream>

#include "chain.h"

using namespace Givy;

template <typename T, typename Tag>
std::ostream & operator<<(std::ostream & os, Chain<T, Tag> & chain) {
	for (auto & it : chain)
		os << it;
	return os;
}

struct Chain_1;
struct Chain_2;

struct Blah : public Chain<Blah, Chain_1>::Element, public Chain<Blah, Chain_2>::Element {
	int x;
	explicit Blah (int i) : x (i) {}
};
std::ostream & operator<<(std::ostream & os, const Blah & b) {
	return os << b.x;
}

int main (void) {
	Chain<Blah, Chain_1> chain_1;
	Chain<Blah, Chain_2> chain_2;

	std::cout << "Sizeof struct Blah = " << sizeof (Blah) << "\n";

	Blah a (1), b (2), c (3);

	chain_1.push_front (a);
	chain_1.push_front (b);
	chain_2.push_front (a);
	chain_2.push_front (c);

	std::cout << "Chain_1 = " << chain_1 << "\n";
	std::cout << "Chain_2 = " << chain_2 << "\n";

	std::cout << "Pop from Chain_2 = " << *chain_2.pop_front () << "\n";
	std::cout << "Chain_1 = " << chain_1 << "\n";
	std::cout << "Chain_2 = " << chain_2 << "\n";
	return 0;
}
