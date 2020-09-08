#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_fast_simple_trailing_pos_16bpp
.global capture_line_fast_simple_leading_pos_16bpp
.global capture_line_fast_simple_trailing_neg_16bpp
.global capture_line_fast_simple_leading_neg_16bpp
.global capture_line_fast_simple_trailing_both_16bpp
.global capture_line_fast_simple_leading_both_16bpp

.macro  COMMON_SIMPLE
loop_16bpp_simple\@:
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO_0                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO_0                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO r12               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     loop_16bpp_simple\@

        mov     r0, r2
        pop     {pc}

.endm


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

        // *** 16 bit ***
        b       preload_capture_line_fast_simple_trailing_pos_16bpp
capture_line_fast_simple_trailing_pos_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11_R12
        SETUP_TWELVE_BITS_MASK_R14
        orr r3, r3, #PSYNC_MASK    // only +ve edge
        SKIP_PSYNC_SIMPLE_TRAILING_SINGLE_EDGE_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_trailing_pos_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_trailing_pos_16bpp

        .ltorg

        // *** 16 bit ***
        b       preload_capture_line_fast_simple_leading_pos_16bpp
capture_line_fast_simple_leading_pos_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11_R12
        SETUP_TWELVE_BITS_MASK_R14
        orr r3, r3, #PSYNC_MASK    // only +ve edge
        SKIP_PSYNC_SIMPLE_LEADING_SINGLE_EDGE_FAST
        COMMON_SIMPLE        
preload_capture_line_fast_simple_leading_pos_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_leading_pos_16bpp

        .ltorg

        // *** 16 bit ***
        b       preload_capture_line_fast_simple_trailing_neg_16bpp
capture_line_fast_simple_trailing_neg_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11_R12
        SETUP_TWELVE_BITS_MASK_R14
        bic r3, r3, #PSYNC_MASK    // only -ve edge
        SKIP_PSYNC_SIMPLE_TRAILING_SINGLE_EDGE_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_trailing_neg_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_trailing_neg_16bpp

        .ltorg

        // *** 16 bit ***
        b       preload_capture_line_fast_simple_leading_neg_16bpp
capture_line_fast_simple_leading_neg_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11_R12
        SETUP_TWELVE_BITS_MASK_R14
        bic r3, r3, #PSYNC_MASK    // only -ve edge
        SKIP_PSYNC_SIMPLE_LEADING_SINGLE_EDGE_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_leading_neg_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_leading_neg_16bpp

        .ltorg

        // *** 16 bit ***
        b       preload_capture_line_fast_simple_trailing_both_16bpp
capture_line_fast_simple_trailing_both_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11_R12
        SETUP_TWELVE_BITS_MASK_R14
        SKIP_PSYNC_SIMPLE_TRAILING_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_trailing_both_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_trailing_both_16bpp

        .ltorg

        // *** 16 bit ***
        b       preload_capture_line_fast_simple_leading_both_16bpp
capture_line_fast_simple_leading_both_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11_R12
        SETUP_TWELVE_BITS_MASK_R14
        SKIP_PSYNC_SIMPLE_LEADING_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_leading_both_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_leading_both_16bpp