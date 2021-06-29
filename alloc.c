
#include "alloc.h"
#include <stdlib.h>

void*
robust_calloc(size_t count, size_t size, FILE **fp)
{
    void* p;
    if ((p = calloc(count, size)) == NULL)
    {
    	fprintf(*fp, "Calloc encountered an error. Exiting");
        //robust_exit("Calloc encountered an error. Exiting.\n", EXIT_FAILURE);
    }
    return p;
}


void*
robust_calloc_q(size_t count, size_t size)
{
    void* p;
    if ((p = calloc(count, size)) == NULL)
    {
    	//fprintf(*fp, "Calloc encountered an error. Exiting");
        robust_exit("Calloc encountered an error. Exiting.\n", EXIT_FAILURE);
    }
    return p;
}
