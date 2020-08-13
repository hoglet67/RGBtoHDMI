#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_fast_sixbits_8bpp
.global capture_line_fast_sixbits_16bpp

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


// 4bpp not currently used but left in in case

        b       preload_capture_line_fast_sixbits
capture_line_fast_sixbits_4bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11
        SKIP_PSYNC_NO_OLD_CPLD_FAST
loop:
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_WIDE r11          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_WIDE r7           // input in r8

        cmp     r1, #1
        stmeqia r0, {r7}
        moveq   r0, r2
        popeq   {pc}

        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_WIDE r11          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_WIDE r10          // input in r8

        stmia   r0!, {r7, r10}
        subs    r1, r1, #2
        bne     loop

        mov     r0, r2
        pop     {pc}

preload_capture_line_fast_sixbits:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_sixbits_4bpp

        .ltorg

        // *** 8 bit ***

        b       preload_capture_line_fast_sixbits_8bpp
capture_line_fast_sixbits_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD_FAST
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r6                // input in r8

        cmp     r1, #1
        stmeqia r0, {r5, r6}
        moveq   r0, r2
        popeq   {pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r10               // input in r8

        stmia   r0!, {r5, r6, r7, r10}
        subs    r1, r1, #2
        bne     loop_8bpp

        mov     r0, r2
        pop     {pc}

preload_capture_line_fast_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_sixbits_8bpp


        .ltorg

        // *** 16 bit ***

        b       preload_capture_line_fast_sixbits_16bpp
capture_line_fast_sixbits_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD_FAST
        SETUP_TWELVE_BITS_MASK_R14
loop_16bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO_0                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        cmp     r1, #1
        stmeqia r0, {r5, r6}
        moveq   r0, r2
        popeq   {pc}

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO_0                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO r12               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r10               // input in r8

        stmia   r0!, {r5, r6, r7, r10}
        subs    r1, r1, #2
        bne     loop_16bpp

        mov     r0, r2
        pop     {pc}

preload_capture_line_fast_sixbits_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_sixbits_16bpp
