#ifndef NAME_SERVER_H
#define NAME_SERVER_H

#include <cci.h>
#include <cstdint>
#include <cstring>

namespace Givy {
namespace NameServer {
	struct msg_uri {
		uint32_t nb_node;
		uint32_t node_id;
		char uri[100];

		msg_uri (size_t nb_node_, size_t node_id_, const char * uri_)
		    : nb_node (static_cast<uint32_t> (nb_node_)), node_id (static_cast<uint32_t> (node_id_)) {
			ASSERT_STD (uri_ != nullptr);
			ASSERT_STD (strlen (uri_) < sizeof (uri));
			strncpy (uri, uri_, sizeof (uri));
		}
	};
}
}

#endif
