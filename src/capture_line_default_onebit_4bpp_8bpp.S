#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_onebit_4bpp
.global capture_line_default_onebit_8bpp
.global capture_line_default_onebit_double_4bpp
.global capture_line_default_onebit_double_8bpp
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


.macro CAPTURE_BITS_ONEBIT reg reg2
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 in GPIO  7.. 5 ->  3.. 0
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 in GPIO 13..11 -> 11.. 8
        mov    \reg2, \reg
        tst    r8, #(0x01 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000070
        tst    r8, #(0x04 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000007
        tst    r8, #(0x08 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00007000
        tst    r8, #(0x20 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000700
        tst    r8, #(0x40 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00700000
        tst    r8, #(0x100 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00070000
        tst    r8, #(0x200 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x70000000
        tst    r8, #(0x800 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x07000000
.endm

.macro CAPTURE_BITS_ONEBIT_8BPP_LO reg reg2
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 in GPIO  7.. 5 ->  3.. 0
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 in GPIO 13..11 -> 11.. 8
        mov    \reg2, \reg
        tst    r8, #(0x01 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000007
        tst    r8, #(0x04 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000700
        tst    r8, #(0x08 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00070000
        tst    r8, #(0x20 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x07000000
.endm

.macro CAPTURE_BITS_ONEBIT_8BPP_HI reg reg2
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 in GPIO  7.. 5 ->  3.. 0
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 in GPIO 13..11 -> 11.. 8
        mov    \reg2, \reg
        tst    r8, #(0x40 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000007
        tst    r8, #(0x100 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000700
        tst    r8, #(0x200 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00070000
        tst    r8, #(0x800 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x07000000
.endm

.macro CAPTURE_BITS_ONEBIT_DOUBLE_LO reg reg2
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 in GPIO  7.. 5 ->  3.. 0
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 in GPIO 13..11 -> 11.. 8
        mov    \reg2, \reg
        tst    r8, #(0x01 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000077
        tst    r8, #(0x04 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00007700
        tst    r8, #(0x08 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00770000
        tst    r8, #(0x20 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x77000000
.endm

.macro CAPTURE_BITS_ONEBIT_DOUBLE_HI reg reg2
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 in GPIO  7.. 5 ->  3.. 0
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 in GPIO 13..11 -> 11.. 8
        mov    \reg2, \reg
        tst    r8, #(0x40 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000077
        tst    r8, #(0x100 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00007700
        tst    r8, #(0x200 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00770000
        tst    r8, #(0x800 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x77000000
.endm


.macro CAPTURE_BITS_ONEBIT_DOUBLE_8BPP_0 reg reg2
        mov    \reg2, \reg
        tst    r8, #(0x01 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000007
        eorne  \reg2, \reg2, #0x00000700
        tst    r8, #(0x04 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00070000
        eorne  \reg2, \reg2, #0x07000000
.endm

.macro CAPTURE_BITS_ONEBIT_DOUBLE_8BPP_1 reg reg2
        mov    \reg2, \reg
        tst    r8, #(0x08 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000007
        eorne  \reg2, \reg2, #0x00000700
        tst    r8, #(0x20 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00070000
        eorne  \reg2, \reg2, #0x07000000
.endm

.macro CAPTURE_BITS_ONEBIT_DOUBLE_8BPP_2 reg reg2
        mov    \reg2, \reg
        tst    r8, #(0x40 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000007
        eorne  \reg2, \reg2, #0x00000700
        tst    r8, #(0x100 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00070000
        eorne  \reg2, \reg2, #0x07000000
.endm

.macro CAPTURE_BITS_ONEBIT_DOUBLE_8BPP_3 reg reg2
        mov    \reg2, \reg
        tst    r8, #(0x200 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00000007
        eorne  \reg2, \reg2, #0x00000700
        tst    r8, #(0x800 << PIXEL_BASE)
        eorne  \reg2, \reg2, #0x00070000
        eorne  \reg2, \reg2, #0x07000000
.endm








        .align 6
        b       preload_capture_line_default_onebit_4bpp
capture_line_default_onebit_4bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11
        SKIP_PSYNC_NO_OLD_CPLD
loop:
        WAIT_FOR_PSYNC_EDGE                  // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_ONEBIT r11 r7         // input in r8
        WRITE_R7_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}
        WAIT_FOR_PSYNC_EDGE                  // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_ONEBIT r11 r10            // input in r8
        WRITE_R7_R10

        subs    r1, r1, #2
        bne     loop

        pop     {r0, pc}

preload_capture_line_default_onebit_4bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_onebit_4bpp

        .ltorg
        .align 6
        b       preload_capture_line_default_onebit_double_4bpp
capture_line_default_onebit_double_4bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11
        SKIP_PSYNC_NO_OLD_CPLD
loopd:
        WAIT_FOR_PSYNC_EDGE                  // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_ONEBIT_DOUBLE_LO r11 r7         // input in r8
        WRITE_R7_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}
        CAPTURE_BITS_ONEBIT_DOUBLE_HI r11 r10            // input in r8
        WRITE_R7_R10

        subs    r1, r1, #2
        bne     loopd

        pop     {r0, pc}

preload_capture_line_default_onebit_double_4bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_onebit_double_4bpp

        .ltorg
        // *** 8 bit ***

        .align 6
        b       preload_capture_line_default_onebit_8bpp
capture_line_default_onebit_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE                 // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_ONEBIT_8BPP_LO r11 r5  // input in r8
        CAPTURE_BITS_ONEBIT_8BPP_HI r12 r6  // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}
        WAIT_FOR_PSYNC_EDGE                 // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_ONEBIT_8BPP_LO r11 r7  // input in r8
        CAPTURE_BITS_ONEBIT_8BPP_HI r12 r10 // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp

        pop     {r0, pc}
preload_capture_line_default_onebit_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_onebit_8bpp


        .ltorg
        // *** 8 bit ***

        .align 6
        b       preload_capture_line_default_onebit_double_8bpp
capture_line_default_onebit_double_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC
loop_8bppd:
        WAIT_FOR_PSYNC_EDGE                 // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_ONEBIT_DOUBLE_8BPP_0 r11 r5  // input in r8
        CAPTURE_BITS_ONEBIT_DOUBLE_8BPP_1 r12 r6  // input in r8
        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}        
        
        CAPTURE_BITS_ONEBIT_DOUBLE_8BPP_2 r11 r7  // input in r8
        CAPTURE_BITS_ONEBIT_DOUBLE_8BPP_3 r12 r10  // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bppd

        pop     {r0, pc}

preload_capture_line_default_onebit_double_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_onebit_double_8bpp