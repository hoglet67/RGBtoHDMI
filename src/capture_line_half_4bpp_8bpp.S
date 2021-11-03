#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_half_even_4bpp
.global capture_line_half_even_8bpp
.global capture_line_half_odd_4bpp
.global capture_line_half_odd_8bpp


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
        .align 6
        b       preload_capture_line_half_even_4bpp
capture_line_half_even_4bpp:
        push    {lr}
        cmp     r1, #400/8               //sanity check on buffer size as only capturing half of pixels so width >400 will never finish
        movgt   r1, r1, lsr#1
        SKIP_PSYNC
        mov    r1, r1, lsr #1
capture_half_4bppe:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r9/r10
        mov     r11, r10                 // save first word
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r9/r10


        and    r9, r11, #0x00000007
        mov    r12, r9, lsl#4
        and    r9, r11, #0x00000700
        orr    r12, r12, r9, lsr #8
        and    r9, r11, #0x00070000
        orr    r12, r12, r9, lsr #4
        and    r9, r11, #0x07000000
        orr    r12, r12, r9, lsr #16

        and    r9, r10, #0x00000007
        orr    r12, r12, r9, lsl#20
        and    r9, r10, #0x00000700
        orr    r12, r12, r9, lsl #8
        and    r9, r10, #0x00070000
        orr    r12, r12, r9, lsl #12
        and    r9, r10, #0x07000000
        orr    r10, r12, r9


        WRITE_WORD

        subs    r1, r1, #1
        bne     capture_half_4bppe
        pop     {r0, pc}

preload_capture_line_half_even_4bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_half_even_4bpp

        .ltorg

        // *** 8 bit ***

        .align 6
        b       preload_capture_line_half_even_8bpp
capture_line_half_even_8bpp:
        push    {lr}
        cmp     r1, #400/8               //sanity check on buffer size as only capturing half of pixels so width >400 will never finish
        movgt   r1, r1, lsr#1
        bic     r3, #MASKDUP_PALETTE_HIGH_NIBBLE
        SKIP_PSYNC
        mov    r1, r1, lsr #1
capture_half_8bppe:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r10, corrupts r9
        mov     r11, r10                 // save first word
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r9/r10


        and    r9, r11, #0x00000700
        mov    r12, r9, lsr#8
        and    r9, r11, #0x07000000
        orr    r12, r12, r9, lsr #16
        and    r9, r10, #0x00000700
        orr    r12, r12, r9, lsl#8
        and    r9, r10, #0x07000000
        orr    r7, r12, r9



        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r10, corrupts r9
        mov     r11, r10                 // save first word
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r9/r10

        and    r9, r11, #0x00000700
        mov    r12, r9, lsr#8
        and    r9, r11, #0x07000000
        orr    r12, r12, r9, lsr #16
        and    r9, r10, #0x00000700
        orr    r12, r12, r9, lsl#8
        and    r9, r10, #0x07000000
        orr    r10, r12, r9


        mov    r9, r7
        WRITE_WORDS_8BPP
        subs    r1, r1, #1
        bne     capture_half_8bppe
        pop     {r0, pc}

preload_capture_line_half_even_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_half_even_8bpp


        .align 6
        b       preload_capture_line_half_odd_4bpp
capture_line_half_odd_4bpp:
        push    {lr}
        cmp     r1, #400/8               //sanity check on buffer size as only capturing half of pixels so width >400 will never finish
        movgt   r1, r1, lsr#1
        SKIP_PSYNC
        mov    r1, r1, lsr #1
capture_half_4bppo:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r9/r10
        mov     r11, r10                 // save first word
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r9/r10


        and    r9, r11, #0x00000070
        mov    r12, r9
        and    r9, r11, #0x00007000
        orr    r12, r12, r9, lsr #12
        and    r9, r11, #0x00700000
        orr    r12, r12, r9, lsr #8
        and    r9, r11, #0x70000000
        orr    r12, r12, r9, lsr #20

        and    r9, r10, #0x00000070
        orr    r12, r12, r9, lsl#16
        and    r9, r10, #0x00007000
        orr    r12, r12, r9, lsl #4
        and    r9, r10, #0x00700000
        orr    r12, r12, r9, lsl #8
        and    r9, r10, #0x70000000
        orr    r10, r12, r9, lsr #4

        WRITE_WORD

        subs    r1, r1, #1
        bne     capture_half_4bppo
        pop     {r0, pc}

preload_capture_line_half_odd_4bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_half_odd_4bpp

        .ltorg

        // *** 8 bit ***

        .align 6
        b       preload_capture_line_half_odd_8bpp
capture_line_half_odd_8bpp:
        push    {lr}
        cmp     r1, #400/8               //sanity check on buffer size as only capturing half of pixels so width >400 will never finish
        movgt   r1, r1, lsr#1
        bic     r3, #MASKDUP_PALETTE_HIGH_NIBBLE
        SKIP_PSYNC
        mov    r1, r1, lsr #1
capture_half_8bppo:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r10, corrupts r9
        mov     r11, r10                 // save first word
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r9/r10

        and    r9, r11, #0x00000007
        mov    r12, r9
        and    r9, r11, #0x00070000
        orr    r12, r12, r9, lsr #8
        and    r9, r10, #0x00000007
        orr    r12, r12, r9, lsl#16
        and    r9, r10, #0x00070000
        orr    r7, r12, r9, lsl #8

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r10, corrupts r9
        mov     r11, r10                 // save first word
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r9/r10

        and    r9, r11, #0x00000007
        mov    r12, r9
        and    r9, r11, #0x00070000
        orr    r12, r12, r9, lsr #8
        and    r9, r10, #0x00000007
        orr    r12, r12, r9, lsl#16
        and    r9, r10, #0x00070000
        orr    r10, r12, r9, lsl #8

        mov    r9, r7
        WRITE_WORDS_8BPP
        subs    r1, r1, #1
        bne     capture_half_8bppo
        pop     {r0, pc}

preload_capture_line_half_odd_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_half_odd_8bpp



