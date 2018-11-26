#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_4bpp

capture_line_default_4bpp:

        push    {lr}

loop:

        WAIT_FOR_PSYNC_10

        CAPTURE_LOW_BITS

        WAIT_FOR_PSYNC_01

        CAPTURE_HIGH_BITS

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

        pop    {lr}

        mov    pc, lr
