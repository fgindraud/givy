#ifndef COHERENCE_H
#define COHERENCE_H

#include <bitset>
#include <map>
#include <atomic>
#include <mutex>
#include <thread>

#include "types.h"
#include "pointer.h"
#include "network.h"
#include "allocator.h"

namespace Givy {
namespace Coherence {

	constexpr size_t max_supported_node = 64;

	struct RegionMetadata {
		Block blk;
		std::bitset<max_supported_node> valid_set; // For owner only
		BoundUint<max_supported_node> owner;
		bool valid;

		static RegionMetadata invalid (Ptr ptr, const Gas::Space & space) {
			return {{ptr, 0},
			        {},
			        static_cast<BoundUint<max_supported_node>> (space.node_of_allocation (ptr)),
			        false};
		}
	};

	/* Coherence messages.
	 */
	enum class MessageType : uint8_t {
		// Protocol
		DataRequest,
		DataAnswer,
		OwnerRequest,
		OwnerTransfer,
		InvalidationRequest,
		InvalidationAck,
		// Others
		Deallocate
	};

	struct DataRequestMsg {
		MessageType type;
		void * ptr;
		size_t from;
	};
	struct DataAnswerMsg {
		MessageType type;
		void * ptr;
	};
	struct OwnerRequestMsg {
		MessageType type;
	};
	struct OwnerTransferMsg {
		MessageType type;
	};
	struct InvalidationRequestMsg {
		MessageType type;
	};
	struct InvalidationAckMsg {
		MessageType type;
	};
	struct DeallocateMsg {
		MessageType type;
		Block blk;
	};

	class Waiter {
	private:
		std::atomic<int> waiting_for{0};

	public:
		void add_query (void) { waiting_for.fetch_add (1, std::memory_order_relaxed); }
		void query_done (void) { waiting_for.fetch_sub (1, std::memory_order_relaxed); }
		void wait (void) {
			while (waiting_for.load (std::memory_order_acquire) > 0)
				;
		}
	};

	class Manager {
	private:
		std::mutex mutex;

		const Gas::Space & space;
		std::map<Ptr, RegionMetadata> regions;
		/* metadata rationale:
		 * - if regions is created locally and has never been shared: no metadata
		 * - metadata is created at first need (DataReq / OwnerReq received)
		 * - metadata is destroyed only at Free
		 */

		Network & network;
		std::multimap<Ptr, Waiter *> query_waiters;

		std::thread thread;

		// ----------
	public:
		Manager (const Gas::Space & space, Network & network)
		    : space (space), network (network), thread ([=] { event_loop (); }) {}

		~Manager () { thread.join (); }

		void request_region_valid (Ptr ptr) {
			Waiter waiter;
			{
				std::lock_guard<std::mutex> lock (mutex);

				auto metadata = regions.find (ptr);
				if (metadata != regions.end ()) {
					if (metadata->second.valid)
						return; // Already valid
				} else {
					if (space.in_local_interval (ptr))
						return; // Valid and never share
					// No header and not local
					metadata = regions.emplace (ptr, RegionMetadata::invalid (ptr, space)).first;
				}

				waiter.add_query ();
				auto query = query_waiters.find (ptr);
				query_waiters.emplace (ptr, &waiter);
				if (query == query_waiters.end ()) {
					// Send query if no entry was in the table
					DataRequestMsg msg{MessageType::DataRequest, ptr, network.node_id ()};
					network.send_to (space.node_of_allocation (ptr), &msg, sizeof (msg));
				}
			}
			waiter.wait ();
		}

	private:
		void event_loop (void);
	};

	inline void Manager::event_loop (void) {
		while (true) {
			std::lock_guard<std::mutex> lock (mutex);
			size_t from;
			auto data = network.try_recv (from);
			if (!data)
				continue;
			auto buf = Ptr (data.get ());

			switch (buf.as_ref<MessageType> ()) {
			case MessageType::DataRequest: {
				/* ----------- */
				
			} break;
			default:
				break;
			}
		}
	}

#if 0
		void event_loop (Allocator::ThreadLocalHeap & tlh) {
			while (true) {
				void * data;
				size_t from;
				if (network.try_recv (from, data)) {
					auto bufp = Ptr (data);
					switch (*bufp.as<MessageType *> ()) {
					case MessageType::DataRequest:
						on_data_request (*bufp.as<DataRequestMsg *> (), from, tlh);
						break;
					case MessageType::DataAnswer:
						on_data_answer (*bufp.as<DataAnswerMsg *> (), from, tlh);
						break;
					case MessageType::OwnerRequest:
						on_owner_request (*bufp.as<OwnerRequestMsg *> (), from, tlh);
						break;
					case MessageType::OwnerTransfer:
						on_owner_transfer (*bufp.as<OwnerTransferMsg *> (), from, tlh);
						break;
					case MessageType::InvalidationRequest:
						on_invalidation_request (*bufp.as<InvalidationRequestMsg *> (), from, tlh);
						break;
					case MessageType::InvalidationAck:
						on_invalidation_ack (*bufp.as<InvalidationAckMsg *> (), from, tlh);
						break;
					case MessageType::Deallocate:
						on_deallocate (*bufp.as<DeallocateMsg *> (), from, tlh);
						break;
					default:
						ASSERT_STD_FAIL ("unknown message type");
					}
					delete[] (char*) data;
				}
			}
		}

		void deallocate (Block blk, Allocator::ThreadLocalHeap & tlh) {
			auto * region = region_metadata (blk.ptr);
			if (region != nullptr) {
				ASSERT_STD (region->owner == network.node_id ());
				DeallocateMsg msg{MessageType::Deallocate, blk};
				for (auto i : range (network.nb_node ()))
					if (i != network.node_id ())
						network.send_to (i, Ptr (&msg), sizeof (msg));
				// TODO deallocate is tricky ; we do not know who knows about the block
			}
			tlh.deallocate (blk, space);
		}

		void ensure_can_read (Ptr ptr) {
		}

	private:
		RegionMetadata * region_metadata (Ptr p) {
			auto it = regions.find (p);
			if (it != regions.end ())
				return &it->second;
			else
				return nullptr;
		}
		void remove_metadata (Ptr p) { regions.erase (p); }

		void on_data_request (DataRequestMsg & msg, size_t from, Allocator::ThreadLocalHeap & tlh) {
			
		}
		void on_data_answer (DataAnswerMsg & msg, size_t from, Allocator::ThreadLocalHeap & tlh) {}
		void on_owner_request (OwnerRequestMsg & msg, size_t from, Allocator::ThreadLocalHeap & tlh) {}
		void on_owner_transfer (OwnerTransferMsg & msg, size_t from, Allocator::ThreadLocalHeap & tlh) {}
		void on_invalidation_request (InvalidationRequestMsg & msg, size_t from, Allocator::ThreadLocalHeap & tlh) {}
		void on_invalidation_ack (InvalidationAckMsg & msg, size_t from, Allocator::ThreadLocalHeap & tlh) {}
		void on_deallocate (DeallocateMsg & msg, size_t, Allocator::ThreadLocalHeap & tlh) {
			remove_metadata (msg.blk.ptr);
			tlh.deallocate (msg.blk, space);
		}
	};
#endif
}
}

#endif
