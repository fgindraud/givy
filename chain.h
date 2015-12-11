#ifndef CHAIN_H
#define CHAIN_H

#include <iterator>
#include <atomic>

#include "reporting.h"

namespace Givy {

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
		Element () { reset (); }
		void reset (void) { prev = next = this; }
		~Element () { ASSERT_SAFE (next == this); /* ensure it has been removed from any list */ }
		// Copy/move is meaningless
		Element (const Element &) = delete;
		Element & operator=(const Element &) = delete;
		Element (Element &&) = delete;
		Element & operator=(Element &&) = delete;
	};

	// Default ctor ok ; others are deleted due to Element

	static void insert_after (Element & to_insert, Element & e) { cross (&e, &to_insert); }
	static void insert_before (Element & to_insert, Element & e) { cross (&to_insert, &e); }
	static void unlink (Element & e) { extract (&e); }

	bool empty (void) const { return root.next == &root; }
	T & front (void) {
		ASSERT_SAFE (!empty ());
		return *static_cast<T *> (root.next);
	}
	T & back (void) {
		ASSERT_SAFE (!empty ());
		return *static_cast<T *> (root.prev);
	}

	void push_front (T & t) { insert_after (t, root); }
	void push_back (T & t) { insert_before (t, root); }
	void pop_front (void) {
		ASSERT_SAFE (!empty ());
		extract (root.next);
	}
	void pop_back (void) {
		ASSERT_SAFE (!empty ());
		extract (root.prev);
	}

	class iterator : public std::iterator<std::bidirectional_iterator_tag, T> {
	private:
		Element * current;

	public:
		iterator (Element * p = nullptr) : current (p) {}
		bool operator==(iterator other) const { return current == other.current; }
		bool operator!=(iterator other) const { return current != other.current; }
		iterator & operator++(void) {
			current = current->next;
			return *this;
		}
		iterator & operator--(void) {
			current = current->prev;
			return *this;
		}
		iterator operator++(int) {
			auto cpy = *this;
			++*this;
			return cpy;
		}
		iterator operator--(int) {
			auto cpy = *this;
			--*this;
			return cpy;
		}
		T & operator*(void) const { return *static_cast<T *> (current); }
		T * operator->(void) const { return static_cast<T *> (current); }
	};

	iterator begin (void) { return {root.next}; }
	iterator end (void) { return {&root}; }

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
	static void cross (Element * left, Element * right) {
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
	static void extract (Element * link) { cross (link, link); }
};

template <typename T, size_t exact_size_slot_nb> struct QuickList {
	/* Quicklist is used to quickly find an element with a specific attribute.
	 * It uses a set of lists, that each store elements if their size() property has the same value
	 * as the list's value.
	 * The set of list is fixed, and is composed of exact_size_slot_nb lists for lengths from 1 to
	 * exact_size_slot_nb, plus another one (always sorted in increasing order) for all bigger
	 * sizes.
	 *
	 * T must have a size () method
	 */
public:
	struct Tag; // Custom tag for Chain
	using ListType = Chain<T, Tag>;
	using Element = typename ListType::Element;

	// Default ctor ok ; others deleted due to Chain

	void insert (T & element) {
		size_t len = element.size ();
		ASSERT_SAFE (len > 0);
		if (len <= exact_size_slot_nb) {
			exact_size_slots[len - 1].push_front (element);
		} else {
			// Insert it sorted in increasing size
			for (auto & t : bigger_sizes) {
				if (t.size () >= len) {
					ListType::insert_before (element, t);
					return;
				}
			}
			bigger_sizes.push_back (element);
		}
		stored_size += len;
	}
	T * take (size_t min_len) {
		ASSERT_SAFE (min_len > 0);
		// Search in exact size slots
		for (size_t n = min_len; n <= exact_size_slot_nb; ++n) {
			if (!exact_size_slots[n - 1].empty ()) {
				T * t = &exact_size_slots[n - 1].front ();
				exact_size_slots[n - 1].pop_front ();
				stored_size -= n;
				return t;
			}
		}
		// Search in higher sizes list
		for (auto & t : bigger_sizes) {
			if (t.size () >= min_len) {
				ListType::unlink (t);
				stored_size -= t.size ();
				return &t;
			}
		}
		return nullptr;
	}
	void remove (T & t) {
		stored_size -= t.size ();
		ListType::unlink (t);
	}

	// Cumulated size stored in the quicklist
	size_t size (void) const { return stored_size; }

private:
	ListType exact_size_slots[exact_size_slot_nb];
	ListType bigger_sizes;
	size_t stored_size{0};
};

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
	Element * head;

public:
	explicit ForwardChain (Element * head_ = nullptr) : head (head_) {}

	bool empty (void) const { return head != nullptr; }
	T & front (void) {
		ASSERT_SAFE (!empty ());
		return *static_cast<T *> (head);
	}
	void pop_front (void) {
		ASSERT_SAFE (!empty ());
		head = head->next;
	}
	void push_front (T & t) {
		Element & e = t;
		e.next = head;
		head = &e;
	}

	class iterator : public std::iterator<std::forward_iterator_tag, T> {
	private:
		Element * current;

	public:
		// default construction is equivalent to a end ptr.
		explicit iterator (Element * p = nullptr) : current (p) {}
		bool operator==(iterator other) { return current == other.current; }
		bool operator!=(iterator other) { return current != other.current; }
		iterator & operator++(void) {
			current = current->next;
			return *this;
		}
		iterator operator++(int) {
			auto cpy = *this;
			++*this;
			return cpy;
		}
		T & operator*(void) { return *static_cast<T *> (current); }
		T * operator->(void) { return static_cast<T *> (current); }
	};

	iterator begin (void) { return iterator (head); }
	iterator end (void) { return iterator (); }

	class Atomic {
	private:
		std::atomic<Element *> head{nullptr};

	public:
		void push_front (T & t) {
			Element & e = t;
			Element * expected = head.load (std::memory_order_relaxed);
			do {
				e.next = expected;
			} while (!head.compare_exchange_weak (expected, &e, std::memory_order_release,
			                                      std::memory_order_relaxed));
		}

		ForwardChain take_all (void) {
			Element * previous = head.load (std::memory_order_relaxed);
			while (!head.compare_exchange_weak (previous, nullptr, std::memory_order_acquire,
			                                    std::memory_order_relaxed))
				;
			return ForwardChain (previous);
		}
	};
};
}

#endif
