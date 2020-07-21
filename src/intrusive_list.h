#pragma once
#ifndef GIVY_INTRUSIVE_LIST_H
#define GIVY_INTRUSIVE_LIST_H

#include <atomic>
#include <iterator>
#include <mutex>
#include <tuple>

#include "concurrency.h"
#include "reporting.h"

namespace Givy {
namespace Intrusive {
	/* === for the following Lists ===
	 * Any struct that wants to be added to the list must inherit from Element.
	 * Tag allows a T object to participate in multiple lists (differentiate the element type).
	 *
	 * StackList, ForwardList, AtomicList share the same element type so they can share elements.
	 * List is a double linked list and has a different element type.
	 */

	template <typename T, typename Tag> class StackList;
	template <typename T, typename Tag> class ForwardList;
	template <typename T, typename Tag> class AtomicForwardList;

	template <typename T, typename Tag> class List;

	/* Forward list family.
	 */
	template <typename T, typename Tag = void> struct ForwardListElement {
		// Non copyable, non movable, no ownership
		ForwardListElement () = default;
		ForwardListElement (const ForwardListElement &) = delete;
		ForwardListElement (ForwardListElement &&) = delete;
		ForwardListElement & operator= (const ForwardListElement &) = delete;
		ForwardListElement & operator= (ForwardListElement &&) = delete;
		~ForwardListElement () = default;
		ForwardListElement * next;
	};

	// TODO configurable_list ?

	/* Stack structure (pop,push,iterate over), using links (not array).
	 * Smallest overhead:
	 * - one pointer per element.
	 * - and one pointer for the head.
	 */
	template <typename T, typename Tag = void> class StackList {
	public:
		using Element = ForwardListElement<T, Tag>;
		using Atomic = AtomicForwardList<T, Tag>;

	private:
		Element * m_head;

		explicit StackList (Element * head) : m_head (head) {}
		friend class AtomicForwardList<T, Tag>; // for private constructor

	public:
		// Movable only, no ownership
		StackList () : StackList (nullptr) {}
		StackList (const StackList &) = delete;
		StackList (StackList && other) : StackList (other.m_head) { other.clear (); }
		StackList & operator= (const StackList &) = delete;
		StackList & operator= (StackList && other) {
			m_head = other.m_head;
			other.clear ();
			return *this;
		}
		~StackList () = default;

		bool empty (void) const { return m_head == nullptr; }
		T & front (void) {
			ASSERT_SAFE (!empty ());
			return *static_cast<T *> (m_head);
		}
		void pop_front (void) {
			ASSERT_SAFE (!empty ());
			m_head = m_head->next;
		}
		void push_front (T & t) {
			Element & e = t;
			e.next = m_head;
			m_head = &e;
		}
		void clear (void) { m_head = nullptr; }

		// iterator/const_iterator
		template <typename Type, typename Elem>
		class iterator_base : public std::iterator<std::forward_iterator_tag, Type> {
		private:
			Elem * current;

			iterator_base (Elem * p) : current (p) {}
			friend class StackList; // For private constructor

		public:
			iterator_base () : iterator_base (nullptr) {}
			iterator_base (Elem & e) : iterator_base (&e) {} // From element
			bool operator== (iterator_base other) const { return current == other.current; }
			bool operator!= (iterator_base other) const { return !(*this == other); }
			iterator_base & operator++ (void) {
				current = current->next;
				return *this;
			}
			iterator_base operator++ (int) {
				auto cpy = *this;
				++*this;
				return cpy;
			}
			Type & operator* (void) const { return *static_cast<Type *> (current); }
			Type * operator-> (void) const { return static_cast<Type *> (current); }
		};
		using iterator = iterator_base<T, Element>;
		using const_iterator = iterator_base<const T, const Element>;

		iterator begin (void) { return {m_head}; }
		iterator end (void) { return {nullptr}; }
		const_iterator begin (void) const { return {m_head}; }
		const_iterator end (void) const { return {nullptr}; }
	};

