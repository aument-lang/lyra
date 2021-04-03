#pragma once

#include <stdlib.h>

#define BSEARCH_THRESHOLD 32

#define BSEARCH(T, name)                                                  \
    static inline T name(T *a, size_t n, T x) {                           \
        size_t i = 0, j = n - 1;                                          \
        if (n < BSEARCH_THRESHOLD) {                                      \
            for (i = 0; i < n; i++)                                       \
                if (a[i] == x)                                            \
                    return i;                                             \
            return ((T)-1);                                               \
        }                                                                 \
        while (i <= j) {                                                  \
            int k = i + ((j - i) / 2);                                    \
            if (a[k] == x) {                                              \
                return k;                                                 \
            } else if (a[k] < x) {                                        \
                i = k + 1;                                                \
            } else {                                                      \
                j = k - 1;                                                \
            }                                                             \
        }                                                                 \
        return ((T)-1);                                                   \
    }

BSEARCH(size_t, lyra_sbsearch)
