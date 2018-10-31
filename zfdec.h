
#include <stddef.h>

size_t decSize(const void* src, size_t srcSize);

size_t decompress(void* dst, size_t dstCapacity,
            const void* src, size_t srcSize);




size_t decompress_pref(void* dst, size_t dstCapacity,
                 const void* src, size_t srcSize,
                       int prefRounds);