	/* TODO descr
	 *
	 * A true
	 * "advanced" version of the ForwardList.
	 * Storage per node is unchanged, but the head and iterators have more data.
	 * In allows to forward remove/insert by iterator.
	 * It uses the same Element type as ForwardList (object can switch from one to the other).
	 */
	template <typename T, typename Tag = void> class ForwardList {
	public:
		using Element = ForwardListElement<T, Tag>;
		using Atomic = AtomicForwardList<T, Tag>;

	private:
		Element * m_head;
		Element * m_tail;

		ForwardList (Element * head, Element * tail) : m_head (head), m_tail (tail) {}

	public:
		// Movable only type, no ownership
		ForwardList () : ForwardList (nullptr, nullptr) {}
		ForwardList (const ForwardList &) = delete;
		ForwardList (ForwardList && other) : ForwardList (other.m_head, other.m_tail) {
			other.clear ();
		}
		ForwardList & operator= (const ForwardList &) = delete;
		ForwardList & operator= (ForwardList && other) {
			m_head = other.m_head;
			m_tail = other.m_tail;
			other.clear ();
			return *this;
		}
		~ForwardList () = default;

		// Build from other
		explicit ForwardList (StackList<T, Tag> && stack) : ForwardList (stack.m_head, nullptr) {
			if (!stack.empty ()) {
				// Find tail
				auto next = stack.begin ();
				decltype (next) it;
				while (next != stack.end ()) {
					it = next;
					++next;
				}
				m_tail = &(*it);
				stack.clear ();
			}
		}

		bool empty (void) const { return m_head == nullptr; }
		T & front (void) {
			ASSERT_SAFE (!empty ());
			return *static_cast<T *> (m_head);
		}
		T & back (void) {
			ASSERT_SAFE (!empty ());
			return *static_cast<T *> (m_tail);
		}
		void pop_front (void) {
			ASSERT_SAFE (!empty ());
			m_head = m_head->next;
			if (m_head == nullptr)
				m_tail = nullptr;
		}
		void push_front (T & t) {
			Element & e = t;
			e.next = m_head;
			m_head = &e;
			if (m_tail == nullptr)
				m_tail = m_head;
		}
		void push_back (T & t) {}
		void clear (void) { m_head = m_tail = nullptr; }

		// iterator/const_iterator
		template <typename Type, typename Elem>
		class iterator_base : public std::iterator<std::forward_iterator_tag, Type> {
		private:
			Elem * current;
			Elem ** ptr_to_current;

			iterator_base (Elem * e, Elem * p_to_e) : current (e), ptr_to_current (p_to_e) {}
			friend class ForwardList; // For private constructor

		public:
			iterator_base () : iterator_base (nullptr, nullptr) {}
			bool operator== (iterator_base other) const { return current == other.current; }
			bool operator!= (iterator_base other) const { return !(*this == other); }
			iterator_base & operator++ (void) {
				ptr_to_current = &(current->next);
				current = current->next;
				return *this;
			}
			iterator_base operator++ (int) {
				auto cpy = *this;
				++*this;
				return cpy;
			}
			Type & operator* (void) const { return *static_cast<Type *> (current); }
			Type * operator-> (void) const { return static_cast<Type *> (current); }
		};
		using iterator = iterator_base<T, Element>;
		using const_iterator = iterator_base<const T, const Element>;

		iterator begin (void) { return {m_head, &m_head}; }
		iterator end (void) { return {nullptr, nullptr}; }
		const_iterator begin (void) const { return {m_head, &m_head}; }
		const_iterator end (void) const { return {nullptr, nullptr}; }

		iterator insert_before (iterator it, T & t) {
			// insert before it, returns new iterator pointing to *it
			ASSERT_SAFE (it->current != nullptr);
			ASSERT_SAFE (*(it->ptr_to_current) == it->current);
			Element & e = t;
			e.next = it->current;
			*(it->ptr_to_current) = &e;
			it->ptr_to_current = &(e.next);
			return it;
		}
		iterator remove_go_next (iterator it) {
			// remove current element from list, return iterator to next
			ASSERT_SAFE (it->current != nullptr);
			ASSERT_SAFE (*(it->ptr_to_current) == it->current);
			it->current = it->current.next;
			*(it->ptr_to_current) = it->current;
			return it;
		}
	};

