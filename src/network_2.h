#pragma once
#ifndef GIVY_NETWORK_H
#define GIVY_NETWORK_H

#include <new>
#include <utility>
#include <thread>
#include <condition_variable>

#include <memory>
#include <mutex>

#include "intrusive_list.h"
#include "reporting.h"

namespace Givy {

/* Messages are stored inline with some metadata and list element.
 * Here are some templates to create the Message type with respect to a Payload type.
 * And also to extract the Payload part for setup.
 */
template <typename Header, typename Payload> struct MessageLayout {
	MessageLayout () = delete; // detect bad use of this type
	Header header;
	Payload payload;
};

struct Message;
using MessageQueue = typename Intrusive::ForwardList<Message>;
struct Message : public MessageQueue::Element {
	void * data;
	size_t size;
	size_t remote_node;

	Message (void * data, size_t size) : data (data), size (size) {}
	template <typename Payload> Payload & as_payload (void) {
		return reinterpret_cast<MessageLayout<Message, Payload> *> (this)->payload;
	}
};
template <typename Payload> Message * make_message (void) {
	// Build it to right size, init message data info
	auto msg = static_cast<Message *> (::operator new (sizeof (MessageLayout<Message, Payload>)));
	new (msg) Message (&msg->as_payload<Payload> (), sizeof (Payload));
	return msg;
}

class Network {
private:
	size_t m_node_id;
	size_t m_nb_node;
	MessageQueue::Atomic m_send_queue;


	std::thread m_thread;

	std::condition_variable var;
	std::mutex mutex;


	static constexpr int protocol_tag{42};

public: // Can be called from everywhere
	Network (int & argc, char **& argv);
	Network (const Network &) = delete;
	Network (Network &&) = delete;
	~Network ();
	Network & operator= (const Network &) = delete;
	Network & operator= (Network &&) = delete;

	size_t node_id (void) const { return m_node_id; }
	size_t nb_node (void) const { return m_nb_node; }

	void send (Message & msg) { m_send_queue.push_front (msg); }

	template <typename Payload, typename... Args>
	void build_and_send_to (size_t destination, Args &&... args) {
		auto msg = make_message<Payload> ();
		new (&msg->as_payload<Payload> ()) Payload (std::forward<Args> (args)...);
		msg->remote_node = destination;
		send (*msg);
	}

	void start_thread (void);

// Private

	void send_to (size_t to, void * data, size_t size) {
		std::lock_guard<std::mutex> lock (mutex);
		DEBUG_TEXT ("[N%d] sending %zu bytes to %zu\n", comm_rank, size, to);
		MPI_Send (data, size, MPI_BYTE, to, protocol_tag, MPI_COMM_WORLD);
	}

	std::unique_ptr<char[]> try_recv (size_t & from) {
		std::lock_guard<std::mutex> lock (mutex);
		std::unique_ptr<char[]> data;
		int flag = 0;
		MPI_Status status;
		MPI_Message message_handle;
		MPI_Improbe (MPI_ANY_SOURCE, protocol_tag, MPI_COMM_WORLD, &flag, &message_handle, &status);
		if (flag) {
			from = static_cast<size_t> (status.MPI_SOURCE);
			int s;
			MPI_Get_count (&status, MPI_BYTE, &s);
			auto size = static_cast<size_t> (s);
			data.reset (new char[size]);
			MPI_Mrecv (data.get (), size, MPI_BYTE, &message_handle, MPI_STATUSES_IGNORE);
		}
		return data;
	}

};
}

#endif
