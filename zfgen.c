/* Experimental long-range decoder
 * testing prefetching */

/* format :
 * 4-bytes : original size
 * 4-bytes : compressed size (including header)
 * 4-bytes : nb sequences
 * Sequences : 6 bytes each : 1 - 1 - 4
 *             1 : literal length, required <= 16
 *             1 : match length, required <= 32
 *             4 : offset, required to stay within output buffer
 * 16 MB : warm up data
 * Literals : remaining of compressed size
 *            note : sum of literal lengths must be >= nb literals
 *            Any literal left after last sequence is added to the block
 *
 * note : warmup data + sum of literal + sum of matches must be == original size
 */

#include <stddef.h>   // size_t
#include <stdlib.h>   // malloc, calloc
#include <assert.h>


#define MB   * (1<<20)
#define WARMUP_SIZE  (16 MB)
#define SEQ_SIZE 6



static int gen_d50_0_16(void)
{
    static int seed = 0;
    assert(seed < 2000 MB);
    seed++;
    int score = 0;
    unsigned mask = seed;
    assert(mask != 0);
    while ((mask & 1) == 0) {
        mask >>= 1;
        score++;
    }
    if (score > 16) score = 16;
    return score;
}

#define PRIME32_1    2654435761U;   /* 0b10011110001101110111100110110001 */
#define PRIME32_2    2246822519U;   /* 0b10000101111010111100101001110111 */
static int gen_d12_3_32(void)
{
    static unsigned seed = PRIME32_1;
    int score = 3;
    for (;;) {
        seed *= PRIME32_2;
        unsigned const mask = (seed >> 19) & 7;
        if (mask == 5) break;
        score++;
    }
    if (score > 32) score = 32;
    return score;
}

int randomVal(int min, int max)
{
    assert(min <= max);
    int variation = max - min;
    unsigned key = (unsigned)rand() * PRIME32_1;
    return min + (key % variation);
}


static void MEM_writeLE32(void* p, int val)
{
    *(int*)p = val;
}



typedef struct {
    void* buffer;
    size_t size;
} buff;

buff generate()
{
#define CSIZE_MAX (48 MB)
    void* const outBuff = calloc(1, CSIZE_MAX); assert(outBuff != NULL);

    char* const ostart = outBuff;
    char* op = ostart;
    int* origSizePtr = (void*)op; op+=4;
    int* cSizePtr = (void*)op; op+=4;
    int* nbSeqPtr = (void*)op; op+=4;

    int origSize = WARMUP_SIZE;
    int cSize = 4 + 4 + 4;
    int litSize = 0;

    int const nbSeqMax = 16 MB / SEQ_SIZE;
    for (int seqNb = 0; seqNb < nbSeqMax; seqNb++) {
        int ll = gen_d50_0_16();
        int ml = gen_d12_3_32();
        assert(ml <= 32);
        *op++ = (char)ll;
        *op++ = (char)ml;

        // offset
        int const offset = randomVal(12 MB, origSize);
        MEM_writeLE32(op, offset); op+=4;

        origSize += ll + ml;
        cSize += ll + SEQ_SIZE;
        litSize += ll;
    }

    // add warmup, then literals
    op += WARMUP_SIZE;
    cSize += WARMUP_SIZE;
    assert(cSize < CSIZE_MAX);
    op += litSize;

    MEM_writeLE32(origSizePtr, origSize);
    MEM_writeLE32(cSizePtr, cSize);
    MEM_writeLE32(nbSeqPtr, nbSeqMax);

    buff result = { .buffer = outBuff,
                    .size = op - (char*)outBuff
                  };
    return result;
}
