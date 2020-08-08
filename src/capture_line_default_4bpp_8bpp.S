#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_4bpp
.global capture_line_default_8bpp

// The capture line function is provided the following:
//   r0 = pointer to current line in frame buffer
//   r1 = number of complete psync cycles to capture (=param_chars_per_line)
//   r2 = frame buffer line pitch in bytes (=param_fb_pitch)
//   r3 = flags register
//   r4 = GPLEV0 constant
//   r5 = line number count down to 0 (initial value =param_nlines)
//   r6 = scan line count modulo 10
//   r7 = number of psyncs to skip
//   r8 = frame buffer height (=param_fb_height)
//
// All registers are available as scratch registers (i.e. nothing needs to be preserved)

        b       preload_capture_line_default
capture_line_default_4bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11
        SKIP_PSYNC
loop:
        WAIT_FOR_PSYNC_EDGE                   // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_NORMAL r11           // input in r8
        WAIT_FOR_PSYNC_EDGE                   // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_NORMAL r7           // input in r8

        WRITE_R7_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE                   // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_NORMAL r11           // input in r8
        WAIT_FOR_PSYNC_EDGE                   // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_NORMAL r10          // input in r8

        WRITE_R7_R10

        subs    r1, r1, #2
        bne     loop

        pop     {r0, pc}

preload_capture_line_default:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_4bpp

        .ltorg

        // *** 8 bit ***


        b       preload_capture_line_default_8bpp
capture_line_default_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP_NORMAL r11 r5  // input in r8
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP_NORMAL r12 r6  // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP_NORMAL r11 r7  // input in r8
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP_NORMAL r12 r10 // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp

        pop     {r0, pc}


preload_capture_line_default_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_8bpp
