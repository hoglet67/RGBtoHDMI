#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_4bpp_subsample_even

.macro CAPTURE_LOW_BITS_SUBSAMPLE
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 ignored
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 ignored

        and    r10, r8, #(7 << PIXEL_BASE)
        mov    r10, r10, lsl #(4 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        orr    r10, r10, r9, lsl #(6 - PIXEL_BASE)

.endm

.macro CAPTURE_HIGH_BITS_SUBSAMPLE
        // Pixel 4 in GPIO  4.. 2 -> 23..20
        // Pixel 5 ignored
        // Pixel 6 in GPIO 10.. 8 -> 31..28
        // Pixel 7 ignored

        and    r9, r8, #(7 << PIXEL_BASE)
        orr    r10, r10, r9, lsl #(20 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        orr    r10, r10, r9, lsl #(22 - PIXEL_BASE)

   .endm

// The capture line function is provided the following:
//   r0 = pointer to current line in frame buffer
//   r1 = number of 8-pixel blocks to capture (=param_chars_per_line)
//   r2 = frame buffer line pitch in bytes (=param_fb_pitch)
//   r3 = flags register
//   r4 = GPLEV0 constant
//   r5 = frame buffer height (=param_fb_height)
//   r6 = scan line count modulo 10
//
// All registers are available as scratch registers (i.e. nothing needs to be preserved)

capture_line_default_4bpp_subsample_even:

        push    {lr}
        mov     r6, #0
        mov     r7, #0
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r7, =0x11111111
loop:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8

        CAPTURE_LOW_BITS_SUBSAMPLE       // input in r8, result in r10, corrupts r9/r14

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8

        CAPTURE_HIGH_BITS_SUBSAMPLE      // input in r8, result in r10, corrupts r9/r14

        // Pixel double
        orr    r10, r10, lsr #4

        // Orr in the VSync indicator

        orr    r10, r10, r7

        // Line double always in Modes 0-6 regardless of interlace
        // On the multi core Pi this introduces stalling artefacts
#ifndef HAS_MULTICORE
        tst    r3, #BIT_SCANLINES
        streq  r10, [r0, r2]
        strne  r6, [r0, r2]
#endif
        str    r10, [r0], #4
        subs   r1, r1, #1
        bne    loop

        pop    {pc}
