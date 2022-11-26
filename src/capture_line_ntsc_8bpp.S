#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

#ifdef USE_ARM_CAPTURE
.macro  SKIP_PSYNC_NO_OLD_CPLD_NTSC
        SKIP_PSYNC_COMMON_NO_OLD_CPLD
        mov    r9, #0
skip_psync_loop_no_oldL\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        ands   r10, r8, #(0x10 << PIXEL_BASE)
        orr    r9, r9, r10
        and    r10, r8, #(0x10 << (PIXEL_BASE + 6))
        orr    r9, r9, r10
        subs   r7, r7, #1
        bne    skip_psync_loop_no_oldL\@
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x01010101
        moveq   r11, #0
        // exit with r9 non-zero if burst detected, r11=red linem
.endm
.macro  SKIP_PSYNC_NO_OLD_CPLD_NTSC_3BPP
        SKIP_PSYNC_NO_OLD_CPLD_NTSC
.endm

#else

.macro  SKIP_PSYNC_NO_OLD_CPLD_NTSC
        SKIP_PSYNC_COMMON_NO_OLD_CPLD
        add    r8, r7, r1
        add    r8, r8, #4
        str    r8, [r4, #(GPU_COMMAND_offset - GPU_DATA_0_offset)]  //command register
        mov    r9, #0
skip_psync_loop_no_oldL6\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        ands   r10, r8, #(0x10 << PIXEL_BASE)
        orr    r9, r9, r10
        and    r10, r8, #(0x10 << (PIXEL_BASE + 6))
        orr    r9, r9, r10
        subs   r7, r7, #1
        bne    skip_psync_loop_no_oldL6\@
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x01010101
        moveq   r11, #0
        // exit with r9 non-zero if burst detected, r11=red linem
.endm

.macro  SKIP_PSYNC_NO_OLD_CPLD_NTSC_3BPP
        SKIP_PSYNC_COMMON_NO_OLD_CPLD
        add    r8, r7, r1
        add    r8, r8, #2
        str    r8, [r4, #(GPU_COMMAND_offset - GPU_DATA_0_offset)]  //command register
        mov    r9, #0
skip_psync_loop_no_oldL3\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        ands   r10, r8, #(0x10 << PIXEL_BASE)
        orr    r9, r9, r10
        and    r10, r8, #(0x10 << (PIXEL_BASE + 6))
        orr    r9, r9, r10
        subs   r7, r7, #1
        bne    skip_psync_loop_no_oldL3\@
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x01010101
        moveq   r11, #0
        // exit with r9 non-zero if burst detected, r11=red linem
.endm
#endif

.macro NTSC_CAPTURE_BITS_8BPP_MONO
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24

        mov    r11, r11, lsr #4

        tst    r12, #NTSC_Y_INVERT
        eorne  r8, r8, #(0x12 << PIXEL_BASE)
        eorne  r8, r8, #(0x12 << (PIXEL_BASE + 6))

        and    r9, r8, #(7 << PIXEL_BASE)
        and    r14, r8, #(7 << (PIXEL_BASE + 3))

        cmp    r9, #(0x07 << PIXEL_BASE)
        cmpne  r9, #(0x02 << PIXEL_BASE)
        orreq  r11, r11, #0x10000000

        cmp    r14, #(0x07 << (PIXEL_BASE + 3))
        cmpne  r14, #(0x02 << (PIXEL_BASE + 3))
        orreq  r11, r11, #0x20000000

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r14, r8, #(7 << (PIXEL_BASE + 9))

        cmp    r9, #(0x07 << (PIXEL_BASE + 6))
        cmpne  r9, #(0x02 << (PIXEL_BASE + 6))
        orreq  r11, r11, #0x40000000

        cmp    r14, #(0x07 << (PIXEL_BASE + 9))
        cmpne  r14, #(0x02 << (PIXEL_BASE + 9))
        orreq  r11, r11, #0x80000000
.endm

.macro  NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        mov    r11, r11, lsr #2

        tst    r12, #NTSC_Y_INVERT
        eorne  r8, r8, #(0x12 << PIXEL_BASE)
        eorne  r8, r8, #(0x12 << (PIXEL_BASE + 6))

        and    r9, r8, #(0x07 << PIXEL_BASE)
        and    r14, r8, #(0x07 << (PIXEL_BASE + 6))

        cmp    r9, #(0x07 << PIXEL_BASE)
        cmpne  r9, #(0x02 << PIXEL_BASE)
        orreq  r11, r11, #0x40000000

        cmp    r14, #(0x07 << (PIXEL_BASE + 6))
        cmpne  r14, #(0x02 << (PIXEL_BASE + 6))
        orreq  r11, r11, #0x80000000
.endm


.macro NTSC_CAPTURE_LOW_BITS_8BPP_WIDE reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        tst    r12, #NTSC_Y_INVERT
        eorne  r8, r8, #(0x12 << PIXEL_BASE)
        eorne  r8, r8, #(0x12 << (PIXEL_BASE + 6))

        and    r9, r8, #(0x3f << PIXEL_BASE)
        and    r14, r8, #(0x3f << (PIXEL_BASE + 6))
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    r10, r10, r14, lsl #(8 - (PIXEL_BASE + 6))
.endm

.macro NTSC_CAPTURE_HIGH_BITS_8BPP_WIDE reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        tst    r12, #NTSC_Y_INVERT
        eorne  r8, r8, #(0x12 << PIXEL_BASE)
        eorne  r8, r8, #(0x12 << (PIXEL_BASE + 6))

        and    r9, r8, #(0x3f << PIXEL_BASE)
        and    r14, r8, #(0x3f << (PIXEL_BASE + 6))
        eor    r10, r10, r9, lsl #(16 - PIXEL_BASE)
        eor    \reg, r10, r14, lsl #(24 - (PIXEL_BASE + 6))
.endm


.macro NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        mov    r11, r11, lsr #4

        tst    r12, #NTSC_Y_INVERT
        eorne  r8, r8, #(0x12 << PIXEL_BASE)
        eorne  r8, r8, #(0x12 << (PIXEL_BASE + 6))

        tst    r12, #NTSC_DONE_FIRST
        andeq  r9, r8, #(0x2d << PIXEL_BASE)
        cmpeq  r9, #(0x20 << PIXEL_BASE)       //cyan
        orreq  r8, r8, #(0x08 << PIXEL_BASE)   //makes it mono

        tst    r12, #NTSC_DONE_FIRST
        andeq  r9, r8, #(0x2d << (PIXEL_BASE + 6))
        cmpeq  r9, #(0x20 << (PIXEL_BASE + 6))       //cyan
        orreq  r8, r8, #(0x08 << (PIXEL_BASE + 6))   //makes it mono

        eor    r9, r8, r8, lsr #3
        and    r9, r9, r9, lsr #2
        tst    r9, #(0x01 << PIXEL_BASE)
        tstne  r9, #(0x01 << (PIXEL_BASE + 6))
        orreq  r3, r3, #BITDUP_LINE_CONDITION_DETECTED       //detect if any U/V content
        tst    r12, #NTSC_DONE_FIRST
        biceq  r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        orreq  r12, r12, #NTSC_DONE_FIRST

        and    r9, r8, #(0x12 << PIXEL_BASE)
        and    r14, r8, #(0x12 << (PIXEL_BASE + 6))

        cmp    r9, #(0x00 << PIXEL_BASE)
        orrne  r11, r11, #0x30000000
        cmp    r14, #(0x00 << (PIXEL_BASE + 6))
        orrne  r11, r11, #0xc0000000

.endm

.macro NTSC_CAPTURE_BITS_DOUBLE_8BPP_WIDE reg reg2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r12, #NTSC_Y_INVERT
        eorne  r8, r8, #(0x12 << PIXEL_BASE)
        eorne  r8, r8, #(0x12 << (PIXEL_BASE + 6))

        tst    r12, #NTSC_DONE_FIRST
        andeq  r9, r8, #(0x2d << PIXEL_BASE)
        cmpeq  r9, #(0x20 << PIXEL_BASE)       //cyan
        orreq  r8, r8, #(0x08 << PIXEL_BASE)   //makes it mono

        tst    r12, #NTSC_DONE_FIRST
        andeq  r9, r8, #(0x2d << (PIXEL_BASE + 6))
        cmpeq  r9, #(0x20 << (PIXEL_BASE + 6))       //cyan
        orreq  r8, r8, #(0x08 << (PIXEL_BASE + 6))   //makes it mono

        eor    r9, r8, r8, lsr #3
        and    r9, r9, r9, lsr #2
        tst    r9, #(0x01 << PIXEL_BASE)
        tstne  r9, #(0x01 << (PIXEL_BASE + 6))
        orreq  r3, r3, #BITDUP_LINE_CONDITION_DETECTED       //detect if any U/V content
        tst    r12, #NTSC_DONE_FIRST
        biceq  r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        orreq  r12, r12, #NTSC_DONE_FIRST

        and    r9, r8, #(0x3f << PIXEL_BASE)
        and    r14, r8, #(0x3f << (PIXEL_BASE + 6))
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    r10, r10, r14, lsl #(16 - (PIXEL_BASE + 6))
        // Pixel double
        orr    \reg2, r10, r10, lsl #8
.endm

.macro NTSC_CAPTURE_BITS_8BPP_NORMAL reg reg2
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24

        tst    r12, #NTSC_Y_INVERT
        eorne  r8, r8, #(0x12 << PIXEL_BASE)
        eorne  r8, r8, #(0x12 << (PIXEL_BASE + 6))

        and    r9, r8, #(7 << PIXEL_BASE)
        and    r14, r8, #(7 << (PIXEL_BASE + 3))
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    r10, r10, r14, lsl #(8 - (PIXEL_BASE + 3))

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r14, r8, #(7 << (PIXEL_BASE + 9))
        eor    r10, r10, r9, lsl #(16 - (PIXEL_BASE + 6))
        eor    \reg2, r10, r14, lsl #(24 - (PIXEL_BASE + 9))
.endm

.macro  PRELOAD_BITCOUNT
        adrl   r8, bit_count
        mov    r9, #127
preload_loop\@:
        ldr    r10, [r8, r9, lsl #2]
        subs   r9, r9, #1
        bpl    preload_loop\@
.endm

.macro  NTSC_DECODE reg
        //enter with top 4 bits representing next 4 mono pixels in r11 but decode earlier pixels to the right of those
        mov    r14, #0x00ff
        orr    r14, r14, #0xff00
        and    r9, r11, #0x00e00000
        and    r8, r11, #0x01000000
        mov    \reg, r9, lsl #4
        orr    \reg, \reg, r8
        and    r9, r11, #0x00f00000
        orr    \reg, \reg, r9, lsr #4
        and    r9, r11, #0x00700000
        and    r8, r11, #0x00080000
        orr    r9, r8, lsl #4
        orr    \reg, \reg, r9, lsr #12
        and    r9, r11, #0x00300000
        and    r8, r11, #0x000c0000
        orr    r9, r8, lsl #4
        orr    \reg, \reg, r9, lsr #20

        adrl   r8, bit_count
        mov    r9, r11, lsr #18
        and    r9, r9, #0x7f
        ldr    r8, [r8, r9, lsl #2]
        orr    \reg, \reg, r8

        eor    r9, r11, r14, lsl #16
        eor    r9, r9, r14

        eor    r8, r11, r11, lsr #1

        tst    r12, #NTSC_SOFT
        mvnne  r9, #0

// test white pixel pairs
        tst    r9, #0x03c00000
        tstne  r9, #0x00f00000
        tsteq  r8, #0x05000000
        tsteq  r8, #0x00140000
        biceq  \reg, \reg, r14, lsl #16
        orreq  \reg, \reg, #0x6f000000
        orreq  \reg, \reg, #0x006f0000

        tst    r9, #0x003c0000
        tstne  r9, #0x00f00000
        tsteq  r8, #0x01400000
        tsteq  r8, #0x00050000
        biceq  \reg, \reg, r14
        orreq  \reg, \reg, #0x00006f00
        orreq  \reg, \reg, #0x0000006f

// test white pixel pairs shifted by 45 degrees

        mov    r9, r9, ror #31
        mov    r8, r8, ror #31

        tst    r9, #0x03c00000
        tstne  r9, #0x00f00000
        tsteq  r8, #0x05000000
        tsteq  r8, #0x00140000
        biceq  \reg, \reg, r14, lsl #16
        orreq  \reg, \reg, #0x6f000000
        orreq  \reg, \reg, #0x006f0000

        tst    r9, #0x003c0000
        tstne  r9, #0x00f00000
        tsteq  r8, #0x01400000
        tsteq  r8, #0x00050000
        biceq  \reg, \reg, r14
        orreq  \reg, \reg, #0x00006f00
        orreq  \reg, \reg, #0x0000006f

.endm

.macro  NTSC_DECODE_FINAL reg
        tst    r12, #NTSC_MEDIUM
        mvnne  r9, #0

// test black pixels adjacent to white pixel pairs shifted by 45 degrees
        eor    r8, r9, #0x00c00000
        tst    r8, #0x0fc00000
        tstne  r8, #0x00fc0000
        biceq  \reg, \reg, r14, lsl #16

        eor    r8, r9, #0x00300000
        tst    r8, #0x03f00000
        tstne  r8, #0x003f0000
        biceq  \reg, \reg, r14

        mov    r9, r9, ror #1

// test black pixels adjacent to white pixel pairs
        eor    r8, r9, #0x00c00000
        tst    r8, #0x0fc00000
        tstne  r8, #0x00fc0000
        biceq  \reg, \reg, r14, lsl #16

        eor    r8, r9, #0x00300000
        tst    r8, #0x03f00000
        tstne  r8, #0x003f0000
        biceq  \reg, \reg, r14
.endm


.macro  NTSC_DECODE_CGA reg
        tst    r11, #0x00000030
        beq    white\@

        and    r9, r11, #0x00e00000
        and    r8, r11, #0x01000000
        mov    \reg, r8
        orr    \reg, \reg, r9, lsl #4
        and    r9, r11, #0x00f00000
        orr    \reg, \reg, r9, lsr #4
        and    r9, r11, #0x00700000
        and    r8, r11, #0x00080000
        orr    r9, r8, lsl #4
        orr    \reg, \reg, r9, lsr #12
        and    r9, r11, #0x00300000
        and    r8, r11, #0x000c0000
        orr    r9, r8, lsl #4
        orr    \reg, \reg, r9, lsr #20

        and    r8, r11, #0x00000030

        cmp    r8, #0x00000040  //cmp    r8, #0x00000030
        orreq  \reg, \reg, #0x30000000
        orreq  \reg, \reg, #0x00300000
        orreq  \reg, \reg, #0x00003000
        orreq  \reg, \reg, #0x00000030

        cmp    r8, #0x00000040 //cmp    r8, #0x00000030
        orreq  \reg, \reg, #0x10000000
        orreq  \reg, \reg, #0x00100000
        orreq  \reg, \reg, #0x00001000
        orreq  \reg, \reg, #0x00000010
        b      done_non_white\@

white\@:
        NTSC_DECODE \reg
done_non_white\@:
.endm

.macro  FULL_NTSC_DECODE_CGA reg
        tst    r11, #0x00000030
        beq    full_white\@

        and    r9, r11, #0x00e00000
        and    r8, r11, #0x01000000
        mov    \reg, r8
        orr    \reg, \reg, r9, lsl #4
        and    r9, r11, #0x00f00000
        orr    \reg, \reg, r9, lsr #4
        and    r9, r11, #0x00700000
        and    r8, r11, #0x00080000
        orr    r9, r8, lsl #4
        orr    \reg, \reg, r9, lsr #12
        and    r9, r11, #0x00300000
        and    r8, r11, #0x000c0000
        orr    r9, r8, lsl #4
        orr    \reg, \reg, r9, lsr #20

        and    r8, r11, #0x00000030

        cmp    r8, #0x00000040  //cmp    r8, #0x00000030
        orreq  \reg, \reg, #0x30000000
        orreq  \reg, \reg, #0x00300000
        orreq  \reg, \reg, #0x00003000
        orreq  \reg, \reg, #0x00000030

        cmp    r8, #0x00000040 //cmp    r8, #0x00000030
        orreq  \reg, \reg, #0x10000000
        orreq  \reg, \reg, #0x00100000
        orreq  \reg, \reg, #0x00001000
        orreq  \reg, \reg, #0x00000010
        b      full_done_non_white\@

full_white\@:
        NTSC_DECODE \reg
        NTSC_DECODE_FINAL \reg
full_done_non_white\@:
.endm


.macro NTSC_CAPTURE_BITS_8BPP
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24

        mov    r11, r11, lsr #4
        bic    r11, #0x00003000

        and    r9, r8, #(0x07 << PIXEL_BASE)

        cmp    r9, #(0x01 << PIXEL_BASE) //red?
        cmpne  r9, #(0x03 << PIXEL_BASE) //yellow?
        orreq  r11, r11, #0x10000000
        cmpne  r9, #(0x02 << PIXEL_BASE) //green?
        orreq  r11, #0x00001000         //palette 0

        cmp    r9, #(0x05 << PIXEL_BASE) //magenta?
        orreq  r11, r11, #0x10000000
        cmpne  r9, #(0x06 << PIXEL_BASE) //cyan?
        orreq  r11, #0x00002000          //palette 1

        cmp    r9, #(0x07 << PIXEL_BASE) //white?
        orreq  r11, r11, #0x10000000


        and    r9, r8, #(0x07 << (PIXEL_BASE + 3))

        cmp    r9, #(0x02 << (PIXEL_BASE + 3)) //green?
        cmpne  r9, #(0x03 << (PIXEL_BASE + 3)) //yellow?
        orreq  r11, r11, #0x20000000
        cmpne  r9, #(0x01 << (PIXEL_BASE + 3)) //red?
        orreq  r11, #0x00001000                //palette 0

        cmp    r9, #(0x06 << (PIXEL_BASE + 3)) //cyan?
        orreq  r11, r11, #0x20000000
        cmpne  r9, #(0x05 << (PIXEL_BASE + 3)) //magenta?
        orreq  r11, #0x00002000                //palette 1

        cmp    r9, #(0x07 << (PIXEL_BASE + 3)) //white?
        orreq  r11, r11, #0x20000000


        and    r9, r8, #(0x07 << (PIXEL_BASE + 6))

        cmp    r9, #(0x01 << (PIXEL_BASE + 6)) //red?
        cmpne  r9, #(0x03 << (PIXEL_BASE + 6)) //yellow?
        orreq  r11, r11, #0x40000000
        cmpne  r9, #(0x02 << (PIXEL_BASE + 6)) //green?
        orreq  r11, #0x00001000                //palette 0

        cmp    r9, #(0x05 << (PIXEL_BASE + 6)) //magenta?
        orreq  r11, r11, #0x40000000
        cmpne  r9, #(0x06 << (PIXEL_BASE + 6)) //cyan?
        orreq  r11, #0x00002000                //palette 1

        cmp    r9, #(0x07 << (PIXEL_BASE + 6)) //white?
        orreq  r11, r11, #0x40000000


        and    r9, r8, #(0x07 << (PIXEL_BASE + 9))

        cmp    r9, #(0x02 << (PIXEL_BASE + 9)) //green?
        cmpne  r9, #(0x03 << (PIXEL_BASE + 9)) //yellow?
        orreq  r11, r11, #0x80000000
        cmpne  r9, #(0x01 << (PIXEL_BASE + 9)) //red?
        orreq  r12, #0x00001000                //palette 0

        cmp    r9, #(0x06 << (PIXEL_BASE + 9)) //cyan?
        orreq  r11, r11, #0x80000000
        cmpne  r9, #(0x05 << (PIXEL_BASE + 9)) //magenta?
        orreq  r11, #0x00002000                //palette 1

        cmp    r9, #(0x07 << (PIXEL_BASE + 9)) //white?
        orreq  r11, r11, #0x80000000

.endm


.macro  NTSC_CAPTURE_BITS_8BPP_WIDE
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        mov    r11, r11, lsr #2
        bic    r11, #0x00003000

        and    r9, r8, #(0x07 << PIXEL_BASE)

        cmp    r9, #(0x01 << PIXEL_BASE) //red?
        cmpne  r9, #(0x03 << PIXEL_BASE) //yellow?
        orreq  r11, r11, #0x40000000
        cmpne  r9, #(0x02 << PIXEL_BASE) //green?
        orreq  r11, #0x00001000           //palette 0

        cmp    r9, #(0x05 << PIXEL_BASE) //magenta?
        orreq  r11, r11, #0x40000000
        cmpne  r9, #(0x06 << PIXEL_BASE) //cyan?
        orreq  r11, #0x00002000                   //palette 1

        cmp    r9, #(0x07 << PIXEL_BASE) //white?
        orreq  r11, r11, #0x40000000

        and    r9, r8, #(0x07 << (PIXEL_BASE + 6))

        cmp    r9, #(0x02 << (PIXEL_BASE + 6)) //green?
        cmpne  r9, #(0x03 << (PIXEL_BASE + 6)) //yellow?
        orreq  r11, r11, #0x80000000
        cmpne  r9, #(0x01 << (PIXEL_BASE + 6)) //red?
        orreq  r11, #0x00001000                   //palette 0

        cmp    r9, #(0x06 << (PIXEL_BASE + 6)) //cyan?
        orreq  r11, r11, #0x80000000
        cmpne  r9, #(0x05 << (PIXEL_BASE + 6)) //magenta?
        orreq  r11, #0x00002000                   //palette 1

        cmp    r9, #(0x07 << (PIXEL_BASE + 6)) //white?
        orreq  r11, r11, #0x80000000
.endm

.text
.global capture_line_ntsc_8bpp_cga
.global capture_line_ntsc_8bpp_mono
.global capture_line_ntsc_sixbits_8bpp_cga
.global capture_line_ntsc_sixbits_16bpp_cga
.global capture_line_ntsc_sixbits_8bpp_mono
.global capture_line_ntsc_sixbits_8bpp_mono_auto
.global capture_line_ntsc_sixbits_double_8bpp_mono
.global capture_line_ntsc_sixbits_double_8bpp_mono_auto
.global bit_count

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

       .ltorg

.align 6
        // *** 8 bit ***

        b       preload_capture_line_ntsc_8bpp_cga
capture_line_ntsc_8bpp_cga:
        push    {lr}
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        SKIP_PSYNC_NO_OLD_CPLD_NTSC_3BPP
        mov    r1, r1, lsr #1
        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE
        NTSC_CAPTURE_BITS_8BPP
        WAIT_FOR_PSYNC_EDGE
        NTSC_CAPTURE_BITS_8BPP
loop_8bpp3:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP           // input in r8
        FULL_NTSC_DECODE_CGA r5
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP           // input in r8
        FULL_NTSC_DECODE_CGA r6

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP           // input in r8
        FULL_NTSC_DECODE_CGA r7
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP           // input in r8
        FULL_NTSC_DECODE_CGA r10

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp3
        pop     {r0, pc}

preload_capture_line_ntsc_8bpp_cga:
        PRELOAD_BITCOUNT
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_8bpp_cga

       .ltorg

.align 6
        b       preload_capture_line_ntsc_8bpp_mono
capture_line_ntsc_8bpp_mono:
        push    {lr}
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        tst    r12, #NTSC_ARTIFACT
        beq    no_ntsc_8bpp_mono
        SKIP_PSYNC_NO_OLD_CPLD_NTSC_3BPP
        mov    r1, r1, lsr #1
        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE
        NTSC_CAPTURE_BITS_8BPP_MONO
        WAIT_FOR_PSYNC_EDGE
        NTSC_CAPTURE_BITS_8BPP_MONO
loop_8bpp_mono3:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO      // input in r8
        NTSC_DECODE r5
        NTSC_DECODE_FINAL r5
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO      // input in r8
        NTSC_DECODE r6
        NTSC_DECODE_FINAL r6

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO      // input in r8
        NTSC_DECODE r7
        NTSC_DECODE_FINAL r7
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO      // input in r8
        NTSC_DECODE r10
        NTSC_DECODE_FINAL r10

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp_mono3
        pop     {r0, pc}

       .ltorg

no_ntsc_8bpp_mono:
        SKIP_PSYNC_NO_OLD_CPLD_NTSC_3BPP
        mov    r1, r1, lsr #1
loop_8bppnoburst:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_NORMAL r11 r5  // input in r8
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_NORMAL r11 r6  // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_NORMAL r11 r7  // input in r8
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_NORMAL r11 r10 // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bppnoburst

        pop     {r0, pc}

preload_capture_line_ntsc_8bpp_mono:
        PRELOAD_BITCOUNT
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_8bpp_mono
        .ltorg

//***************************************************************************************
.align 6
bit_count:
   .word 0x40404040
   .word 0x40404040
   .word 0x40404040
   .word 0x40404050
   .word 0x40404040
   .word 0x40404050
   .word 0x40405050
   .word 0x40405060
   .word 0x40404040
   .word 0x40404050
   .word 0x40405050
   .word 0x40405060
   .word 0x40505050
   .word 0x40505060
   .word 0x40506060
   .word 0x40506060
   .word 0x40404040
   .word 0x40404040
   .word 0x40405040
   .word 0x40405050
   .word 0x40505040
   .word 0x40505050
   .word 0x40506050
   .word 0x40506060
   .word 0x50505040
   .word 0x50505050
   .word 0x50506050
   .word 0x50506060
   .word 0x50606050
   .word 0x50606060
   .word 0x50606060
   .word 0x50606060
   .word 0x40404040
   .word 0x40404040
   .word 0x40404040
   .word 0x40404050
   .word 0x40504040
   .word 0x40504050
   .word 0x40505050
   .word 0x40505060
   .word 0x50504040
   .word 0x50504050
   .word 0x50505050
   .word 0x50505060
   .word 0x50605050
   .word 0x50605060
   .word 0x50606060
   .word 0x50606060
   .word 0x50504040
   .word 0x50504040
   .word 0x50505040
   .word 0x50505050
   .word 0x50605040
   .word 0x50605050
   .word 0x50606050
   .word 0x50606060
   .word 0x60605040
   .word 0x60605050
   .word 0x60606050
   .word 0x60606060
   .word 0x60606050
   .word 0x60606060
   .word 0x60606060
   .word 0x60606060
   .word 0x40404040
   .word 0x40404040
   .word 0x40404040
   .word 0x40404050
   .word 0x40404040
   .word 0x40404050
   .word 0x40405050
   .word 0x40405060
   .word 0x50404040
   .word 0x50404050
   .word 0x50405050
   .word 0x50405060
   .word 0x50505050
   .word 0x50505060
   .word 0x50506060
   .word 0x50506060
   .word 0x50404040
   .word 0x50404040
   .word 0x50405040
   .word 0x50405050
   .word 0x50505040
   .word 0x50505050
   .word 0x50506050
   .word 0x50506060
   .word 0x60505040
   .word 0x60505050
   .word 0x60506050
   .word 0x60506060
   .word 0x60606050
   .word 0x60606060
   .word 0x60606060
   .word 0x60606060
   .word 0x50404040
   .word 0x50404040
   .word 0x50404040
   .word 0x50404050
   .word 0x50504040
   .word 0x50504050
   .word 0x50505050
   .word 0x50505060
   .word 0x60504040
   .word 0x60504050
   .word 0x60505050
   .word 0x60505060
   .word 0x60605050
   .word 0x60605060
   .word 0x60606060
   .word 0x60606060
   .word 0x60504040
   .word 0x60504040
   .word 0x60505040
   .word 0x60505050
   .word 0x60605040
   .word 0x60605050
   .word 0x60606050
   .word 0x60606060
   .word 0x60605040
   .word 0x60605050
   .word 0x60606050
   .word 0x60606060
   .word 0x60606050
   .word 0x60606060
   .word 0x60606060
   .word 0x60606060

       .ltorg

.align 6
        b       preload_capture_line_ntsc_sixbits_8bpp_cga

capture_line_ntsc_sixbits_8bpp_cga:
        push    {lr}
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        tst    r12, #(NTSC_MEDIUM | NTSC_SOFT)
        beq    full_capture_line_ntsc_sixbits_8bpp_cga
        SKIP_PSYNC_NO_OLD_CPLD_NTSC
        mov    r1, r1, lsr #2
        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
loop_8bpp6:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        NTSC_DECODE_CGA r5
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        NTSC_DECODE_CGA r6

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        NTSC_DECODE_CGA r7
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        NTSC_DECODE_CGA r10

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp6

        pop     {r0, pc}

        .ltorg
preload_capture_line_ntsc_sixbits_8bpp_cga:
        PRELOAD_BITCOUNT
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_8bpp_cga

        .ltorg

full_capture_line_ntsc_sixbits_8bpp_cga:
        SKIP_PSYNC_NO_OLD_CPLD_NTSC
        mov    r1, r1, lsr #2
        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
full_loop_8bpp6:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        FULL_NTSC_DECODE_CGA r5
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        FULL_NTSC_DECODE_CGA r6

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        FULL_NTSC_DECODE_CGA r7
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_WIDE                   // input in r8
        FULL_NTSC_DECODE_CGA r10

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     full_loop_8bpp6

        pop     {r0, pc}


        .ltorg

        // *** 8 bit mono ***
.align 6
        b       preload_capture_line_ntsc_sixbits_8bpp_mono
capture_line_ntsc_sixbits_8bpp_mono:
        push    {lr}
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        SKIP_PSYNC_NO_OLD_CPLD_NTSC
        mov    r1, r1, lsr #2
        tst    r12, #(NTSC_MEDIUM | NTSC_SOFT)
        beq    full_link_8bpp_mono6
        b      link_8bpp_mono6

preload_capture_line_ntsc_sixbits_8bpp_mono:
        PRELOAD_BITCOUNT
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_8bpp_mono

        .ltorg


        // *** 8 bit mono auto ***
.align 6
        b      preload_capture_line_ntsc_sixbits_8bpp_mono_auto
capture_line_ntsc_sixbits_8bpp_mono_auto:
        orr    r3, r3, #BITDUP_LINE_CONDITION_DETECTED         //detecting colour burst
        push    {lr}
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        tst    r12, #(NTSC_MEDIUM | NTSC_SOFT)
        beq    full_capture_line_ntsc_sixbits_8bpp_mono_auto
        SKIP_PSYNC_NO_OLD_CPLD_NTSC         // returns r9 != 0 if burst detected
        mov    r1, r1, lsr #2
        eor    r8, r12, #NTSC_Y_INVERT     // invert signal so tests can cascade
        cmp    r9, #0                       //no burst?
        biceq  r3, #BITDUP_LINE_CONDITION_DETECTED
        tsteq  r8, #NTSC_Y_INVERT
        biceq  r12, #NTSC_ARTIFACT          // if no burst and inverted video then force mono mode irrespective of artifact setting
link_8bpp_mono6:
        tst    r12, #NTSC_ARTIFACT
        beq  no_ntsc_sixbits_8bpp_mono_auto

preload_ntsc:

        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
loop_8bpp_mono6_auto:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NTSC_DECODE r5
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NTSC_DECODE r6

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NTSC_DECODE r7
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NTSC_DECODE r10

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp_mono6_auto
        pop     {r0, pc}

no_ntsc_sixbits_8bpp_mono_auto:
loop_8bpp_mono6_auto_noburst:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_HIGH_BITS_8BPP_WIDE r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_HIGH_BITS_8BPP_WIDE r6                // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_HIGH_BITS_8BPP_WIDE r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_HIGH_BITS_8BPP_WIDE r10               // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp_mono6_auto_noburst

        pop     {r0, pc}

preload_capture_line_ntsc_sixbits_8bpp_mono_auto:

        push   {lr}
        PRELOAD_BITCOUNT
        SETUP_DUMMY_PARAMETERS
        SKIP_PSYNC_NO_OLD_CPLD_NTSC
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        tst    r12, #(NTSC_MEDIUM | NTSC_SOFT)
        beq    preload_ntsc_full
        b      preload_ntsc

        .ltorg

full_capture_line_ntsc_sixbits_8bpp_mono_auto:
        SKIP_PSYNC_NO_OLD_CPLD_NTSC         // returns r9 != 0 if burst detected
        mov    r1, r1, lsr #2
        eor    r8, r12, #NTSC_Y_INVERT     // invert signal so tests can cascade
        cmp    r9, #0                       //no burst?
        biceq  r3, #BITDUP_LINE_CONDITION_DETECTED
        tsteq  r8, #NTSC_Y_INVERT
        biceq  r12, #NTSC_ARTIFACT          // if no burst and inverted video then force mono mode irrespective of artifact setting
full_link_8bpp_mono6:
        tst    r12, #NTSC_ARTIFACT
        beq    no_ntsc_sixbits_8bpp_mono_auto

preload_ntsc_full:

        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
full_loop_8bpp_mono6_auto:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NTSC_DECODE r5
        NTSC_DECODE_FINAL r5
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NTSC_DECODE r6
        NTSC_DECODE_FINAL r6

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NTSC_DECODE r7
        NTSC_DECODE_FINAL r7
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NTSC_DECODE r10
        NTSC_DECODE_FINAL r10

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     full_loop_8bpp_mono6_auto
        pop     {r0, pc}


        .ltorg


.align 6
        // *** 8 bit mono double ***
        b      preload_capture_line_ntsc_sixbits_double_8bpp_mono
capture_line_ntsc_sixbits_double_8bpp_mono:
        push   {lr}
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        bic    r12, #NTSC_DONE_FIRST
        tst    r12, #NTSC_ARTIFACT
        beq    no_ntsc_sixbits_double_8bpp_mono
        SKIP_PSYNC_NO_OLD_CPLD_NTSC                // returns with ntsc_status in r12
        mov    r1, r1, lsr #1
        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE
loop_8bppndm:

        WAIT_FOR_PSYNC_EDGE                         // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE     // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r5
        NTSC_DECODE_FINAL r5
        mov  r11, r11, lsl #1
        WAIT_FOR_PSYNC_EDGE                         // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE     // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r6
        NTSC_DECODE_FINAL r6
        mov  r11, r11, lsl #1

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        bic     r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE                          // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE      // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r7
        NTSC_DECODE_FINAL r7
        mov  r11, r11, lsl #1
        WAIT_FOR_PSYNC_EDGE                          // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE      // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r10
        NTSC_DECODE_FINAL r10
        mov  r11, r11, lsl #1

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bppndm
        bic     r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        pop     {r0, pc}

preload_capture_line_ntsc_sixbits_double_8bpp_mono:
        PRELOAD_BITCOUNT
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_double_8bpp_mono


no_ntsc_sixbits_double_8bpp_mono:
        SKIP_PSYNC_NO_OLD_CPLD_NTSC                // returns with ntsc_status in r12
        mov    r1, r1, lsr #1
loop_8bppdm:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_WIDE r11 r5          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_WIDE r11 r6          // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        bic     r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_WIDE r11 r7          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_WIDE r11 r10         // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bppdm
        bic     r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        pop     {r0, pc}

        .ltorg

.align 6
        // *** 8 bit mono double auto ***
        b      preload_capture_line_ntsc_sixbits_double_8bpp_mono_auto
capture_line_ntsc_sixbits_double_8bpp_mono_auto:
        push   {lr}
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        bic    r12, #NTSC_DONE_FIRST
        tst    r12, #NTSC_ARTIFACT
        beq    no_ntsc_sixbits_double_8bpp_mono_auto
        SKIP_PSYNC_NO_OLD_CPLD_NTSC                // returns with ntsc_status in r12
        mov    r1, r1, lsr #1


        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE
loop_8bppnd_auto:

        WAIT_FOR_PSYNC_EDGE                         // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE     // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r5
        NTSC_DECODE_FINAL r5
        mov  r11, r11, lsl #1
        WAIT_FOR_PSYNC_EDGE                         // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE     // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r6
        NTSC_DECODE_FINAL r6
        mov  r11, r11, lsl #1

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE                          // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE      // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r7
        NTSC_DECODE_FINAL r7
        mov  r11, r11, lsl #1
        WAIT_FOR_PSYNC_EDGE                          // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE      // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r10
        NTSC_DECODE_FINAL r10
        mov  r11, r11, lsl #1

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bppnd_auto
        pop     {r0, pc}

preload_capture_line_ntsc_sixbits_double_8bpp_mono_auto:
        PRELOAD_BITCOUNT
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_double_8bpp_mono_auto

no_ntsc_sixbits_double_8bpp_mono_auto:
        SKIP_PSYNC_NO_OLD_CPLD_NTSC                // returns with ntsc_status in r12
        mov    r1, r1, lsr #1
loop_8bppd_auto:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_WIDE r11 r5          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_WIDE r11 r6          // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_WIDE r11 r7          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_WIDE r11 r10         // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bppd_auto
        pop     {r0, pc}

        .ltorg



.macro CAPTURE_SIX_BITS_16BPP_0 reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        and    r9, r8, #(0x07 << PIXEL_BASE)
        tst    r8, #(0x10 << PIXEL_BASE)
        orrne  r9, r9, #(0x08 << PIXEL_BASE)
        mov    \reg, r9, lsr #PIXEL_BASE
.endm

.macro CAPTURE_SIX_BITS_16BPP_1 reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24
        and    r9, r8, #(0x07 << (PIXEL_BASE + 6))
        tst    r8, #(0x10 << (PIXEL_BASE + 6))
        orrne  r9, r9, #(0x08 << (PIXEL_BASE + 6))
        orr    \reg, \reg, r9, lsl #(8 - (PIXEL_BASE + 6))
.endm

.macro CAPTURE_SIX_BITS_16BPP_2 reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        and    r9, r8, #(0x07 << PIXEL_BASE)
        tst    r8, #(0x10 << PIXEL_BASE)
        orrne  r9, r9, #(0x08 << PIXEL_BASE)
        orr    \reg, \reg, r9, lsl #(16 - PIXEL_BASE)
.endm

.macro CAPTURE_SIX_BITS_16BPP_3 reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24
        and    r9, r8, #(0x07 << (PIXEL_BASE + 6))
        tst    r8, #(0x10 << (PIXEL_BASE + 6))
        orrne  r9, r9, #(0x08 << (PIXEL_BASE + 6))
        orr    \reg, \reg, r9, lsl #(24 - (PIXEL_BASE + 6))
.endm

.macro SWAP reg0 reg1
        eor \reg0, \reg0, \reg1
        eor \reg1, \reg0, \reg1
        eor \reg0, \reg0, \reg1
.endm

.global cga_process_artifact
.global cga_render_words
.global Composite_Process_Asm
.global CGA_Composite_Table
.global validate_cga
.global video_ri
.global video_rq
.global video_gi
.global video_gq
.global video_bi
.global video_bq

.macro DECODE_CGA phase bits           //rgbi value enters in r0  //r12 now free
//mov r0, #0x02

        ldmia r14, {r1-r9}             //r1=old rgbi from last capture r2-r9 = 3 to -4
//and r1, #0x0f
        sub   r11, r14, #(pixelbuffer - CGA_Composite_Table)
        mov  r1, r1, lsl #(6 + 2)      //6 shifted 2 because words not bytes
        orr  r1, r1, r0, lsl #(2 + 2)  //2 shifted 2 because words not bytes
.if \phase != 0                         //omit instruction if phase is 0
        orr  r1, r1, #(\phase << 2)      //shifted by 2 as word not byte
.endif
        ldr  r1, [r11, r1]             // read CGA_Composite_Table
        // r1 - r5 now = i(2) to i(-2)

        //r10 = ap[1] = (-i[-2]+((i[0])<<1)-i[2])<<1;
        //r11 = bp[1] = (-i[-1]+i[1])<<2;

        //r10 = ap[1] = (-r5+(r3<<1)-r1)<<1;
        //r11 = bp[1] = (-r4+r2)<<2;
     mov r10, r3, lsl #1
     sub r10, r10, r1
     //   rsb   r10, r1, r3, lsl #1
        sub   r11, r2, r4
        sub   r10, r10, r5
        mov   r11, r11, lsl #2
        mov   r10, r10, lsl #1
        // r6 = adjusted i[0], r7 = adjusted i[-1]
        mov   r5, r2, lsl #3           //(i[1]<<3)
        sub   r5, r5, r10              //adjusted i[1] = (i[1]<<3) - ap[1]
        // now r5 = adjusted i[1]
        add   r12, r7, r5               //r12 = Y = (adjusted) i[-1] +i[1]
        mov   r7, r10                   //r7 = ap[1]

        // r8 = ap[0], r9 = bp[0]

        mov   r10, r8                   // r10 is now ap[0]  r9 is now bp[0]
        mov   r8, r11                   // r8 is now bp[1]

        stmia r14, {r0-r8}              //save last rgbi value (r0) plus yuv values (r1-r4) plus adjusted i[1] & i[0] values plus ap[1] & bp[1]

        add   r11, r14, #(video_ri - pixelbuffer)
        ldmia r11, {r0-r5}

        add   r12, r12, r6, lsl #1      //r12 = Y = (adjusted) i[0]+i[0] + i[-1] +i[1] (c + d)
        mov   r12, r12, lsl #8         //r12 = c+d << 8

.if \phase == 1
        rsb   r9, r9, #0       //negate b(0)
        SWAP  r9 r10           //swap a(0) & b(0)
.elseif \phase == 2
        rsb   r10, r10, #0     //negate a(0)
        rsb   r9, r9, #0       //negate b(0)
.elseif \phase == 3
        rsb   r10, r10, #0     //negate a(0)
        SWAP  r9 r10           //swap a(0) & b(0)
.endif
        mul   r0, r0, r10      //video_ri*(a)
        mul   r1, r1, r9       //video_rq*(b)

        mul   r2, r2, r10      //video_gi*(a)
        mul   r3, r3, r9       //video_gq*(b)

        add   r0, r0, r1       //video_ri*(a) + video_rq*(b);
        adds  r0, r0, r12      //rr = y + video_ri*(a) + video_rq*(b);
//     movs r0, r12
        movmi r0, #0

.if \bits == 4
        mov   r0, r0, lsr #(13 + 4)  //v >>= 13 but add 4 as 4 bit RGB
.else
        mov   r0, r0, lsr #13        //v >>= 13 for 8 bit RGB
.endif

        mul   r4, r4, r10      //video_bi*(a)
        mul   r5, r5, r9       //video_bq*(b)

        add   r1, r2, r3       //video_gi*(a) + video_gq*(b);
        adds  r1, r1, r12      //gg = y + video_gi*(a) + video_gq*(b);
//     movs r1, r12
        movmi r1, #0

.if \bits == 4
        mov   r1, r1, lsr #(13 + 4)  //v >>= 13 but add 4 as 4 bit RGB
.else
        mov   r1, r1, lsr #13        //v >>= 13 for 8 bit RGB
.endif

        add   r2, r4, r5       //video_bi*(a) + video_bq*(b);
        adds  r2, r2, r12      //bb = y + video_bi*(a) + video_bq*(b);
//    movs r2, r12
        movmi r2, #0

.if \bits == 4
        mov   r2, r2, lsr #(13 + 4)  //v >>= 13 but add 4 as 4 bit RGB
.else
        mov   r2, r2, lsr #13        //v >>= 13 for 8 bit RGB
.endif

.if \bits == 4
        cmp   r0, #0x10
        movge r0, #0x0f
        cmp   r1, #0x10
        movge r1, #0x0f
        cmp   r2, #0x10
        movge r2, #0x0f
        orr   r2, r2, r1, lsl #4
        orr   r0, r2, r0, lsl #8
.else
        cmp   r0, #0x100
        movge r0, #0xff
        cmp   r1, #0x100
        movge r1, #0xff
        cmp   r2, #0x100
        movge r2, #0xff
        orr   r2, r2, r1, lsl #8
        orr   r0, r2, r0, lsl #16
.endif

.endm


        .align 6
Composite_Process_Asm:
        push {r1-r12,lr}
        //r0= cga_screen_blocks_copy
        //r1= cga_rgbi_table
        //r2= writeflag
        str r0, saved_blocks
        str r1, saved_table
        str r2, saved_flag
Composite_Process_Asm_loop:
        adrl r14, pixelbuffer
        ldr  r1, saved_table
        ldr  r0, [r1]
        and  r0, #0x0f
        DECODE_CGA 0 4
        str  r0, decoded_pixel
        ldr  r1, saved_table
        ldr  r0, [r1]
        mov  r0, r0, lsr #8
        and  r0, #0x0f
        DECODE_CGA 1 4
        ldr  r1, decoded_pixel
        orr  r1, r0, lsl #16
        str  r1, decoded_pixel

        ldr  r1, saved_table
        ldr  r0, [r1]
        mov  r0, r0, lsr #16
        and  r0, #0x0f
        DECODE_CGA 2 4
        str  r0, decoded_pixel + 4
        ldr  r1, saved_table
        ldr  r0, [r1]
        mov  r0, r0, lsr #24
        and  r0, #0x0f
        DECODE_CGA 3 4
        ldr  r1, decoded_pixel + 4
        orr  r1, r0, lsl #16
        str  r1, decoded_pixel + 4


        ldr  r1, saved_table
        ldr  r0, [r1, #4]
        and  r0, #0x0f
        DECODE_CGA 0 4
        str  r0, decoded_pixel + 8
        ldr  r1, saved_table
        ldr  r0, [r1, #4]
        mov  r0, r0, lsr #8
        and  r0, #0x0f
        DECODE_CGA 1 4
        ldr  r1, decoded_pixel + 8
        orr  r1, r0, lsl #16
        str  r1, decoded_pixel + 8

        ldr  r1, saved_table
        ldr  r0, [r1, #4]
        mov  r0, r0, lsr #16
        and  r0, #0x0f
        DECODE_CGA 2 4
        str  r0, decoded_pixel + 12
        ldr  r1, saved_table
        ldr  r0, [r1, #4]
        mov  r0, r0, lsr #24
        and  r0, #0x0f
        DECODE_CGA 3 4
        ldr  r1, decoded_pixel + 12
        orr  r1, r0, lsl #16
        str  r1, decoded_pixel + 12

        ldr  r2, saved_flag
        cmp  r2, #0
        beq  norendercga

        adr   r0, decoded_pixel
        ldmia r0, {r5-r7, r10}

        adrl  r4, cga_screen_pointer_copy
        ldmia r4, {r0-r3, r11, r12}

        orr   r5, r5, r11
        orr   r6, r6, r11
        orr   r7, r7, r11
        orr   r10, r10, r11

        WRITE_R5_R6_R7_R10_16BPP
        adrl  r4, cga_screen_pointer_copy
        str   r0, [r4]
norendercga:
        ldr   r0, saved_table
        add   r0, r0, #8
        str   r0, saved_table
        ldr   r1, saved_blocks
        subs  r1, r1, #1
        str   r1, saved_blocks
        bne   Composite_Process_Asm_loop

        pop  {r1-r12, pc}

saved_blocks:
    .word 0
saved_table:
    .word 0
saved_flag:
    .word 0
decoded_pixel:
        .word 0
        .word 0
        .word 0
        .word 0


        .align 6
CGA_Composite_Table:
        .space (4096)
        .align 6
pixelbuffer:
        .word 0 // 2 r1 (stored oldrgbi <<6  + new rgbi <<2)  (when loaded contains stored old rgbi but after contains looked up YUV value)
        .word 0 // 1 r2
        .word 0 // 0 r3
        .word 0 //-1 r4
        .word 0 //-2 r5
i_buffer:
        .word 0
        .word 0
ap_buffer:
        .word 0
        .word 0
        .align 6
decoded_pixels: //64 bit aligned
        .word 0
        .word 0
        .word 0
        .word 0

video_ri:       //64 bit aligned
        .word 0
video_rq:
        .word 0
video_gi:
        .word 0
video_gq:
        .word 0
video_bi:
        .word 0
video_bq:
        .word 0

saved_regs:
        .word 0
        .word 0
        .word 0
        .word 0
        .word 0



        .align 6

cga_rgbi_table:
        .space 2048, 0

        .align 6
        // *** 16 bit ***
        b       preload_capture_line_ntsc_sixbits_16bpp_cga
capture_line_ntsc_sixbits_16bpp_cga:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11
        subs  r7, r7, #3
        moveq r7, #1
        movmi r7, #1
        SKIP_PSYNC_NO_OLD_CPLD_HIGH_LATENCY
        str   r0,  cga_screen_pointer
        mov   r1,  r1, lsr #2
        add   r1,  r1, #1
        str   r1,  cga_screen_blocks
        add   r1,  r1, #1
        str   r2,  cga_screen_pitch
        str   r3,  cga_screen_flags
        str   r11, cga_screen_alpha
        str   r12, cga_screen_intensity
        adrl  r5,  cga_rgbi_table
loop_16bpp:
        WAIT_FOR_PSYNC_EDGE_FAST            // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_16BPP_0 r6         // input in r8
        CAPTURE_SIX_BITS_16BPP_1 r6         // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST            // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_16BPP_2 r6         // input in r8
        CAPTURE_SIX_BITS_16BPP_3 r6         // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST            // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_16BPP_0 r7         // input in r8
        CAPTURE_SIX_BITS_16BPP_1 r7         // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST            // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_16BPP_2 r7         // input in r8
        CAPTURE_SIX_BITS_16BPP_3 r7         // input in r8

        stmia r5!, {r6-r7}

        subs    r1, r1, #1
        bne     loop_16bpp

        ldr     r5, =start_core_1_code
        mov     r6, #1
        str     r6, [r5]                    //semaphore to start core 1 with reenigne's artifact code
        dmb     //ensure memory up to date
        sev     //send event to wake up core 1
        pop     {r0, pc}

preload_capture_line_ntsc_sixbits_16bpp_cga:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_16bpp_cga

cga_screen_pointer:
        .word 0
cga_screen_blocks:
        .word 0
cga_screen_pitch:
        .word 0
cga_screen_flags:
        .word 0
cga_screen_alpha:
        .word 0
cga_screen_intensity:
        .word 0

cga_screen_pointer_copy:
        .word 0
cga_screen_blocks_copy:
        .word 0
cga_screen_pitch_copy:
        .word 0
cga_screen_flags_copy:
        .word 0
cga_screen_alpha_copy:
        .word 0
cga_screen_intensity_copy:
        .word 0

cga_process_artifact:                 //called from core 1
        push {lr}
        adr   r0, cga_screen_pointer
        adr   r1, cga_screen_pointer_copy
        ldmia r0, {r2 - r7}
        stmia r1, {r2 - r7}
        ldr   r0, cga_screen_blocks_copy
        adrl  r1, cga_rgbi_table
        mov   r2, #1
        bl    Composite_Process       //call reenigne's artifact code
        //bl    Composite_Process_Asm  //in progress
        pop {pc}

cga_render_words:                     //write 4 words of rgb data (eight 16 bit pixels) to the screen. (Called from reenigne's artifact code)
        push  {r4-r12, lr}
        ldr   r11, cga_screen_alpha_copy
        ldr   r12, cga_screen_intensity_copy
        orr   r5, r0, r11
        orr   r6, r1, r11
        orr   r7, r2, r11
        orr   r10, r3, r11
        adr   r4, cga_screen_pointer_copy
        ldmia r4, {r0-r3}
        WRITE_R5_R6_R7_R10_16BPP
        str   r0, cga_screen_pointer_copy
        pop   {r4-r12, pc}


preload_capture_line_default_twelvebits_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_16bpp_cga

.macro CAPTURE_SIX_BITS_MONO_16BPP_0 reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        mov    \reg, #0
        tst    r12, #NTSC_Y_INVERT
        eorne  r8, r8, #(0x12 << PIXEL_BASE)
        eorne  r8, r8, #(0x12 << (PIXEL_BASE + 6))
        and    r9, r8, #(0x07 << PIXEL_BASE)
        cmp    r9, #(0x07 << PIXEL_BASE)
        cmpne  r9, #(0x02 << PIXEL_BASE)
        orreq  \reg, #0x0F
.endm

.macro CAPTURE_SIX_BITS_MONO_16BPP_1 reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24
        and    r9, r8, #(0x07 << (PIXEL_BASE + 6))
        cmp    r9, #(0x07 << (PIXEL_BASE + 6))
        cmpne  r9, #(0x02 << (PIXEL_BASE + 6))
        orreq  \reg, #0x0F00
.endm

.macro CAPTURE_SIX_BITS_MONO_16BPP_2 reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        tst    r12, #NTSC_Y_INVERT
        eorne  r8, r8, #(0x12 << PIXEL_BASE)
        eorne  r8, r8, #(0x12 << (PIXEL_BASE + 6))
        and    r9, r8, #(0x07 << PIXEL_BASE)
        cmp    r9, #(0x07 << PIXEL_BASE)
        cmpne  r9, #(0x02 << PIXEL_BASE)
        orreq  \reg, #0x0F0000
.endm

.macro CAPTURE_SIX_BITS_MONO_16BPP_3 reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24
        and    r9, r8, #(0x07 << (PIXEL_BASE + 6))
        cmp    r9, #(0x07 << (PIXEL_BASE + 6))
        cmpne  r9, #(0x02 << (PIXEL_BASE + 6))
        orreq  \reg, #0x0F000000
.endm

.global capture_line_ntsc_sixbits_16bpp_mono
.global capture_line_ntsc_sixbits_16bpp_mono_auto

        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_ntsc_sixbits_16bpp_mono
capture_line_ntsc_sixbits_16bpp_mono:
        push    {lr}
        SETUP_VSYNC_DEBUG_NOINVERT_16BPP_R11
        str    r11, cga_screen_alpha
        str    r12, cga_screen_intensity
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        subs   r7, r7, #3
        moveq  r7, #1
        movmi  r7, #1
        SKIP_PSYNC_NO_OLD_CPLD_NTSC         // returns r9 != 0 if burst detected
        b      link_16bpp_MONO
preload_capture_line_ntsc_sixbits_16bpp_mono:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_16bpp_mono

        b       preload_capture_line_ntsc_sixbits_16bpp_mono_auto
capture_line_ntsc_sixbits_16bpp_mono_auto:
        push    {lr}
        orr    r3, r3, #BITDUP_LINE_CONDITION_DETECTED         //detecting colour burst
        SETUP_VSYNC_DEBUG_NOINVERT_16BPP_R11
        str    r11, cga_screen_alpha
        str    r12, cga_screen_intensity
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        subs  r7, r7, #3
        moveq r7, #1
        movmi r7, #1
        SKIP_PSYNC_NO_OLD_CPLD_NTSC         // returns r9 != 0 if burst detected
        eor    r8, r12, #NTSC_Y_INVERT     // invert signal so tests can cascade
        cmp    r9, #0                       //no burst?
        biceq  r3, #BITDUP_LINE_CONDITION_DETECTED
        tsteq  r8, #NTSC_Y_INVERT
        biceq  r12, #NTSC_ARTIFACT          // if no burst and inverted video then force mono mode irrespective of artifact setting

link_16bpp_MONO:
        tst    r12, #NTSC_ARTIFACT
        beq    normal_6_16_capture
        str    r0, cga_screen_pointer
        mov    r1, r1, lsr #2
        add    r1, r1, #1
        str    r1, cga_screen_blocks
        add    r1, r1, #1
        str    r2, cga_screen_pitch
        str    r3, cga_screen_flags
        adrl   r5, cga_rgbi_table
loop_16bpp_MONO:
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_MONO_16BPP_0 r6         // input in r8
        CAPTURE_SIX_BITS_MONO_16BPP_1 r6          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_MONO_16BPP_2 r6          // input in r8
        CAPTURE_SIX_BITS_MONO_16BPP_3 r6          // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_MONO_16BPP_0 r7         // input in r8
        CAPTURE_SIX_BITS_MONO_16BPP_1 r7          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_MONO_16BPP_2 r7         // input in r8
        CAPTURE_SIX_BITS_MONO_16BPP_3 r7         // input in r8

        stmia r5!, {r6-r7}

        subs    r1, r1, #1
        bne     loop_16bpp_MONO

        ldr     r5, =start_core_1_code
        mov     r6, #1
        str     r6, [r5]                    //semaphore to start core 1 with reenigne's artifact code
        dmb     //ensure memory up to date
        sev     //send event to wake up core 1
        pop     {r0, pc}

normal_6_16_capture:
        ldr    r11, cga_screen_alpha
        ldr    r12, cga_screen_intensity
        mov    r1, r1, lsr #2
        //add    r1, r1, #2
        ldr    r14, =palette_data_16
        WAIT_FOR_PSYNC_EDGE_FAST
        WAIT_FOR_PSYNC_EDGE_FAST
        WAIT_FOR_PSYNC_EDGE_FAST
loop_16bppn:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_16BPP r11 r5                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_16BPP r11 r6                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_16BPP r11 r7                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_16BPP r11 r10                // input in r8

        WRITE_R5_R6_R7_R10_16BPP

        subs    r1, r1, #1
        bne     loop_16bppn

        pop     {r0, pc}


preload_capture_line_ntsc_sixbits_16bpp_mono_auto:
        ldr    r0, =palette_data_16
        mov    r1, #64
preload_loop:
        ldr    r2, [r0], #4
        subs   r1, r1, #1
        bne    preload_loop
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_16bpp_mono_auto



