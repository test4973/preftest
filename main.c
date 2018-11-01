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

static int bench(void)
{
    gen_params gparams = init_gen_params();
    buff sample = generate(gparams);

    printf("Benchmarking zfdec (no prefetch) : \n");
    {   benchfn_params params = { .fn = zfdec,
                                  .payload = NULL,
                                  .srcBuffer = sample,
                                  .nbSecs = 4 };
        benchFunction(params);
    }

    for (int i=1; i<50; i++) {
        printf("Benchmarking zfpref, prefetch %i in advance \n", i);
        benchfn_params params = { .fn = zfpref,
                                  .payload = &i,
                                  .srcBuffer = sample,
                                  .nbSecs = 4 };
        benchFunction(params);
    }

    free_buff(sample);
    return 0;
}


int main(int argc, const char* argv[])
{
    (void)argc; (void)argv;
    return bench();
}
