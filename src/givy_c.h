#pragma once
#ifndef GIVY_C_H
#define GIVY_C_H

#include "block.h"

#ifdef __cplusplus
extern "C" {
#endif

void givy_init (int * argc, char **argv[]);

struct givy_block givy_allocate (size_t size, size_t align);
void givy_deallocate (void * ptr);

void givy_require_read_only (void * ptr);
void givy_require_read_write (void * ptr);

#ifdef __cplusplus
} // extern
#endif

#endif
