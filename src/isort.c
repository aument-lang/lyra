#include "qsort.h"
#include <stdlib.h>

void lyra_ssort(size_t A[], size_t n) {
    size_t tmp;
#define LESS(i, j) A[i] < A[j]
#define SWAP(i, j) tmp = A[i], A[i] = A[j], A[j] = tmp
    QSORT(n, LESS, SWAP);
#undef LESS
#undef SWAP
}