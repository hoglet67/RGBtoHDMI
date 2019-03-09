#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_half_4bpp
.global capture_line_half_8bpp


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

        b       preload_capture_line_half_4bpp
capture_line_half_4bpp:
        push    {lr}
        cmp     r1, #400/8               //sanity check on buffer size as only capturing half of pixels so width >400 will never finish
        movgt   r1, r1, lsr#1
        cmp     r5, #DUPLICATE_HEIGHT
        orrle   r3, r3, #BITDUP_NO_LINE_DOUBLE
        bicgt   r3, r3, #BITDUP_NO_LINE_DOUBLE
        SKIP_PSYNC
capture_half_4bpp:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r9/r10
        mov     r11, r10                 // save first word
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r9/r10

        tst    r3, #BIT_EVEN_SAMPLES
        andne  r9, r11, #0x00000007
        movne  r12, r9, lsl#4
        andne  r9, r11, #0x00000700
        orrne  r12, r12, r9, lsr #8
        andne  r9, r11, #0x00070000
        orrne  r12, r12, r9, lsr #4
        andne  r9, r11, #0x07000000
        orrne  r12, r12, r9, lsr #16

        andne  r9, r10, #0x00000007
        orrne  r12, r12, r9, lsl#20
        andne  r9, r10, #0x00000700
        orrne  r12, r12, r9, lsl #8
        andne  r9, r10, #0x00070000
        orrne  r12, r12, r9, lsl #12
        andne  r9, r10, #0x07000000
        orrne  r10, r12, r9

        tst    r3, #BIT_ODD_SAMPLES
        andne  r9, r11, #0x00000070
        movne  r12, r9
        andne  r9, r11, #0x00007000
        orrne  r12, r12, r9, lsr #12
        andne  r9, r11, #0x00700000
        orrne  r12, r12, r9, lsr #8
        andne  r9, r11, #0x70000000
        orrne  r12, r12, r9, lsr #20

        andne  r9, r10, #0x00000070
        orrne  r12, r12, r9, lsl#16
        andne  r9, r10, #0x00007000
        orrne  r12, r12, r9, lsl #4
        andne  r9, r10, #0x00700000
        orrne  r12, r12, r9, lsl #8
        andne  r9, r10, #0x70000000
        orrne  r10, r12, r9, lsr #4

        WRITE_WORD

        subs    r1, r1, #1
        bne     capture_half_4bpp
        pop     {pc}

preload_capture_line_half_4bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_half_4bpp

        .ltorg

        // *** 8 bit ***


        b       preload_capture_line_half_8bpp
capture_line_half_8bpp:
        push    {lr}
        cmp     r1, #400/8               //sanity check on buffer size as only capturing half of pixels so width >400 will never finish
        movgt   r1, r1, lsr#1
        cmp     r5, #DUPLICATE_HEIGHT
        orrle   r3, r3, #BITDUP_NO_LINE_DOUBLE
        bicgt   r3, r3, #BITDUP_NO_LINE_DOUBLE
        bic     r3, #MASKDUP_PALETTE_HIGH_NIBBLE
        SKIP_PSYNC
capture_half_8bpp:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r10, corrupts r9
        mov     r11, r10                 // save first word
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r9/r10

        tst    r3, #BIT_EVEN_SAMPLES
        andne  r9, r11, #0x00000700
        movne  r12, r9, lsr#8
        andne  r9, r11, #0x07000000
        orrne  r12, r12, r9, lsr #16
        andne  r9, r10, #0x00000700
        orrne  r12, r12, r9, lsl#8
        andne  r9, r10, #0x07000000
        orrne  r7, r12, r9

        tst    r3, #BIT_ODD_SAMPLES
        andne  r9, r11, #0x00000007
        movne  r12, r9
        andne  r9, r11, #0x00070000
        orrne  r12, r12, r9, lsr #8
        andne  r9, r10, #0x00000007
        orrne  r12, r12, r9, lsl#16
        andne  r9, r10, #0x00070000
        orrne  r7, r12, r9, lsl #8

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r10, corrupts r9
        mov     r11, r10                 // save first word
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r9/r10

        tst    r3, #BIT_EVEN_SAMPLES
        andne  r9, r11, #0x00000700
        movne  r12, r9, lsr#8
        andne  r9, r11, #0x07000000
        orrne  r12, r12, r9, lsr #16
        andne  r9, r10, #0x00000700
        orrne  r12, r12, r9, lsl#8
        andne  r9, r10, #0x07000000
        orrne  r10, r12, r9

        tst    r3, #BIT_ODD_SAMPLES
        andne  r9, r11, #0x00000007
        movne  r12, r9
        andne  r9, r11, #0x00070000
        orrne  r12, r12, r9, lsr #8
        andne  r9, r10, #0x00000007
        orrne  r12, r12, r9, lsl#16
        andne  r9, r10, #0x00070000
        orrne  r10, r12, r9, lsl #8

        mov    r9, r7
        WRITE_WORDS_8BPP
        subs    r1, r1, #1
        bne     capture_half_8bpp
        pop     {pc}

preload_capture_line_half_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_half_8bpp








