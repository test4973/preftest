#include <stddef.h>   // size_t

typedef struct {
    void* buffer;
    size_t size;
} buff;

typedef struct {
    size_t cSize_max;  // must be > 16 MB
    int offset_min;
    int offset_max;
} gen_params;

gen_params init_gen_params();

buff generate(gen_params params);

void free_buff(buff buffer);
