#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_4bpp
.global capture_line_default_8bpp

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


        b       preload_capture_line_default
capture_line_default_4bpp:
        push    {lr}
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r6, =0x11111111
        moveq   r6, #0
        tst     r3, #BIT_DEBUG
        eorne   r6, r6, #0x50         //magenta in leftmost
        eorne   r6, r6, #0x02000000   //green in rightmost
        SKIP_PSYNC
loop:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9
        WRITE_WORD_FAST
        subs    r1, r1, #1
        bne     loop
        pop     {pc}

preload_capture_line_default:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_4bpp

        .ltorg

        // *** 8 bit ***


        b       preload_capture_line_default_8bpp
capture_line_default_8bpp:
        push    {lr}
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r5, =0x01010101
        moveq   r5, #0
        ldrne   r6, =0x01010101
        moveq   r6, #0
        tst     r3, #BIT_DEBUG
        eorne   r5, r5, #0x05           //magenta in leftmost
        eorne   r6, r6, #0x02000000     //green in rightmost
        SKIP_PSYNC
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r10, corrupts r9
        mov     r11, r10                 // save first word
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r9/r10
        mov     r9, r11
        WRITE_WORDS_8BPP_FAST
        subs    r1, r1, #1
        bne     loop_8bpp
        pop     {pc}

preload_capture_line_default_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_8bpp
