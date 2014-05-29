/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include <stdlib.h>
#include <math.h>

#define ORCM_PWRVIRUS_HEAP_SIZE 1024
static float *pwrvirus_heap = NULL;

int main(int argc, char *argv[])
{
    int i;

    /* grab and init a chunk of memory */
    pwrvirus_heap = (float*)malloc(ORCM_PWRVIRUS_HEAP_SIZE * sizeof(float));
    for (i=0; i < ORCM_PWRVIRUS_HEAP_SIZE; i++) {
        pwrvirus_heap[i] = 3.1459 * (float)i;
    }

    /* burn the cpu by computing and shoving things in/out of memory */
    while (1) {
        for (i=1; i < ORCM_PWRVIRUS_HEAP_SIZE; i++) {
            pwrvirus_heap[i-1] = pwrvirus_heap[i] * pwrvirus_heap[i-1] / 1.2345;
        }
        for (i=0; i < ORCM_PWRVIRUS_HEAP_SIZE-1; i++) {
            pwrvirus_heap[i] = (pwrvirus_heap[i+1] / pwrvirus_heap[i]) * 1.2345;
        }
    }

    return 0;
}
