#define ASSERT_LEVEL_SAFE

#include <cci.h>
#include <string>
#include <vector>
#include <cstdint>
#include <unistd.h>

#include "reporting.h"
#include "range.h"
#include "name_server.h"

namespace {
struct client {
	std::string uri{};
	cci_connection_t * connection{nullptr};
};

size_t send_registry (cci_endpoint_t * ep, std::vector<client> & clients) {
	using namespace Givy;

	size_t sent = 0;
	for (auto i : range<size_t> (clients.size ())) {
		NameServer::msg_uri msg{clients.size (), i, clients[i].uri.c_str ()};
		for (auto j : range<size_t> (clients.size ())) {
			if (i != j) {
				// Send msg, except for the initial emitter
				int r_send = cci_send (clients[j].connection, &msg, sizeof (msg), nullptr, 0);
				if (r_send != CCI_SUCCESS)
					FAILURE ("cci_send: %s", cci_strerror (ep, static_cast<cci_status_t> (r_send)));
				DEBUG_TEXT ("[server] Send %zu to %zu\n", i, j);
				sent++;
			}
		}
	}
	return sent;
}

void do_name_server (void) {
	using namespace Givy;
	cci_endpoint_t * end_point{nullptr};

	// Init
	uint32_t caps = 0; // Ignored
	int r_init = cci_init (CCI_ABI_VERSION, 0, &caps);
	if (r_init != CCI_SUCCESS)
		FAILURE ("cci_init: %s", cci_strerror (nullptr, static_cast<cci_status_t> (r_init)));
	int r_create_endpoint = cci_create_endpoint (nullptr, 0, &end_point, nullptr);
	if (r_create_endpoint != CCI_SUCCESS)
		FAILURE ("cci_create_endpoint: %s",
		         cci_strerror (nullptr, static_cast<cci_status_t> (r_create_endpoint)));
	// Print uri and kill stdout
	char * uri{nullptr};
	int r_uri = cci_get_opt (end_point, CCI_OPT_ENDPT_URI, &uri);
	if (r_uri != CCI_SUCCESS)
		FAILURE ("cci_get_opt: %s", cci_strerror (end_point, static_cast<cci_status_t> (r_uri)));
	ASSERT_STD (uri != nullptr);
	printf ("%s\n", uri);
	free (uri);
	fclose (stdout);

	std::vector<client> clients;
	size_t registered = 0;
	size_t connected = 0;
	size_t waiting_sent_signal = 0;
	bool stop = false;

	while (!stop) {
		// Event
		cci_event_t * event{nullptr};
		int r_event = cci_get_event (end_point, &event);
		if (r_event == CCI_EAGAIN)
			continue;
		ASSERT_STD (r_event == CCI_SUCCESS);
		ASSERT_STD (event != nullptr);
		switch (event->type) {
		case CCI_EVENT_CONNECT_REQUEST: {
			// Check msg
			auto msg = static_cast<const NameServer::msg_uri *> (event->request.data_ptr);
			ASSERT_STD (msg != nullptr);
			ASSERT_STD (event->request.data_len == sizeof (*msg));
			ASSERT_STD (msg->nb_node > 0);
			ASSERT_STD (msg->node_id < msg->nb_node);
			ASSERT_STD (registered == 0 || registered < clients.size ());
			// Accept connection
			int r_accept = cci_accept (event, reinterpret_cast<const void *> (msg->node_id));
			if (r_accept != CCI_SUCCESS)
				FAILURE ("cci_accept: %s", cci_strerror (end_point, static_cast<cci_status_t> (r_accept)));
			// Size the uri vector at first message
			if (registered == 0)
				clients.resize (msg->nb_node);
			ASSERT_STD (clients.size () == msg->nb_node);
			// Add uri to list
			ASSERT_STD (clients[msg->node_id].uri.empty ());
			clients[msg->node_id].uri = msg->uri;
			registered++;
			INFO_TEXT ("[server][%u/%u] Registered %s\n", msg->node_id, msg->nb_node, msg->uri);
		} break;
		case CCI_EVENT_ACCEPT: {
			// Finalize connnection
			auto node_id = reinterpret_cast<const size_t> (event->accept.context);
			clients.at (node_id).connection = event->accept.connection;
			connected++;
			// If everyone arrived, send the registery to all
			if (connected == clients.size ()) {
				DEBUG_TEXT ("[server] Broadcasting registry\n");
				waiting_sent_signal = send_registry (end_point, clients);
			}
		} break;
		case CCI_EVENT_SEND: {
			// Terminate when all has been sent
			ASSERT_STD (event->send.status == CCI_SUCCESS);
			ASSERT_STD (waiting_sent_signal > 0);
			waiting_sent_signal--;
			if (waiting_sent_signal == 0)
				stop = true;
		} break;
		default:
			break;
		}
		cci_return_event (event);
	}

	for (auto & c : clients)
		if (c.connection != nullptr)
			cci_disconnect (c.connection);

	// Cleanup
	if (end_point)
		cci_destroy_endpoint (end_point);
	cci_finalize ();
	DEBUG_TEXT ("[server] Finished\n");
}
}

int main (void) {
	// Fork
	int f = fork ();
	if (f > 0) {
		// Parent just dies
	} else if (f == 0) {
		// Child
		do_name_server ();
	} else {
		FAILURE ("fork");
	}
	return EXIT_SUCCESS;
}
