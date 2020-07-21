#pragma once
#ifndef GIVY_INSTANCE_H
#define GIVY_INSTANCE_H

#include "gas_space.h"
#include "network.h"

namespace Givy {
struct GasStructs {
	union {
		Network network;
	};
	union {
		Gas::Space space;
	};
	bool inited{false};
	GasStructs () = default; // Just set to not initialized
	void init (int & argc, char **& argv);
	~GasStructs (); // Cleanup
};

extern GasStructs gas;
}

#endif
