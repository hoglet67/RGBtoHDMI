/*
 * cocovga_bench.h - time the CoCoVGA per-frame work (cv_frame) on the
 * machine it runs on, without a Dragon connected.
 *
 * The benchmark generates MC6847 frames the way the MC6847digitizer and
 * RGBtoHDMI put them in the frame buffer (gen6847.c), runs them through
 * the same controller the capture loop calls once per frame, and times
 * only that call. On the Pi the frames live in non-cacheable memory, the
 * same memory type as the real frame buffers.
 *
 * Plain C, no RGBtoHDMI dependencies (also built by cocovga_tools/).
 */
#ifndef COCOVGA_BENCH_H
#define COCOVGA_BENCH_H

#include <stdint.h>

#define CV_BENCH_MAX_ROWS  20

typedef struct {
   const char *name;
   int group;            // 0 = lower case off, 1 = lower case on
   int n;                // timed frames
   unsigned int min_us, avg_us, max_us;
   int ok;               // every timed frame took the expected path
   int frames;           // "W64 enter": frames until the screen was shown
} cv_bench_row_t;

typedef struct {
   int rows;
   cv_bench_row_t row[CV_BENCH_MAX_ROWS];
   int lowercase_available;   // 0 in builds without 32 column lower case
} cv_bench_result_t;

// microsecond clock
typedef unsigned int (*cv_usec_fn)(void);

// Bytes of frame buffer memory the benchmark needs (4 buffers of 640x480)
#define CV_BENCH_MEM_SIZE  (4 * 640 * 480)

// mem: CV_BENCH_MEM_SIZE bytes, ideally the same memory type as the real
// frame buffers. budget_us / decode_budget_us: the per-frame work limits the
// firmware uses (0 = none), so the times are what each frame really costs.
// "W64 enter" rows time every frame until the 64 column screen is shown.
// Takes a few seconds on a Pi Zero.
void cv_bench_run(uint8_t *mem, cv_usec_fn usec, unsigned int budget_us, unsigned int decode_budget_us,
                  cv_bench_result_t *res);

#endif
