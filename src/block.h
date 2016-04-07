#ifndef BLOCK_H
#define BLOCK_H

#ifdef __cplusplus
#include <cstddef>
using std::size_t;
#else
#include <stddef.h>
#endif

struct givy_block {
	void * ptr;
	size_t size;
};

#ifdef __cplusplus
namespace Givy {
	using Block = struct givy_block;
}
#endif

#endif
