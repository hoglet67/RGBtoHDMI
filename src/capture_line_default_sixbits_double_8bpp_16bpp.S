#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_sixbits_double_8bpp
.global capture_line_default_odd_even_sixbits_double_8bpp
.global capture_line_default_sixbits_double_16bpp

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
        .align 6
        b       preload_capture_line_default_sixbits_double
capture_line_default_sixbits_double_4bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_DOUBLE
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #1
loop:
        WAIT_FOR_PSYNC_EDGE_FAST           // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_WIDE r11   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST           // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_WIDE r7   // input in r8

        WRITE_R7_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST           // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_WIDE r11   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST           // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_WIDE r10  // input in r8

        WRITE_R7_R10

        subs    r1, r1, #2
        bne     loop

        pop     {r0, pc}


preload_capture_line_default_sixbits_double:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_sixbits_double_4bpp

        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_default_sixbits_double_8bpp
capture_line_default_sixbits_double_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #1
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_WIDE r11 r5          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_WIDE r12 r6          // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_WIDE r11 r7          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_WIDE r12 r10         // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp

        pop     {r0, pc}


preload_capture_line_default_sixbits_double_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_sixbits_double_8bpp
        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_default_odd_even_sixbits_double_8bpp
capture_line_default_odd_even_sixbits_double_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #1
loop_oe8bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_ODD_EVEN_8BPP_WIDE r11 r5          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_ODD_EVEN_8BPP_WIDE r12 r6          // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_ODD_EVEN_8BPP_WIDE r11 r7          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_ODD_EVEN_8BPP_WIDE r12 r10         // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_oe8bpp

        pop     {r0, pc}


preload_capture_line_default_odd_even_sixbits_double_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_odd_even_sixbits_double_8bpp
        .ltorg

        // *** 16 bit ***
        .align 6
        b       preload_capture_line_default_sixbits_double_16bpp
capture_line_default_sixbits_double_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #1
        ldr    r14, =palette_data_16
loop_16bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_LO r11 r5                 // input in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_HI r11 r6                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_LO r11 r7                 // input in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_HI r11 r10                 // input in r8

        WRITE_R5_R6_R7_R10_16BPP

        subs    r1, r1, #1
        bne     loop_16bpp

        pop     {r0, pc}

preload_capture_line_default_sixbits_double_16bpp:
        ldr    r0, =palette_data_16
        mov    r1, #64
preload_loop:
        ldr    r2, [r0], #4
        subs   r1, r1, #1
        bne    preload_loop
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_sixbits_double_16bpp
