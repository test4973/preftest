/* Experimental long-range decoder
 * testing prefetching */


#include <stddef.h>   // size_t
#include <string.h>   // memcpy
#include <assert.h>
#include "zfdec.h"

#define MB       * (1 << 20)
#define PREFIX_SIZE  (16 MB)


static int isLittleEndian(void)
{
    typedef union { int i; char c; } ic;
    ic const u = { 1 };
    return u.c == 1;
}

static int MEM_readLE32(const void* p)
{
    int val;
    static_assert(sizeof(val) == 4 , "int must be a 4-bytes type");
    assert( isLittleEndian() ); (void)isLittleEndian();
    memcpy(&val, p, 4);
    return val;
}


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

size_t decSize(const void* src, size_t srcSize)
{
    assert(srcSize >= 4); (void)srcSize;
    return MEM_readLE32(src);
}

size_t decompress(void* dst, size_t dstCapacity,
            const void* src, size_t srcSize)
{
    const char* ip = src;

    size_t const dstSize = MEM_readLE32(ip); ip += 4;
    assert(dstSize <= dstCapacity); (void)dstSize;

    size_t const cSize = MEM_readLE32(ip); ip += 4;
    assert(srcSize == cSize); (void)cSize;

    int const nbSeqs = MEM_readLE32(ip); ip += 4;
    const char* seqPtr = ip;
    ip += nbSeqs * 6;

    char* const ostart = dst;
    char* op = ostart;
    char* const oend = ostart + dstCapacity;

    /* warm up data */
    memcpy(op, ip, PREFIX_SIZE);
    op += PREFIX_SIZE;
    ip += PREFIX_SIZE;

    const char* litPtr = ip;
    const char* const litEnd = (const char*)src + srcSize;

    for (int seqNb = 0 ; seqNb < nbSeqs ; seqNb++) {  // sequences
        // take commands
        int const nbLiterals = *seqPtr++;
        int const nbMatches = *seqPtr++;
        int const offset = MEM_readLE32(seqPtr); seqPtr += 4;

        // start with literals
        assert(nbLiterals <= 16);
        assert(litEnd >= litPtr);
        assert(nbLiterals <= (litEnd - litPtr));
        memcpy(op, litPtr, 16);
        op += nbLiterals;
        litPtr += nbLiterals;

        // match
        const void* match = op - offset;
        assert(offset >= 32);
        assert(nbMatches <= 32);
        assert(offset <= op - ostart);
        memcpy(op, match, 32);
        op += nbMatches;
    }

    // last literals
    assert(litPtr <= litEnd);
    size_t nbLastLiterals = (size_t)(litPtr - litEnd);
    assert((size_t)(oend - op) >= nbLastLiterals); (void)oend;
    memcpy(op, litPtr, nbLastLiterals);
    op += nbLastLiterals;

    return (size_t)(op - ostart);
}
