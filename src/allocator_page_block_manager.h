#pragma once
#ifndef GIVY_ALLOCATOR_PAGE_BLOCK_MANAGER_H
#define GIVY_ALLOCATOR_PAGE_BLOCK_MANAGER_H

#include "types.h"
#include "array.h"
#include "allocator_defs.h"

namespace Givy {
namespace Intrusive {

	// TODO
	// Index list, index quicklist
	// Page block manager : merges, cut top part everytime (avoids modifying too much offset fields)
	// Use a dynamically constructible union for small alloc data

	template <typename T, size_t max_index, typename Tag = void> class IndexList {
	private:
		using Index = BoundUint<max_index>;
		static constexpr Index invalid = max_index;

	public:
		struct Element {
			Index prev;
			Index next;

			// Compared to List, no default values (this is not a ring)
			// No destructor check either...

			// Copy/move is meaningless
			Element (const Element &) = delete;
			Element & operator=(const Element &) = delete;
			Element (Element &&) = delete;
			Element & operator=(Element &&) = delete;
		};

	private:
		Index first{invalid};
		Index last{invalid};

	public:
		// Default ctor ok

		// Copy/move is meaningless
		IndexList (const IndexList &) = delete;
		IndexList & operator=(const IndexList &) = delete;
		IndexList (IndexList &&) = delete;
		IndexList & operator=(IndexList &&) = delete;

		bool empty (void) const { return first == invalid; }
		template <typename Deref> T & front (Deref & d) {
			ASSERT_SAFE (!empty ());
			return d[first];
		}
		template <typename Deref> T & back (Deref & d) {
			ASSERT_SAFE (!empty ());
			return d[last];
		}

		template<typename Deref> 
		void push_front (T & t, Deref& d) {

		}
	};

	template <typename T, size_t max_index, size_t exact_slot_nb> class IndexQuickList {
	private:
		struct Tag;
		using ListType = IndexList<T, max_index, Tag>;

	public:
		using Element = typename ListType::Element;

		void insert (T & element) {}

		T * take (size_t min_sz) {}
	};
}

namespace Allocator {

	template <typename T, size_t N> class PageBlockManager {
	public:
		struct PageBlockHeader : public T {
			// Page block info
			MemoryType type;          // Type of page block
			BoundUint<N> nb_page;     // Size of page block
			BoundUint<N> page_offset; // Offset in page block
		};

	private:
		StaticArray<PageBlockHeader, N> table;

	public:
	private:
		size_t index (const PageBlockHeader * pbh) const { return pbh - table.data (); }
		size_t index (const PageBlockHeader & pbh) const { return index (&pbh); }
	};
}
}

#endif
