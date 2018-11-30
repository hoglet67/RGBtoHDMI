#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_4bpp

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

capture_line_default_4bpp:

        push    {lr}
        mov     r6, #0
        mov     r7, #0
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r7, =0x11111111
loop:
        WAIT_FOR_PSYNC_10                // expects GPLEV0 in r4, result in r8

        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14

        WAIT_FOR_PSYNC_01                // expects GPLEV0 in r4, result in r8

        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14

        // Orr in the VSync indicator
        
        orr  r10, r10, r7

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
