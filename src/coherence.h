#pragma once
#ifndef COHERENCE_H
#define COHERENCE_H

#include <atomic>
#include <bitset>
#include <map>
#include <mutex>
#include <thread>
#include <utility>

#include "allocator.h"
#include "block.h"
#include "intrusive_list.h"
#include "network.h"
#include "range.h"
#include "types.h"

namespace Givy {
namespace Coherence {

	class Waiter;
	using WaiterList = Intrusive::StackList<Waiter>;

	class Waiter : public WaiterList::Element {
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

	constexpr size_t max_supported_node = 64;

	struct RegionMetadata {
		Block blk;
		std::bitset<max_supported_node> valid_set; // For owner only
		BoundUint<max_supported_node> owner;
		bool valid;
		WaiterList::Atomic waiters;

		// Invalid region for ptr
		RegionMetadata (void * ptr, const Gas::Space & space)
		    : blk{ptr, 0}, owner (space.node_of_allocation (ptr)), valid (false) {}
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
		Deallocate,
		// Control
		NodeFinished,
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

	struct NodeFinishedMsg {
		MessageType type;
		size_t from;
	};

	class Manager {
	private:
		std::mutex mutex;

		const Gas::Space & space;
		Network & network;

		std::thread thread;

		/* metadata rationale:
		 * - if regions is created locally and has never been shared: no metadata
		 * - metadata is created at first need (DataReq / OwnerReq received)
		 * - metadata is destroyed only at Free
		 */
		std::map<void *, RegionMetadata> regions;

		/* Termination management : all nodes track the number of alive node.
		 * On finish, a node decrements its alive counter, and broadcasts to everyone to let them
		 * decrement theirs.
		 * On zero, exit.
		 */
		size_t nb_node_still_running;

		// ----------
	public:
		Manager (const Gas::Space & space, Network & network)
		    : space (space),
		      network (network),
		      thread ([=] { event_loop (); }),
		      nb_node_still_running (network.nb_node ()) {}

		~Manager () {
			// Send Finished messages
			{
				for (auto target : range (network.nb_node ()))
					if (target != network.node_id ()) {
						NodeFinishedMsg msg{MessageType::NodeFinished, network.node_id ()};
						network.send_to (target, &msg, sizeof (msg));
					}
				// No self message, so track ourselves
				std::lock_guard<std::mutex> lock (mutex);
				nb_node_still_running--;
				DEBUG_TEXT ("[N%zu] finished, count=%zu\n", network.node_id (), nb_node_still_running);
			}

			// Wait for system exit
			thread.join ();
		}

		void request_region_valid (void * ptr) {
			Waiter waiter;
			{
				std::lock_guard<std::mutex> lock (mutex);

				auto metadata = get_metadata (ptr);
				if (metadata) {
					if (metadata->valid)
						return; // Already valid
				} else {
					if (space.in_local_interval (ptr))
						return; // Valid and never share

					// No header and not local : construct in place
					metadata = create_metadata_invalid (ptr);
				}

				waiter.add_query ();
				if (metadata->waiters.push_front (waiter)) {
					// Send query if no entry was in the table
					DataRequestMsg msg{MessageType::DataRequest, ptr, network.node_id ()};
					network.send_to (space.node_of_allocation (ptr), &msg, sizeof (msg));
				}
			}
			waiter.wait ();
		}

	private:
		void on_data_request (const DataRequestMsg & msg) { std::lock_guard<std::mutex> lock (mutex); }

		// Under lock !
		RegionMetadata * get_metadata (void * ptr) {
			auto it = regions.find (ptr);
			if (it == regions.end ())
				return nullptr;
			else
				return &(it->second);
		}
		RegionMetadata * create_metadata_invalid (void * ptr) {
			return &(regions
			             .emplace (std::piecewise_construct, std::forward_as_tuple (ptr),
			                       std::forward_as_tuple (ptr, space))
			             .first->second);
		}

		void event_loop (void) {
			while (true) {
				std::lock_guard<std::mutex> lock (mutex);
				if (nb_node_still_running == 0) {
					// EXIT
					return;
				}

				size_t from;
				auto data = network.try_recv (from);
				if (!data)
					continue;
				auto buf = Ptr (data.get ());

				switch (buf.as_ref<MessageType> ()) {
				case MessageType::DataRequest: {
					on_data_request (buf.as_ref<DataRequestMsg> ());
				} break;
				case MessageType::NodeFinished: {
					nb_node_still_running--;
					DEBUG_TEXT ("[N%zu] Recv NodeFinished(%zu), count=%zu\n", network.node_id (), from,
					            nb_node_still_running);
				} break;
				default:
					break;
				}
			}
		}
	};

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
