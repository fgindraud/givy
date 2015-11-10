#ifndef CHAIN_H
#define CHAIN_H

#include <iterator>

template <typename T, typename Tag = void> class ForwardChain {
	/* Simple linked list with embedded elements.
	 *
	 * Any struct that wants to be added to the list must inherit from Element.
	 * Tag allows a T object to participate in multiple lists (differentiate the type).
	 */
public:
	struct Element {
		Element * next;
	};

private:
	Element * head = nullptr;

public:
	T * pop (void) noexcept {
		T * r = static_cast<T *> (head);
		if (head != nullptr) head = head->next;
		return r;
	}
	void push (T & t) noexcept {
		Element & e = t;
		e.next = head;
		head = &e;
	}

	class iterator : public std::iterator<std::forward_iterator_tag, T> {
	private:
		Element * current;

	public:
		// default construction is equivalent to a end ptr.
		iterator (Element * p = nullptr) noexcept : current (p) {}
		bool operator==(iterator other) noexcept { return current == other.current; }
		bool operator!=(iterator other) noexcept { return current != other.current; }
		iterator & operator++(void) noexcept {
			current = current->next;
			return *this;
		}
		iterator operator++(int) noexcept {
			auto cpy = *this;
			++*this;
			return cpy;
		}
		T & operator*(void) noexcept { return *static_cast<T *> (current); }
	};

	iterator begin (void) noexcept { return {head}; }
	iterator end (void) noexcept { return {}; }
};

template <typename T, typename Tag = void> class Chain {
	/* Double linked list using embedded elements.
	 *
	 * Any struct that wants to be added in such a list should inherit from Element.
	 * Tag is used to allow 2 independent lists in the same struct
	 */
public:
	struct Element {
		Element * prev;
		Element * next;
		// Initialise as a singleton link (loops on itself)
		Element () noexcept { reset (); }
		void reset (void) noexcept { prev = next = this; }
		~Element () noexcept { extract (this); /* remove from any list */ }
		// Copy/move is meaningless
		Element (const Element &) = delete;
		Element & operator=(const Element &) = delete;
		Element (Element &&) = delete;
		Element & operator=(Element &&) = delete;
	};

	void push_front (T & t) noexcept { cross (&root, &t); }
	void push_back (T & t) noexcept { cross (&t, &root); }
	T * pop_front (void) noexcept {
		Element * e = root.next;
		if (e == &root) {
			return nullptr;
		} else {
			extract (e);
			return e;
		}
	}
	T * pop_back (void) noexcept {
		Element * e = root.prev;
		if (e == &root) {
			return nullptr;
		} else {
			extract (e);
			return e;
		}
	}

	static void unlink (T & t) noexcept { extract (&t); }

	/* comp (a, b) is a < b */
	template <typename FunctionType>
	void insert_sorted (T & to_insert, FunctionType & comp) noexcept (noexcept (comp)) {
		for (auto & t : *this) {
			if (comp (to_insert, t)) {
				cross (&to_insert, &t);
				return;
			}
		}
		// Put at the end if above all others
		cross (&to_insert, &root);
	}
	template <typename FunctionType>
	T * pop_first_true (FunctionType & cond) noexcept (noexcept (cond)) {
		for (auto & t : *this) {
			if (cond (t)) {
				extract (&t);
				return &t;
			}
		}
		return nullptr;
	}

	class iterator : public std::iterator<std::bidirectional_iterator_tag, T> {
	private:
		Element * current;

	public:
		iterator (Element * p = nullptr) noexcept : current (p) {}
		bool operator==(iterator other) const noexcept { return current == other.current; }
		bool operator!=(iterator other) const noexcept { return current != other.current; }
		iterator & operator++(void) noexcept {
			current = current->next;
			return *this;
		}
		iterator & operator--(void) noexcept {
			current = current->prev;
			return *this;
		}
		iterator operator++(int) noexcept {
			auto cpy = *this;
			++*this;
			return cpy;
		}
		iterator operator--(int) noexcept {
			auto cpy = *this;
			--*this;
			return cpy;
		}
		T & operator*(void) const noexcept { return *static_cast<T *> (current); }
	};

	iterator begin (void) const noexcept { return {root.next}; }
	iterator end (void) const noexcept { return {&root}; }

private:
	/* Element structs forms a double linked circular chain. */
	Element root;

	/* Cross :
	 *  _left_a_     _b_right_      _left_right_
	 * /        \ + /         \ => /            \
	 * \__....__/   \___...___/    \__.._a_b_.._/
	 *
	 * Cuts left->a, b->right, and creates left->right, b->a.
	 * If left or right is a singleton chain, equivalent to before/after insertion.
	 */
	static void cross (Element * left, Element * right) noexcept {
		Element * a = left->next;
		Element * b = right->prev;
		a->prev = b;
		b->next = a; // a->b link
		left->next = right;
		right->prev = left; // left->right link
	}

	/* Extract :
	 *  _a_link_b_      _a__b_     _link_
	 * /          \ => /      \ + /      \
	 * \___....___/    \_...._/   \______/
	 *
	 * Cuts a->link, link->b, generates a->b, link->link.
	 */
	static void extract (Element * link) noexcept { cross (link, link); }
};

#endif
