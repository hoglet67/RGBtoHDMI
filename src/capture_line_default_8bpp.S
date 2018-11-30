#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_8bpp

.macro CAPTURE_BITS
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24

        and    r10, r8, #(7 << PIXEL_BASE)
        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        mov    r10, r10, lsr #(PIXEL_BASE)
        orr    r10, r10, r9, lsl #(8 - (PIXEL_BASE + 3))

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r8, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #(16 - (PIXEL_BASE + 6))
        orr    r10, r10, r8, lsl #(24 - (PIXEL_BASE + 9))
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

capture_line_default_8bpp:

        push    {lr}

        mov     r6, #0
        mov     r7, #0
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r7, =0x01010101
loop:
        WAIT_FOR_PSYNC_10

        CAPTURE_BITS

        // Orr in the VSync indicator
        orr     r10, r10, r7

        // Line double always in Modes 0-6 regardless of interlace
        // On the multi core Pi this introduces stalling artefacts
#ifndef HAS_MULTICORE
        tst    r3, #BIT_SCANLINES
        streq  r10, [r0, r2]
        strne  r6, [r0, r2]
#endif
        str    r10, [r0], #4

        WAIT_FOR_PSYNC_01

        CAPTURE_BITS

        // Orr in the VSync indicator
        orr    r10, r10, r7

        // Line double always in Modes 0-6 regardless of interlace
        // On the multi core Pi this introduces stalling artefacts
#ifndef HAS_MULTICORE
        tst    r3, #BIT_SCANLINES
        streq  r10, [r0, r2]
        strne  r6, [r0, r2]
#endif
        subs   r1, r1, #1
        str    r10, [r0], #4
        bne    loop

exit:
        pop    {pc}
