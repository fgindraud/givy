#ifndef INTRUSIVE_LIST_H
#define INTRUSIVE_LIST_H

#include <iterator>
#include <atomic>
#include <tuple>

#include "reporting.h"

namespace Givy {
namespace Intrusive {

	template <typename T, typename Tag = void> class ForwardList {
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
		explicit ForwardList (Element * head_ = nullptr) : head (head_) {}

		bool empty (void) const { return head == nullptr; }
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
		void clear (void) { head = nullptr; }

		/* iterator/const_iterator
		 */
		template <typename Type, typename Elem>
		class iterator_base : public std::iterator<std::forward_iterator_tag, Type> {
		private:
			Elem * current;

			iterator_base (Elem * p) : current (p) {}
			friend class ForwardList;

		public:
			iterator_base () : iterator_base (nullptr) {}
			bool operator==(iterator_base other) { return current == other.current; }
			bool operator!=(iterator_base other) { return current != other.current; }
			iterator_base & operator++(void) {
				current = current->next;
				return *this;
			}
			iterator_base operator++(int) {
				auto cpy = *this;
				++*this;
				return cpy;
			}
			Type & operator*(void) { return *static_cast<Type *> (current); }
			Type * operator->(void) { return static_cast<Type *> (current); }
		};
		using iterator = iterator_base<T, Element>;
		using const_iterator = iterator_base<const T, const Element>;

		iterator begin (void) { return {head}; }
		iterator end (void) { return {nullptr}; }
		const_iterator begin (void) const { return {head}; }
		const_iterator end (void) const { return {nullptr}; }

		/* Atomic single linked list, supporting only push and take_all.
		 * Is impervious to ABA problem as there is no pop ().
		 */
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

			ForwardList take_all (void) {
				Element * previous = head.load (std::memory_order_relaxed);
				while (!head.compare_exchange_weak (previous, nullptr, std::memory_order_acquire,
				                                    std::memory_order_relaxed))
					;
				return ForwardList (previous);
			}
		};
	};

	template <typename T, typename Tag = void> class List {
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
			~Element () {
				/* FIXME
				 * This assert requires that lists be empty at destruction.
				 * In particular, in the allocator, it requires that everything is deallocated before
				 * allocator destruction.
				 * This is very constraining, but useful for testing to detect metadata failures.
				 * It should be removed for working build.
				 */
				ASSERT_SAFE (next == this);
			}
			// Copy/move is meaningless
			Element (const Element &) = delete;
			Element & operator=(const Element &) = delete;
			Element (Element &&) = delete;
			Element & operator=(Element &&) = delete;
		};

	private:
		/* Element structs forms a double linked circular chain. */
		Element root;

	public:
		// Default ctor ok ; others are deleted due to Element

		// Headless insert/remove
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
		void remove (T & t) { unlink (t); }

		/* iterator/const_iterator
		 */
		template <typename Type, typename Elem>
		class iterator_base : public std::iterator<std::bidirectional_iterator_tag, Type> {
		private:
			Elem * current;

			iterator_base (Elem * p) : current (p) {}
			friend class List;

		public:
			iterator_base () : Elem (nullptr) {}
			bool operator==(iterator_base other) const { return current == other.current; }
			bool operator!=(iterator_base other) const { return current != other.current; }
			iterator_base & operator++(void) {
				current = current->next;
				return *this;
			}
			iterator_base & operator--(void) {
				current = current->prev;
				return *this;
			}
			iterator_base operator++(int) {
				auto cpy = *this;
				++*this;
				return cpy;
			}
			iterator_base operator--(int) {
				auto cpy = *this;
				--*this;
				return cpy;
			}
			Type & operator*(void) const { return *static_cast<Type *> (current); }
			Type * operator->(void) const { return static_cast<Type *> (current); }
		};
		using iterator = iterator_base<T, Element>;
		using const_iterator = iterator_base<const T, const Element>;

		iterator begin (void) { return {root.next}; }
		iterator end (void) { return {&root}; }
		const_iterator begin (void) const { return {root.next}; }
		const_iterator end (void) const { return {&root}; }

	private:
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
		struct Tag; // Custom tag for List
		using ListType = List<T, Tag>;
		using Element = typename ListType::Element;

	private:
		ListType exact_size_slots[exact_size_slot_nb];
		ListType bigger_sizes;
		size_t stored_size{0};

	public:
		// Default ctor ok ; others deleted due to List

		void insert (T & element) {
			size_t sz = element.size ();
			ASSERT_SAFE (sz > 0);
			stored_size += sz;
			if (sz <= exact_size_slot_nb) {
				exact_size_slots[sz - 1].push_front (element);
			} else {
				// Insert it sorted in increasing size
				for (auto & t : bigger_sizes) {
					if (t.size () >= sz) {
						ListType::insert_before (element, t);
						return;
					}
				}
				bigger_sizes.push_back (element);
			}
		}
		T * take (size_t min_sz) {
			ASSERT_SAFE (min_sz > 0);
			// Search in exact size slots
			for (size_t n = min_sz; n <= exact_size_slot_nb; ++n) {
				if (!exact_size_slots[n - 1].empty ()) {
					T * t = &exact_size_slots[n - 1].front ();
					exact_size_slots[n - 1].pop_front ();
					stored_size -= n;
					return t;
				}
			}
			// Search in higher sizes list
			for (auto & t : bigger_sizes) {
				if (t.size () >= min_sz) {
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
	};
}
}

#endif
