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


static int benchFunction(BMK_benchFn_t fn, void* payload, int nbSec)
{
    unsigned const total_ms = nbSec * 1000;
    unsigned const run_ms = 1000;
    BMK_timedFnState_t* const benchState = BMK_createTimedFnState(total_ms, run_ms);
    assert(benchState != NULL);

    buff sample = generate();
    size_t result;
    size_t dstCapacity = decSize(sample.buffer, sample.size);
    void* dstBuffer = malloc(dstCapacity); assert(dstBuffer != NULL);
    double bestSpeed = 0.0;

    while (!BMK_isCompleted_TimedFn(benchState)) {
        BMK_runOutcome_t const outcome = BMK_benchTimedFn(benchState,
                                                    fn, payload,
                                                    NULL, NULL,
                                                    1,
                                                    (const void* const*)&sample.buffer, &sample.size,
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
    printf("Benchmarking zfdec (no prefetch) : \n");
    benchFunction(zfdec,  NULL, 4);
    for (int i=1; i<50; i++) {
        printf("Benchmarking zfpref, prefetch %i in advance \n", i);
        benchFunction(zfpref, &i, 4);
    }
    return 0;
}


int main(int argc, const char* argv[])
{
    (void)argc; (void)argv;
    return bench();
}
