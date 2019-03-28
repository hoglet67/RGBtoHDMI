#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_sixbits_4bpp
.global capture_line_default_sixbits_8bpp

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

        b       preload_capture_line_default_sixbits
capture_line_default_sixbits_4bpp:
        push    {lr}
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x11111111
        moveq   r11, #0
        tst     r3, #BIT_DEBUG
        eorne   r11, r11, #0x50         //magenta in leftmost
        eorne   r11, r11, #0x02000000   //green in rightmost
        SKIP_PSYNC_NO_OLD_CPLD
        push    {r14}
loop:
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_WIDE r11          // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_WIDE              // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_WIDE              // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_WIDE r7           // input in r8, result in r7, corrupts r9

        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_WIDE r11          // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_WIDE              // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_WIDE              // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_WIDE r10          // input in r8, result in r10, corrupts r9

        stmia   r0, {r7, r10}
        tst     r3, #BIT_SCANLINES
        movne   r7, #0
        movne   r10, #0
        tst     r3, #BIT_NO_LINE_DOUBLE
        addeq   r0, r0, r2
        stmeqia r0, {r7, r10}
        subeq   r0, r0, r2
        add     r0, r0, #8
        
        sub     r1, r1, #2
        cmp     r1, #1
        bgt     loop
        popne   {r0, pc}
        
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_WIDE r11          // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_WIDE              // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_WIDE              // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_WIDE r7           // input in r8, result in r7, corrupts r9
        
        stmia   r0, {r7}
        tst     r3, #BIT_SCANLINES
        movne   r7, #0
        tst     r3, #BIT_NO_LINE_DOUBLE
        addeq   r0, r0, r2
        stmeqia r0, {r7}

        pop     {r0, pc}

preload_capture_line_default_sixbits:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_sixbits_4bpp

        .ltorg

        // *** 8 bit ***

        b       preload_capture_line_default_sixbits_8bpp
capture_line_default_sixbits_8bpp:
        push    {lr}
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x01010101
        moveq   r11, #0
        movne   r12, r11
        moveq   r12, #0
        tst     r3, #BIT_DEBUG
        eorne   r11, r11, #0x05           //magenta in leftmost
        eorne   r12, r12, #0x02000000     //green in rightmost
        SKIP_PSYNC_NO_OLD_CPLD
        push    {r14}
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r5                // input in r8, result in r5, corrupts r9

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r12                // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r6                // input in r8, result in r6, corrupts r9

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r7                // input in r8, result in r7, corrupts r9

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r12                // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r10               // input in r8, result in r10, corrupts r9

        stmia   r0, {r5, r6, r7, r10}
        tst     r3, #BIT_SCANLINES
        movne   r5, #0
        movne   r6, #0
        movne   r7, #0
        movne   r10, #0
        tst     r3, #BIT_NO_LINE_DOUBLE
        addeq   r0, r0, r2
        stmeqia r0, {r5, r6, r7, r10}
        subeq   r0, r0, r2
        add     r0, r0, #16
        
        sub     r1, r1, #2
        cmp     r1, #1
        bgt     loop_8bpp
        popne   {r0, pc}
        
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r5                // input in r8, result in r5, corrupts r9

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r12                // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r6                // input in r8, result in r6, corrupts r9
        
        stmia   r0, {r5, r6}
        tst     r3, #BIT_SCANLINES
        movne   r5, #0
        movne   r6, #0
        tst     r3, #BIT_NO_LINE_DOUBLE
        addeq   r0, r0, r2
        stmeqia r0, {r5, r6}        
        pop    {r0, pc}      
 

preload_capture_line_default_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_sixbits_8bpp
