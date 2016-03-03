#ifndef COHERENCE_H
#define COHERENCE_H

#include <bitset>
#include <map>

#include "types.h"
#include "pointer.h"
#include "network.h"

namespace Givy {
namespace Coherence {

	constexpr size_t max_supported_node = 64;

	struct RegionMetadata {
		Ptr ptr;
		std::bitset<max_supported_node> valid_set; // For owner only
		BoundUint<max_supported_node> owner;
		bool valid;
	};

	/* Coherence messages.
	 */
	enum class MessageType : uint8_t {
		DataRequest,
		DataAnswer,
		OwnerRequest,
		OwnerTransfer,
		InvalidationRequest,
		InvalidationAck
	};

	struct DataRequestMsg {};
	struct DataAnswerMsg {};
	struct OwnerRequestMsg {};
	struct OwnerTransferMsg {};
	struct InvalidationRequestMsg {};
	struct InvalidationAckMsg {};

	struct Message {
		MessageType type;
		union {
			DataRequestMsg data_request;
			DataAnswerMsg data_answer;
			OwnerRequestMsg owner_request;
			OwnerTransferMsg owner_transfer;
			InvalidationRequestMsg invalidation_request;
			InvalidationAckMsg invalidation_ack;
		};
	};

	class Manager {
	private:
		Network & network;
		std::map<Ptr, RegionMetadata> regions;

	public:
		Manager (Network & network_) : network (network_) {}

		void event_loop (void) {

		}
	};
}
}

#endif
