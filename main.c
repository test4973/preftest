#include <stdlib.h>   // malloc
#include <stdio.h>    // fprintf
#include <string.h>   // memcpy
#include <assert.h>

#include "bench.h"   // BMK_*
#include "zfgen.h"   // generate
#include "zfdec.h"   // decompress


/* =========================== */
/* ***   Macro functions   *** */
/* =========================== */
#define DISPLAY(...)  { fprintf(stdout, __VA_ARGS__); fflush(stdout); }

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

static size_t zfstat(const void* src, size_t srcSize, void* dst, size_t dstCapacity, void* customPayload) // type BMK_benchFn_t;
{
    (void)dst; (void)dstCapacity;
    frame_stats const stats = collect_stats(src, srcSize);
    memcpy(customPayload, &stats, sizeof(stats));
    return 0;
}


typedef struct {
    BMK_benchFn_t fn;
    void* payload;
    buff srcBuffer;
    int nbSecs;
    int nbPrefetchs;
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
        DISPLAY("\r%2i prefetchs - dec speed = %.1f MB/s    --  %i byte \r",
                params.nbPrefetchs, bestSpeed, (int)runTime.sumOfReturn);
    }
    DISPLAY("\n");

    BMK_freeTimedFnState(benchState);
    return 0;
}

static int bench_variant(int prefetch_level, buff sample, int bench_nbSeconds)
{
    assert(prefetch_level >= 0);
    if (prefetch_level == 0) {
        benchfn_params params = { .fn = zfdec,
                                  .payload = NULL,
                                  .srcBuffer = sample,
                                  .nbSecs = bench_nbSeconds,
                                  .nbPrefetchs = 0 };
        return benchFunction(params);
    }

    // prefetch_level > 0
    benchfn_params params = { .fn = zfpref,
                              .payload = &prefetch_level,
                              .srcBuffer = sample,
                              .nbSecs = bench_nbSeconds,
                              .nbPrefetchs = prefetch_level };
    benchFunction(params);

    return 0;
}

static int bench_once(int prefetch_level, int bench_nbSeconds)
{
    gen_params gparams = init_gen_params();
    buff sample = generate(gparams);

    assert(prefetch_level >= 0);
    bench_variant(prefetch_level, sample, bench_nbSeconds);

    free_buff(sample);
    return 0;
}

static int bench_all(int bench_nbSeconds)
{
    gen_params gparams = init_gen_params();
    buff sample = generate(gparams);

    for (int i = 0; i < 50; i++)
        bench_variant(i, sample, bench_nbSeconds);

    free_buff(sample);
    return 0;
}

static int visualize_stats(void)
{
    gen_params gparams = init_gen_params();
    buff sample = generate(gparams);

    frame_stats stats;
    benchfn_params params = { .fn = zfstat,
                              .payload = &stats,
                              .srcBuffer = sample,
                              .nbSecs = 1,
                              .nbPrefetchs = 0 };
    benchFunction(params);

    unsigned const nb_sequences = (unsigned)stats.nb_sequences;
    DISPLAY("nb sequences : %5u \n", nb_sequences);
    size_t const total_sequence_length = stats.original_size  - stats.literal_leftover;
    double const average_sequence_length = (double)total_sequence_length / nb_sequences;
    DISPLAY("average sequences length : %5.1f \n", average_sequence_length);
    double const average_match_length = (double)stats.total_match_lengths / nb_sequences;
    DISPLAY("average match length : %5.1f \n", average_match_length);
    double const average_literal_length = (double)stats.total_literal_lengths / nb_sequences;
    DISPLAY("average literal length : %5.1f \n", average_literal_length);
    DISPLAY("minimum offset : %6u \n", (unsigned)stats.offset_min);
    DISPLAY("maximum offset : %6u \n", (unsigned)stats.offset_max);

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
    int prefetch_level = -1;

    for (int argNb=1; argNb<argCount; argNb++) {
        const char* argument = argv[argNb];
        if (argument[0]=='-') {
            argument++;
            while (argument[0] != 0) {
                switch(argument[0]) {

                /* Control bench time */
                case 'i':
                    argument++;
                    bench_nbSeconds = readU32FromChar(&argument);
                    break;

                /* Control prefetch level (single variant) */
                case 'b':
                    argument++;
                    prefetch_level = readU32FromChar(&argument);
                    break;

                default : errorOut("bad command line \n");

                }
            }  //while (argument[0] != 0)

        }
    }  // for (int argNb=1; argNb<argCount; argNb++)

    if (prefetch_level == 999)
        return visualize_stats();

    if (prefetch_level >= 0)
        return bench_once(prefetch_level, bench_nbSeconds);

    return bench_all(bench_nbSeconds);
}
