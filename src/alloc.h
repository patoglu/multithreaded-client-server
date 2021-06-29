#ifndef alloc_h
#define alloc_h

#include <stdio.h>
#include "robust_io.h"

void*
robust_calloc(size_t count, size_t size, FILE ** fp);

void*
robust_calloc_q(size_t count, size_t size);

#endif /* alloc_h */
