#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_atom_4bpp

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

        mov     pc, lr                   // entry point for preloading cache
capture_line_atom_4bpp:
        push    {lr}
        SKIP_PSYNC
        push    {r14}
        mov     r6, #0
        mov     r7, #0
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r7, =0x11111111

loop:

        WAIT_FOR_PSYNC_EDGE                   // expects GPLEV0 in r4, result in r8

        // Pixel 0 in GPIO  5..2 ->  3..0
        // Pixel 1 in GPIO  9..6 -> 11..8

        and    r10, r8, #(0x0F << PIXEL_BASE)
        and     r9, r8, #(0xF0 << PIXEL_BASE)
        eor     r8, r8, #(0x88 << PIXEL_BASE) // flip bit 3 of each color
                                              // this makes the extended colour tests easier to code

        tst     r8,     #(0x08 << PIXEL_BASE) // Extended color, so default to black
        biceq  r10,     #(0x0F << PIXEL_BASE)
        tst     r8,     #(0x0E << PIXEL_BASE) // but change orange => yellow
        orreq  r10,     #(0x03 << PIXEL_BASE)

        tst     r8,     #(0x80 << PIXEL_BASE)
        biceq   r9,     #(0xF0 << PIXEL_BASE)
        tst     r8,     #(0xE0 << PIXEL_BASE)
        orreq   r9,     #(0x30 << PIXEL_BASE)

        mov    r10, r10, lsr #(PIXEL_BASE)
        orr    r10, r10, r9, lsl #(8 - (4 + PIXEL_BASE))

        WAIT_FOR_PSYNC_EDGE                   // expects GPLEV0 in r4, result in r8

        // Pixel 0 in GPIO  5..2 -> 19..16
        // Pixel 1 in GPIO  9..6 -> 27..24

        and    r12, r8, #(0x0F << PIXEL_BASE)
        and     r9, r8, #(0xF0 << PIXEL_BASE)
        eor     r8, r8, #(0x88 << PIXEL_BASE) // flip bit 3 of each color
                                              // this makes the extended colour tests easier to code

        tst     r8,     #(0x08 << PIXEL_BASE) // Extended color, so default to black
        biceq  r12,     #(0x0F << PIXEL_BASE)
        tst     r8,     #(0x0E << PIXEL_BASE) // but change orange => yellow
        orreq  r12,     #(0x03 << PIXEL_BASE)

        tst     r8,     #(0x80 << PIXEL_BASE)
        biceq   r9,     #(0xF0 << PIXEL_BASE)
        tst     r8,     #(0xE0 << PIXEL_BASE)
        orreq   r9,     #(0x30 << PIXEL_BASE)

        orr    r10, r10, r12, lsl #(16 - PIXEL_BASE)
        orr    r10, r10,  r9, lsl #(24 - (PIXEL_BASE + 4))

        // Now pixel double
        orr    r10, r10, r10, lsl #4

        // Orr in the VSync indicator
        orrne  r10, r10, r7

        str    r10, [r0]
        tst    r3,  #BIT_SCANLINES
        movne  r10, #0
        tst    r3,  #BIT_NO_LINE_DOUBLE
        streq  r10, [r0, r2]
        add    r0, r0, #4

        subs   r1, r1, #1
        bne    loop

        pop     {r0, pc}
