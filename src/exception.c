#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

void lyra_fatal_index(const void *array, size_t idx, size_t len) {
    fprintf(stderr,
            "trying to access array %p (with len %" PRIdMAX
            ") at idx %" PRIdMAX "\n",
            array, len, idx);
    abort();
}
