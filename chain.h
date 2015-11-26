#ifndef CHAIN_H
#define CHAIN_H

#include <iterator>
#include <atomic>

#include "assert_level.h"

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
		Element () noexcept { reset (); }
		void reset (void) noexcept { prev = next = this; }
		~Element () noexcept { extract (this); /* remove from any list */ }
		// Copy/move is meaningless
		Element (const Element &) = delete;
		Element & operator=(const Element &) = delete;
		Element (Element &&) = delete;
		Element & operator=(Element &&) = delete;
	};

	static void insert_after (Element & to_insert, Element & e) noexcept { cross (&e, &to_insert); }
	static void insert_before (Element & to_insert, Element & e) noexcept { cross (&to_insert, &e); }
	static void unlink (Element & e) noexcept { extract (&e); }

	void push_front (T & t) noexcept { insert_after (t, root); }
	void push_back (T & t) noexcept { insert_before (t, root); }
	T * pop_front (void) noexcept {
		Element * e = root.next;
		if (e == &root) {
			return nullptr;
		} else {
			extract (e);
			return static_cast<T *> (e);
		}
	}
	T * pop_back (void) noexcept {
		Element * e = root.prev;
		if (e == &root) {
			return nullptr;
		} else {
			extract (e);
			return static_cast<T *> (e);
		}
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

	iterator begin (void) noexcept { return {root.next}; }
	iterator end (void) noexcept { return {&root}; }

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

template <typename T, size_t exact_size_slot_nb> struct QuickList {
	/* Quicklist is used to quickly find an element with a specific attribute.
	 * It uses a set of lists, that each store elements if their length() property has the same value
	 * as the list's value.
	 * The set of list is fixed, and is composed of exact_size_slot_nb lists for lengths from 1 to
	 * exact_size_slot_nb, plus another one (always sorted in increasing order) for all bigger
	 * lengths.
	 */
	struct Tag; // Custom tag for Chain
	using ListType = Chain<T, Tag>;
	using Element = typename ListType::Element;

	ListType exact_size_slots[exact_size_slot_nb];
	ListType bigger_sizes;
	size_t stored_length = 0;

	// T must have a length () method
	void insert (T & element) {
		size_t len = element.length ();
		ASSERT_SAFE (len > 0);
		if (len <= exact_size_slot_nb) {
			exact_size_slots[len - 1].push_front (element);
		} else {
			// Insert it sorted in increasing length
			for (auto & t : bigger_sizes) {
				if (t.length () >= len) {
					ListType::insert_before (element, t);
					return;
				}
			}
			bigger_sizes.push_back (element);
		}
		stored_length += len;
	}
	T * take (size_t min_len) {
		ASSERT_SAFE (min_len > 0);
		// Search in exact size slots
		for (size_t n = min_len; n <= exact_size_slot_nb; ++n) {
			if (T * t = exact_size_slots[n - 1].pop_front ()) {
				stored_length -= n;
				return t;
			}
		}
		// Search in higher sizes list
		for (auto & t : bigger_sizes) {
			if (t.length () >= min_len) {
				ListType::unlink (t);
				stored_length -= t.length ();
				return &t;
			}
		}
		return nullptr;
	}
	void remove (T & t) noexcept {
		stored_length -= t.length ();
		ListType::unlink (t);
	}
	size_t length (void) const { return stored_length; }
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
	ForwardChain (Element * head_ = nullptr) : head (head_) {}

	T * pop (void) noexcept {
		T * r = static_cast<T *> (head);
		if (head != nullptr)
			head = head->next;
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

	class Atomic {
	private:
		std::atomic<Element *> head;

	public:
		Atomic () : head (nullptr) {}

		void push (T & t) noexcept {
			Element & e = t;
			Element * expected = head.load (std::memory_order_relaxed);
			do {
				e.next = expected;
			} while (!head.compare_exchange_weak (expected, &e, std::memory_order_release,
			                                      std::memory_order_relaxed));
		}

		ForwardChain take_all (void) noexcept {
			Element * previous = head.load (std::memory_order_relaxed);
			while (!head.compare_exchange_weak (previous, nullptr, std::memory_order_acquire,
			                                    std::memory_order_relaxed))
				;
			return {previous};
		}
	};
};
}

#endif
