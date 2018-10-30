#include <stddef.h>   // size_t

typedef struct {
    void* buffer;
    size_t size;
} buff;

buff generate(void);
