#include <stdlib.h>   // malloc
#include <stdio.h>    // fprintf
#include <assert.h>

#include "bench.h"   // BMK_*
#include "zfgen.h"   // generate
#include "zfdec.h"   // decompress


#define DISPLAY(...)  fprintf(stdout, __VA_ARGS__)

static size_t zfdec(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* customPayload) // type BMK_benchFn_t;
{
    (void)customPayload;
    return decompress(dst, dstCapacity, src, srcSize);
}

static size_t zfpref(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* customPayload) // type BMK_benchFn_t;
{
    int const nbRounds = *(int*)customPayload;
    return decompress_pref(dst, dstCapacity, src, srcSize, nbRounds);
}


typedef struct {
    BMK_benchFn_t fn;
    void* payload;
    buff srcBuffer;
    int nbSecs;
} benchfn_params;

static int benchFunction(benchfn_params params)
{
    unsigned const total_ms = params.nbSecs * 1000;
    unsigned const run_ms = 1000;
    BMK_timedFnState_t* const benchState = BMK_createTimedFnState(total_ms, run_ms);
    assert(benchState != NULL);

    size_t result;
    size_t dstCapacity = decSize(params.srcBuffer.buffer, params.srcBuffer.size);
    void* dstBuffer = malloc(dstCapacity); assert(dstBuffer != NULL);
    double bestSpeed = 0.0;

    while (!BMK_isCompleted_TimedFn(benchState)) {
        BMK_runOutcome_t const outcome = BMK_benchTimedFn(benchState,
                                                    params.fn, params.payload,
                                                    NULL, NULL,
                                                    1,
                                                    (const void* const*)&params.srcBuffer.buffer, &params.srcBuffer.size,
                                                    &dstBuffer, &dstCapacity,
                                                    &result);
        BMK_runTime_t const runTime = BMK_extract_runTime(outcome);
        //DISPLAY("nanosec per run : %llu \n", runTime.nanoSecPerRun);
        //DISPLAY("decompressed size : %zu \n", runTime.sumOfReturn);

        double const bytePerNs = (double)runTime.sumOfReturn / runTime.nanoSecPerRun;
        double const bytePerSec = bytePerNs * 1000000000;
        double const MBperSec = bytePerSec / 1000000;
        if (MBperSec > bestSpeed) bestSpeed = MBperSec;
        DISPLAY("\rdecompression speed : %.1f MB/s    --  %i byte \r", bestSpeed, (int)runTime.sumOfReturn);
    }
    DISPLAY("\n");

    BMK_freeTimedFnState(benchState);
    return 0;
}

static int bench(int bench_nbSeconds)
{
    gen_params gparams = init_gen_params();
    buff sample = generate(gparams);

    printf("Benchmarking zfdec (no prefetch) : \n");
    {   benchfn_params params = { .fn = zfdec,
                                  .payload = NULL,
                                  .srcBuffer = sample,
                                  .nbSecs = bench_nbSeconds };
        benchFunction(params);
    }

    for (int i=1; i<50; i++) {
        printf("Benchmarking zfpref, prefetch %i in advance \n", i);
        benchfn_params params = { .fn = zfpref,
                                  .payload = &i,
                                  .srcBuffer = sample,
                                  .nbSecs = bench_nbSeconds };
        benchFunction(params);
    }

    free_buff(sample);
    return 0;
}


static void errorOut(const char* msg)
{
    fprintf(stderr, "%s \n", msg); exit(1);
}


/*! readU32FromChar() :
 * @return : unsigned integer value read from input in `char` format.
 *  allows and interprets K, KB, KiB, M, MB and MiB suffix.
 *  Will also modify `*stringPtr`, advancing it to position where it stopped reading.
 *  Note : function will exit() program if digit sequence overflows */
static unsigned readU32FromChar(const char** stringPtr)
{
    const char errorMsg[] = "error: numeric value too large";
    unsigned result = 0;
    while ((**stringPtr >='0') && (**stringPtr <='9')) {
        unsigned const max = (((unsigned)(-1)) / 10) - 1;
        if (result > max) errorOut(errorMsg);
        result *= 10, result += **stringPtr - '0', (*stringPtr)++ ;
    }
    if ((**stringPtr=='K') || (**stringPtr=='M')) {
        unsigned const maxK = ((unsigned)(-1)) >> 10;
        if (result > maxK) errorOut(errorMsg);
        result <<= 10;
        if (**stringPtr=='M') {
            if (result > maxK) errorOut(errorMsg);
            result <<= 10;
        }
        (*stringPtr)++;  /* skip `K` or `M` */
        if (**stringPtr=='i') (*stringPtr)++;
        if (**stringPtr=='B') (*stringPtr)++;
    }
    return result;
}

int main(int argCount, const char* argv[])
{
    unsigned bench_nbSeconds = 4;
    for (int argNb=1; argNb<argCount; argNb++) {
        const char* argument = argv[argNb];
        if (argument[0]=='-') {
            argument++;
            while (argument[0] != 0) {
                switch(argument[0]) {
                /* Modify Nb Iterations (benchmark only) */
                case 'i':
                    argument++;
                    bench_nbSeconds = readU32FromChar(&argument);
                    break;
                }
            }

        }
    }
    return bench(bench_nbSeconds);
}
