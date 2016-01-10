#ifndef COHERENCE_H
#define COHERENCE_H

#include <cci.h>
#include <cstdlib>
#include <sys/select.h>
#include <vector>

#include "reporting.h"
#include "gas_layout.h"
#include "range.h"
#include "name_server.h"

namespace Givy {

struct Network {
	const GasLayout & layout;
	cci_endpoint_t * end_point{nullptr};
	std::vector<cci_connection_t *> connections;

	static constexpr void * connect_cookie{reinterpret_cast<void *> (0xE000)};

	Network (const GasLayout & layout_) : layout (layout_), connections (layout.nb_node, nullptr) {
		// General setup
		uint32_t caps = 0; // Ignored
		check_cci (cci_init (CCI_ABI_VERSION, 0, &caps), "cci_init");
		check_cci (cci_create_endpoint (nullptr, 0, &end_point, nullptr), "cci_create_endpoint");
		establish_connections ();
	}
	~Network () {
		for (auto & c : connections)
			if (c != nullptr)
				cci_disconnect (c);
		if (end_point)
			cci_destroy_endpoint (end_point);
		cci_finalize ();
	}

	void check_cci (int ret, const char * func) {
		if (ret != CCI_SUCCESS)
			FAILURE ("%s: %s", func, cci_strerror (end_point, static_cast<cci_status_t> (ret)));
	}

	void establish_connections (void) {
		constexpr size_t connect_name_server_cookie = 0xFFFF;
		{
			// Get local uri
			char * uri{nullptr};
			check_cci (cci_get_opt (end_point, CCI_OPT_ENDPT_URI, &uri), "cci_get_opt");
			NameServer::msg_uri msg{layout.nb_node, layout.local_node, uri};
			free (uri);
			DEBUG_TEXT ("[givy][%zu/%zu] Endpoint %s\n", layout.local_node, layout.nb_node, msg.uri);
			// Connect and send request
			const char * name_server_uri = getenv ("GIVY_NAME_SERVER");
			ASSERT_STD (name_server_uri != nullptr);
			check_cci (cci_connect (end_point, name_server_uri, &msg, sizeof (msg),
			                        CCI_CONN_ATTR_RU, // only need reliable
			                        reinterpret_cast<void *> (connect_name_server_cookie), 0, nullptr),
			           "cci_connect");
		}

		cci_connection_t * name_server{nullptr};
		size_t waiting_connections = layout.nb_node - 1;

		while (waiting_connections > 0) {
			// Event
			cci_event_t * event{nullptr};
			int r_event = cci_get_event (end_point, &event);
			if (r_event == CCI_EAGAIN)
				continue;
			ASSERT_STD (r_event == CCI_SUCCESS);
			ASSERT_STD (event != nullptr);
			switch (event->type) {
			case CCI_EVENT_CONNECT: {
				auto cookie = reinterpret_cast<const size_t> (event->connect.context);
				if (cookie == connect_name_server_cookie) {
					ASSERT_STD (event->connect.status == CCI_SUCCESS);
					ASSERT_STD (name_server == nullptr);
					name_server = event->connect.connection;
				}
				if (cookie < layout.nb_node) {
					ASSERT_STD (cookie != layout.local_node);
					ASSERT_STD (connections[cookie] == nullptr);
					ASSERT_STD (waiting_connections > 0);
					connections[cookie] = event->connect.connection;
					waiting_connections--;
					DEBUG_TEXT ("[givy][%zu/%zu] Connected %zu\n", layout.local_node, layout.nb_node, cookie);
				}
			} break;
			case CCI_EVENT_CONNECT_REQUEST: {
				// Check msg
				auto id = static_cast<const size_t *> (event->request.data_ptr);
				ASSERT_STD (id != nullptr);
				ASSERT_STD (event->request.data_len == sizeof (*id));
				ASSERT_STD (*id < layout.local_node);
				check_cci (cci_accept (event, reinterpret_cast<const void *> (*id)), "cci_accept");
				DEBUG_TEXT ("[givy][%zu/%zu] Connect request %zu\n", layout.local_node, layout.nb_node, *id);
			} break;
			case CCI_EVENT_ACCEPT: {
				auto id = reinterpret_cast<const size_t> (event->accept.context);
				connections.at (id) = event->accept.connection;
				waiting_connections--;
				DEBUG_TEXT ("[givy][%zu/%zu] Accepted %zu\n", layout.local_node, layout.nb_node, id);
			} break;
			case CCI_EVENT_RECV: {
				// Check msg
				auto msg = static_cast<const NameServer::msg_uri *> (event->recv.ptr);
				ASSERT_STD (msg != nullptr);
				ASSERT_STD (event->recv.len == sizeof (*msg));
				ASSERT_STD (msg->nb_node > 0);
				ASSERT_STD (msg->node_id < msg->nb_node);
				ASSERT_STD (msg->node_id != layout.local_node);
				DEBUG_TEXT ("[givy][%zu/%zu] Received %zu\n", layout.local_node, layout.nb_node,
				            static_cast<size_t> (msg->node_id));
				// Open connection to other only if it is of superior id (prevent double connection)
				if (msg->node_id > layout.local_node) {
					check_cci (cci_connect (end_point, msg->uri, &layout.local_node,
					                        sizeof (layout.local_node),
					                        CCI_CONN_ATTR_RU, // only need reliable
					                        reinterpret_cast<const void *> (msg->node_id), 0, nullptr),
					           "cci_connect");
				DEBUG_TEXT ("[givy][%zu/%zu] Connect to %zu (%s)\n", layout.local_node, layout.nb_node,
				            static_cast<size_t> (msg->node_id), msg->uri);
				}
			} break;
			default:
				break;
			}
			cci_return_event (event);
		}
		DEBUG_TEXT ("[givy][%zu/%zu] Setup finished\n", layout.local_node, layout.nb_node);
		cci_disconnect (name_server);
	}
};
}

#endif
