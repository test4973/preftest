
#include <stddef.h>

size_t decSize(const void* src, size_t srcSize);

size_t decompress(void* dst, size_t dstCapacity,
            const void* src, size_t srcSize);




size_t decompress_pref(void* dst, size_t dstCapacity,
                 const void* src, size_t srcSize,
                       int prefRounds);




typedef struct {
    size_t compressed_size;
    size_t original_size;
    size_t nb_sequences;
    size_t total_literal_lengths;
    size_t literal_length_min;
    size_t literal_length_max;
    size_t literal_leftover;
    size_t total_match_lengths;
    size_t match_length_min;
    size_t match_length_max;
    size_t offset_min;
    size_t offset_max;
} frame_stats;

frame_stats collect_stats(const void* src, size_t srcSize);
