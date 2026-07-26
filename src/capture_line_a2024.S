#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_a2024_8bpp
.extern a2024_convert_line
.extern a2024_decode_control
.extern a2024_capture_result
.extern a2024_h_offset_adjust
.extern a2024_hires
.extern a2024_line_buffer
.extern capture_line_default_twelvebits_16bpp

/*
 * Preserve all twelve Amiga colour bits in the sampling-critical loop, then
 * expand R3/G3/B3/B0 into two grayscale lines during horizontal blanking.
 */
        .align 6
        b       preload_capture_line_a2024_8bpp
capture_line_a2024_8bpp:
        tst     r3, #BIT_NO_SKIP_HSYNC
        beq     capture_line_a2024_common
        ldr     r12, =a2024_h_offset_adjust
        ldr     r12, [r12]
        add     r7, r7, r12
        ldr     r12, =a2024_hires
        ldr     r12, [r12]
        cmp     r12, #0
        moveq   r1, #704
        movne   r1, #512
capture_line_a2024_common:
        /*
         * The active-line dispatcher enters with sp four bytes off an
         * eight-byte boundary.  Save an odd number of words so the C
         * converter sees the AAPCS-required alignment.
         */
        push    {r0, r1, r2, r3, r5, r12, lr}
        ldr     r0, =a2024_line_buffer
        orr     r3, r3, #BIT_NO_LINE_DOUBLE
        bic     r3, r3, #BITDUP_RGB_INVERT | BITDUP_Y_INVERT
        bl      capture_line_default_twelvebits_16bpp
        str     r0, [sp, #4]
        ldr     r0, [sp]
        ldr     r1, [sp, #8]
        ldr     r2, [sp, #12]
        tst     r2, #BIT_NO_SKIP_HSYNC
        moveq   r1, #16
        moveq   r2, #8
        movne   r2, #0
        str     r3, [sp, #8]
        bl      a2024_convert_line

        /*
         * A new phase or DPL value is learned only after the last real
         * data line.  The current field went to scratch because the fast
         * decoder rejected it; the next field can then use the cached LUT
         * without variable work in the command-to-data gap.
         */
        ldr     r2, [sp, #12]
        tst     r2, #BIT_NO_SKIP_HSYNC
        beq     a2024_deferred_done
        ldr     r2, [sp, #16]
        cmp     r2, #1
        bne     a2024_deferred_done
        ldr     r2, =a2024_capture_result
        ldr     r2, [r2]
        cmp     r2, #0
        blt     a2024_deferred_decode
        b       a2024_deferred_done
a2024_deferred_decode:
        bl      a2024_decode_control
a2024_deferred_done:
        ldr     r0, [sp, #4]
        ldr     r3, [sp, #8]
        ldr     r2, [sp, #12]
        and     r2, r2, #BITDUP_RGB_INVERT | BITDUP_Y_INVERT
        bic     r3, r3, #BITDUP_RGB_INVERT | BITDUP_Y_INVERT
        orr     r3, r3, r2
        add     sp, sp, #24
        pop     {pc}

preload_capture_line_a2024_8bpp:
        SETUP_DUMMY_PARAMETERS
        /*
         * Preload enters aligned, unlike the active dispatcher.  Add one
         * word so the common wrapper still enters four bytes off.
         */
        push    {lr}
        bl      capture_line_a2024_common
        pop     {pc}
