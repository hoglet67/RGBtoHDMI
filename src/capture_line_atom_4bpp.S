#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_atom_4bpp

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

capture_line_atom_4bpp:

        push    {lr}

        mov    r12, r0                   // pointer to the line in the frame buffer
        mov    r6, r1                    // number of 8-bit characters per line

loop:

        WAIT_FOR_PSYNC_10                // expects GPLEV0 in r4, result in r8

        // Pixel 0 in GPIO  4..2 ->  2..0
        // Pixel 1 in GPIO  8..6 -> 10..8

        and    r10, r8, #(7 << PIXEL_BASE)
        and    r9, r8, #(7 << (PIXEL_BASE + 4))
        mov    r10, r10, lsr #(PIXEL_BASE)
        orr    r10, r10, r9, lsl #(8 - (4 + PIXEL_BASE))

        WAIT_FOR_PSYNC_01                // expects GPLEV0 in r4, result in r8

        // Pixel 0 in GPIO  4..2 -> 18..16
        // Pixel 1 in GPIO  8..6 -> 26..24

        and    r9, r8, #(7 << PIXEL_BASE)
        and    r14, r8, #(7 << (PIXEL_BASE + 4))
        orr    r10, r10, r9, lsl #(16 - PIXEL_BASE)
        orr    r10, r10, r14, lsl #(24 - (PIXEL_BASE + 4))

        // Now pixel double
        orr    r10, r10, r10, lsl #4

        // Orr in the VSync indicator
        ldr    r8, =0x11111111
        tst    r3, #BIT_VSYNC_MARKER
        orrne  r10, r10, r8

        // Line double always in Modes 0-6 regardless of interlace
        // On the multi core Pi this introduces stalling artefacts
#ifndef HAS_MULTICORE
        tst    r3, #BIT_SCANLINES
        movne  r0, #0
        moveq  r0, r10
        str    r0, [r12, r2]
#endif
        str    r10, [r12], #4
        subs   r6, r6, #1
        bne    loop

        pop    {pc}
