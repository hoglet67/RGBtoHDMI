#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_atom_8bpp

.macro CAPTURE_BITS
        // Pixel 0 in GPIO  5.. 2 -> 15.. 8 and  7.. 0
        // Pixel 1 in GPIO  9.. 6 -> 31..24 and 23..16
        and    r10, r8, #(15 << PIXEL_BASE)
        and    r9, r8, #(15 << (PIXEL_BASE + 4))
        mov    r10, r10, lsr #(PIXEL_BASE)
        orr    r10, r10, r9, lsl #(16 - (PIXEL_BASE + 4))
        orr    r10, r10, r10, lsl #8
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

        mov     pc, lr                   // entry point for preloading cache
capture_line_atom_8bpp:
        push    {lr}
        SKIP_PSYNC
        push    {r14}
        lsl     r1, #1
        mov     r6, #0
        ldr     r7, =0x01010101
loop:

        WAIT_FOR_PSYNC_EDGE

        CAPTURE_BITS

        // Mov in the VSync indicator
        tst    r3, #BIT_VSYNC_MARKER
        movne  r10, r7

        str    r10, [r0]
        tst    r3,  #BIT_SCANLINES
        movne  r10, #0
        tst    r3,  #BIT_NO_LINE_DOUBLE
        streq  r10, [r0, r2]
        add    r0, r0, #4

        subs   r1, r1, #1
        bne    loop

exit:
        pop     {r0, pc}
