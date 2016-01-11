#ifndef COHERENCE_H
#define COHERENCE_H

#include <bitset>

#include "types.h"
#include "pointer.h"

namespace Givy {
namespace Coherence {

	namespace {
		constexpr size_t MaxSupportedNode = 64;
	}

	struct RegionMetadata {
		Ptr ptr;
		std::bitset<MaxSupportedNode> valid_set; // For owner only
		BoundUint<MaxSupportedNode> owner;
		bool valid;
	};

	/* Coherence messages
	 */
	enum class MessageType : uint8_t {
		DataRequest,
		DataAnswer,
		OwnerRequest,
		OwnerTransfer,
		InvalidationRequest,
		InvalidationAck
	};

	struct DataRequestMsg {
		MessageType type; // Must be DataRequest
	};
	struct DataAnswerMsg {
		MessageType type; // Must be DataAnswer
	};
	struct OwnerRequestMsg {
		MessageType type; // Must be OwnerRequest
	};
	struct OwnerTransferMsg {
		MessageType type; // Must be OwnerTransfer
	};
	struct InvalidationRequestMsg {
		MessageType type; // Must be InvalidationRequest
	};
	struct InvalidationAckMsg {
		MessageType type; // Must be InvalidationAck
	};

	union Message {
		MessageType type;
		DataRequestMsg data_request;
		DataAnswerMsg data_answer;
		OwnerRequestMsg owner_request;
		OwnerTransferMsg owner_transfer;
		InvalidationRequestMsg invalidation_request;
		InvalidationAckMsg invalidation_ack;
	};
}
}

#endif
