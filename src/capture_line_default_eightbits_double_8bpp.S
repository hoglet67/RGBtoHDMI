#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_eightbits_double_8bpp

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

        .ltorg

        // *** 8 bit ***

        b       preload_capture_line_default_eightbits_double_8bpp
capture_line_default_eightbits_double_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        SKIP_PSYNC_NO_OLD_CPLD
        push    {r14}
        SETUP_MASK_R14
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_DOUBLE_8BPP_LO r11     // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_DOUBLE_8BPP_HI r5      // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_DOUBLE_8BPP_LO r12     // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_DOUBLE_8BPP_HI r6      // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_DOUBLE_8BPP_LO r11     // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_DOUBLE_8BPP_HI r7      // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_DOUBLE_8BPP_LO r12     // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_DOUBLE_8BPP_HI r10     // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp

        pop     {r0, pc}


preload_capture_line_default_eightbits_double_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_eightbits_double_8bpp