	/* Atomic single linked list structure.
	 * Supports:
	 * - push an element
	 * - push a sequence of element (ForwardList as we need the tail)
	 * - take all elements (as a StackList)
	 *
	 * It is impervious to ABA problem as there is no pop ().
	 */
	template <typename T, typename Tag = void> class AtomicForwardList {
	public:
		using Element = ForwardListElement<T, Tag>;

	private:
		std::atomic<Element *> head{nullptr};

	public:
		// push_front returns true if was empty
		bool push_front (T & t) {
			Element & e = t;
			Element * expected = head.load (std::memory_order_relaxed);
			do {
				e.next = expected;
			} while (!head.compare_exchange_weak (expected, &e, std::memory_order_release,
			                                      std::memory_order_relaxed));
			return expected == nullptr;
		}
		bool push_front (ForwardList<T, Tag> && list) {
			// Expects a non-empty list
			ASSERT_SAFE (!list.empty ());
			Element & h = list.front ();
			Element & t = list.back ();
			list.clear (); // Empty the list
			Element * expected = head.load (std::memory_order_relaxed);
			do {
				t.next = expected;
			} while (!head.compare_exchange_weak (expected, &h, std::memory_order_release,
			                                      std::memory_order_relaxed));
			return expected == nullptr;
		}

		StackList<T, Tag> take_all (void) {
			Element * previous = head.load (std::memory_order_relaxed);
			while (!head.compare_exchange_weak (previous, nullptr, std::memory_order_acquire,
			                                    std::memory_order_relaxed))
				;
			return StackList<T, Tag> (previous);
		}
	};

	// TODO unique_lock ?

	/* Double linked list.
	 * Elements and head size are bigger.
	 */
	template <typename T, typename Tag = void> class List {
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
			Element & operator= (const Element &) = delete;
			Element (Element &&) = delete;
			Element & operator= (Element &&) = delete;
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
			bool operator== (iterator_base other) const { return current == other.current; }
			bool operator!= (iterator_base other) const { return current != other.current; }
			iterator_base & operator++ (void) {
				current = current->next;
				return *this;
			}
			iterator_base & operator-- (void) {
				current = current->prev;
				return *this;
			}
			iterator_base operator++ (int) {
				auto cpy = *this;
				++*this;
				return cpy;
			}
			iterator_base operator-- (int) {
				auto cpy = *this;
				--*this;
				return cpy;
			}
			Type & operator* (void) const { return *static_cast<Type *> (current); }
			Type * operator-> (void) const { return static_cast<Type *> (current); }
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

	public:
		/* Atomic list protected by a lock.
		 * TODO split from List
		 */
		class Atomic {
		private:
			SpinLock mutex;
			List list;

		public:
			void push_front (T & t) {
				std::lock_guard<decltype (mutex)> lock (mutex);
				list.push_front (t);
			}
			void push_back (T & t) {
				std::lock_guard<decltype (mutex)> lock (mutex);
				list.push_back (t);
			}
			void remove (T & t) {
				std::lock_guard<decltype (mutex)> lock (mutex);
				list.remove (t);
			}
			template <typename Callable> void apply_all (Callable && callable) {
				std::lock_guard<decltype (mutex)> lock (mutex);
				for (auto & t : list)
					callable (t);
			}
		};
	};

	/* Quicklist is used to quickly find an element with a specific attribute.
	 * It uses a set of lists, that each store elements if their size() property has the same value
	 * as the list's value.
	 * The set of list is fixed, and is composed of exact_size_slot_nb lists for lengths from 1 to
	 * exact_size_slot_nb, plus another one (always sorted in increasing order) for all bigger
	 * sizes.
	 *
	 * T must have a size () method
	 */
	template <typename T, size_t exact_size_slot_nb> struct QuickList {
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
