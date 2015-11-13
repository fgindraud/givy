#include <iostream>

#include "chain.h"

using namespace Givy;

template<typename T, typename Tag>
std::ostream & operator<< (std::ostream & os, ForwardChain<T, Tag> & chain) {
	for (auto & it : chain)
		os << it;
	return os;
}

struct Chain_1;
struct Chain_2;

struct Blah : public ForwardChain<Blah, Chain_1>::Element, public ForwardChain<Blah, Chain_2>::Element {
	int x;
	Blah (int i) : x (i) {}
};
std::ostream & operator<< (std::ostream & os, const Blah & b) { return os << b.x; }

int main (void) {
	ForwardChain<Blah, Chain_1> chain_1;
	ForwardChain<Blah, Chain_2> chain_2;

	std::cout << "Sizeof struct Blah = " << sizeof (Blah) << "\n";

	Blah a (1), b (2), c (3);

	chain_1.push (a); chain_1.push (b);
	chain_2.push (a); chain_2.push (c);

	std::cout << "Chain_1 = " << chain_1 << "\n";
	std::cout << "Chain_2 = " << chain_2 << "\n";

	std::cout << "Pop from Chain_2 = " << *chain_2.pop () << "\n";
	std::cout << "Chain_1 = " << chain_1 << "\n";
	std::cout << "Chain_2 = " << chain_2 << "\n";
	return 0;
}
