#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_fast_sixbits_4bpp
.global capture_line_fast_sixbits_8bpp

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

        b       preload_capture_line_fast_sixbits
capture_line_fast_sixbits_4bpp:
        push    {lr}
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x11111111
        moveq   r11, #0
        tst     r3, #BIT_DEBUG
        eorne   r11, r11, #0x50         //magenta in leftmost
        eorne   r11, r11, #0x02000000   //green in rightmost
        mov     r1, r1, lsr #1
        SKIP_PSYNC_NO_H_SCROLL
        push    {r14}
loop:
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_WIDE_R11          // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_WIDE              // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_WIDE              // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_WIDE              // input in r8, result in r10, corrupts r9
        MOV     r7, r10
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_WIDE_R11          // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_WIDE              // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_WIDE              // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_WIDE              // input in r8, result in r10, corrupts r9
        stmia   r0!, {r7, r10}
        subs    r1, r1, #1
        bne     loop
        pop     {r0, pc}

preload_capture_line_fast_sixbits:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_sixbits_4bpp

        .ltorg

        // *** 8 bit ***

        b       preload_capture_line_fast_sixbits_8bpp
capture_line_fast_sixbits_8bpp:
        push    {lr}
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x01010101
        moveq   r11, #0
        movne   r12, r11
        moveq   r12, #0
        tst     r3, #BIT_DEBUG
        eorne   r11, r11, #0x05           //magenta in leftmost
        eorne   r12, r12, #0x02000000     //green in rightmost
        mov     r1, r1, lsr #1
        SKIP_PSYNC_NO_H_SCROLL
        push    {r14}        
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE_R11                // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE                   // input in r8, result in r10, corrupts r9
        mov     r5, r10
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE_R12                // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE                   // input in r8, result in r10, corrupts r9
        mov     r6, r10
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE_R11                // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE                   // input in r8, result in r10, corrupts r9
        mov     r7, r10
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE_R12                // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE                   // input in r8, result in r10, corrupts r9
        stmia   r0!, {r5, r6, r7, r10}
        subs    r1, r1, #1
        bne     loop_8bpp
        pop     {r0, pc}

preload_capture_line_fast_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_sixbits_8bpp
