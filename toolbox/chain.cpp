#include <iostream>

template<typename T, typename Tag = void> struct Chain {
	struct Element {
		T * next;
	};

	T * head;
	Chain () : head (nullptr) {}
	T * pop (void) {
		if (head == nullptr) {
			return nullptr;
		} else {
			T * r = head;
			Element & e = *r;
			head = e.next;
			e.next = nullptr;
			return r;
		}
	}
	void push (T & t) {
		Element & e = t;
		e.next = head;
		head = &t;
	}
};

template<typename T, typename Tag>
std::ostream & operator<< (std::ostream & os, const Chain<T, Tag> & chain) {
	for (T * it = chain.head; it != nullptr; it = static_cast<typename Chain<T, Tag>::Element &> (*it).next)
		os << *it;
	return os;
}

struct Chain_1;
struct Chain_2;

struct Blah : public Chain<Blah, Chain_1>::Element, public Chain<Blah, Chain_2>::Element {
	int x;
	Blah (int i) : x (i) {}
};
std::ostream & operator<< (std::ostream & os, const Blah & b) { return os << b.x; }

int main (void) {
	Chain<Blah, Chain_1> chain_1;
	Chain<Blah, Chain_2> chain_2;

	std::cout << "Sizeof struct Blah = " << sizeof (Blah) << "\n";

	Blah a (1), b (2), c (3);

	chain_1.push (a); chain_1.push (b);
	chain_2.push (a); chain_2.push (c);

	std::cout << "Chain_1 = " << chain_1 << "\n";
	std::cout << "Chain_2 = " << chain_2 << "\n";

	std::cout << "Pop from Chain_2 = " << *chain_2.pop () << "\n";
	std::cout << "Chain_1 = " << chain_1 << "\n";
	std::cout << "Chain_2 = " << chain_2 << "\n";
}
