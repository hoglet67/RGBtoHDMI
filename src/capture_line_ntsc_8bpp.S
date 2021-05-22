#include "rpi-base.h"
#include "defs.h"

#include "macros.S"


.macro  SKIP_PSYNC_NO_OLD_CPLD_NTSC
        // only called if 6 bits/pixel in non-fast mode (old CPLDs v1 & v2 don't work at 6bpp so no need for test)
        WAIT_FOR_CSYNC_0_FAST_SKIP_HSYNC
        READ_CYCLE_COUNTER r10
        push  {r10}
        bic   r3, r3, #PSYNC_MASK         // wait for zero after CSYNC
        // Wait for the end of hsync
        WAIT_FOR_CSYNC_1_FAST
        READ_CYCLE_COUNTER r14
        // Calculate length of low hsync pulse (in ARM cycles = ns)
        subs   r10, r14, r10
        rsbmi  r10, r10, #0
        // Calculate length of low hsync pulse (in ARM cycles = ns)
        // Start with the configured horizontal offset
        // Implement half character horizontal scrolling:
        // - a "short"  hsync is 3.5us, leave h_offset as-is
        // - a "normal" hsync is 4.0us, increment h_offset by 1
        // - a "long"   hsync is 4.5us, increment h_offset by 2
        // So test against two thresholds inbetween these values

        // new CPLD code only (not called from CPLD v1 & v2)
        mov    r8, r7
        cmp    r10, r9, lsr #16     //HSYNC_SCROLL_HI
        addlt  r8, r8, #1
        orrgt  r3, r3, #BIT_INHIBIT_MODE_DETECT
        bic    r9, r9, #0xff000000
        bic    r9, r9, #0x00ff0000
        cmp    r10, r9              //HSYNC_SCROLL_LO
        addlt  r8, r8, #1
        orrlt  r3, r3, #BIT_INHIBIT_MODE_DETECT
        tst    r3, #BIT_NO_H_SCROLL
        moveq  r7, r8                      // only allow fine sideways scrolling in bbc / electron mode (causes timing issues in ega mode)
        // Skip the configured number of psync edges (modes 0..6: edges every 250ns, mode 7: edges ever 333ns)
        mov    r9, #0
skip_psync_loop_no_old\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        ands   r10, r8, #(0x10 << PIXEL_BASE)
        orr    r9, r9, r10
        and    r10, r8, #(0x10 << (PIXEL_BASE + 6))
        orr    r9, r9, r10
        subs   r7, r7, #1
        bne    skip_psync_loop_no_old\@
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x01010101
        moveq   r11, #0
        // exit with r9 non-zero if burst detected, r11=red linem
.endm


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
        eorne  r8, r8, #(0x02 << PIXEL_BASE)
        eorne  r8, r8, #(0x02 << (PIXEL_BASE + 6))

        and    r9, r8, #(0x07 << PIXEL_BASE)
        and    r14, r8, #(0x07 << (PIXEL_BASE + 6))

        cmp    r9, #(0x07 << PIXEL_BASE)
        cmpne  r9, #(0x02 << PIXEL_BASE)
        orreq  r11, r11, #0x40000000

        cmp    r14, #(0x07 << (PIXEL_BASE + 6))
        cmpne  r14, #(0x02 << (PIXEL_BASE + 6))
        orreq  r11, r11, #0x80000000
.endm

.macro NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        mov    r11, r11, lsr #4

        eor    r9, r8, r8, lsr #3
        tst    r9, #(0x05 << PIXEL_BASE)
        tsteq  r9, #(0x05 << (PIXEL_BASE + 6))
        biceq  r3, r3, #BITDUP_LINE_CONDITION_DETECTED       //detect if any U/V content

        and    r9, r8, #(0x12 << PIXEL_BASE)
        and    r14, r8, #(0x12 << (PIXEL_BASE + 6))

        tst    r12, #NTSC_Y_INVERT
        eorne  r9, r9, #(0x12 << PIXEL_BASE)
        eorne  r14, r14, #(0x12 << (PIXEL_BASE + 6))

        cmp    r9, #(0x00 << PIXEL_BASE)
        orrne  r11, r11, #0x30000000
        cmp    r14, #(0x00 << (PIXEL_BASE + 6))
        orrne  r11, r11, #0xc0000000

.endm

.macro NTSC_CAPTURE_BITS_DOUBLE_8BPP_WIDE reg reg2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        eor    r9, r8, r8, lsr #3
        tst    r9, #(0x05 << PIXEL_BASE)
        tsteq  r9, #(0x05 << (PIXEL_BASE + 6))
        biceq  r3, r3, #BITDUP_LINE_CONDITION_DETECTED       //detect if any U/V content

        and    r9, r8, #(0x3f << PIXEL_BASE)
        and    r14, r8, #(0x3f << (PIXEL_BASE + 6))
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    r10, r10, r14, lsl #(16 - (PIXEL_BASE + 6))
        // Pixel double
        orr    \reg2, r10, r10, lsl #8
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
        SKIP_PSYNC_NO_OLD_CPLD_NTSC
        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE
        NTSC_CAPTURE_BITS_8BPP
        WAIT_FOR_PSYNC_EDGE
        NTSC_CAPTURE_BITS_8BPP
loop_8bpp3:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP           // input in r8
        NTSC_DECODE_CGA r5
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP           // input in r8
        NTSC_DECODE_CGA r6

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP           // input in r8
        NTSC_DECODE_CGA r7
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP           // input in r8
        NTSC_DECODE_CGA r10

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
        SKIP_PSYNC_NO_OLD_CPLD_NTSC
        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE
        NTSC_CAPTURE_BITS_8BPP_MONO
        WAIT_FOR_PSYNC_EDGE
        NTSC_CAPTURE_BITS_8BPP_MONO
loop_8bpp_mono3:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO      // input in r8
        NTSC_DECODE r5
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO      // input in r8
        NTSC_DECODE r6

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO      // input in r8
        NTSC_DECODE r7
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO      // input in r8
        NTSC_DECODE r10

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp_mono3
        pop     {r0, pc}

       .ltorg

no_ntsc_8bpp_mono:
        SKIP_PSYNC_NO_OLD_CPLD_NTSC
loop_8bppnoburst:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP_NORMAL r11 r5  // input in r8
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP_NORMAL r11 r6  // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP_NORMAL r11 r7  // input in r8
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP_NORMAL r11 r10 // input in r8

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
        SKIP_PSYNC_NO_OLD_CPLD_NTSC
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

preload_capture_line_ntsc_sixbits_8bpp_cga:
        PRELOAD_BITCOUNT
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_8bpp_cga

        .ltorg

        // *** 8 bit mono auto ***
.align 6
        b      preload_capture_line_ntsc_sixbits_8bpp_mono_auto
capture_line_ntsc_sixbits_8bpp_mono_auto:
        orr    r3, r3, #BITDUP_LINE_CONDITION_DETECTED         //detecting colour burst
        b      capture_line_ntsc_sixbits_8bpp_mono
preload_capture_line_ntsc_sixbits_8bpp_mono_auto:
        orr    r3, r3, #BITDUP_LINE_CONDITION_DETECTED         //detecting colour burst
        // falls into
        // *** 8 bit mono ***
        b       preload_capture_line_ntsc_sixbits_8bpp_mono
capture_line_ntsc_sixbits_8bpp_mono:
        push    {lr}
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        tst    r12, #NTSC_ARTIFACT
        beq    no_ntsc_sixbits_8bpp_mono
        SKIP_PSYNC_NO_OLD_CPLD_NTSC         // returns r9 != 0 if burst detected
        cmp    r9, #0
        biceq  r3, #BITDUP_LINE_CONDITION_DETECTED
        mov    r11, #0
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
loop_8bpp_mono6:
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
        bne     loop_8bpp_mono6
        pop     {r0, pc}

no_ntsc_sixbits_8bpp_mono:
        SKIP_PSYNC_NO_OLD_CPLD_NTSC                // returns with ntsc_status in r12
        cmp    r9, #0
        biceq  r3, #BITDUP_LINE_CONDITION_DETECTED
loop_8bpp_mono6_auto_noburst:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r6                // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE r10               // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp_mono6_auto_noburst

        pop     {r0, pc}
preload_capture_line_ntsc_sixbits_8bpp_mono:
        PRELOAD_BITCOUNT
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_8bpp_mono

        .ltorg

.align 6
        // *** 8 bit mono double auto ***
        b       preload_capture_line_ntsc_sixbits_double_8bpp_mono_auto
capture_line_ntsc_sixbits_double_8bpp_mono_auto:
        orr    r3, r3, #BITDUP_LINE_CONDITION_DETECTED         //detecting UV non monochrome
        b      capture_line_ntsc_sixbits_double_8bpp_mono
preload_capture_line_ntsc_sixbits_double_8bpp_mono_auto:
        orr    r3, r3, #BITDUP_LINE_CONDITION_DETECTED         //detecting UV non monochrome
        // falls into
        // *** 8 bit mono double***
        b      preload_capture_line_ntsc_sixbits_double_8bpp_mono
capture_line_ntsc_sixbits_double_8bpp_mono:
        push   {lr}
        ldr    r12, =ntsc_status
        ldr    r12, [r12]
        tst    r12, #NTSC_ARTIFACT
        beq    no_ntsc_sixbits_double_8bpp_mono
        SKIP_PSYNC_NO_OLD_CPLD_NTSC                // returns with ntsc_status in r12
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
        mov  r11, r11, lsl #1
        WAIT_FOR_PSYNC_EDGE                         // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE     // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r6
        mov  r11, r11, lsl #1

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE                          // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE      // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r7
        mov  r11, r11, lsl #1
        WAIT_FOR_PSYNC_EDGE                          // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_DOUBLE_8BPP_MONO_WIDE      // input in r8
        mov  r11, r11, lsr #1
        NTSC_DECODE r10
        mov  r11, r11, lsl #1

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bppnd_auto

        pop     {r0, pc}

preload_capture_line_ntsc_sixbits_double_8bpp_mono:
        PRELOAD_BITCOUNT
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_double_8bpp_mono

no_ntsc_sixbits_double_8bpp_mono:
        SKIP_PSYNC_NO_OLD_CPLD_NTSC                // returns with ntsc_status in r12
loop_8bppd:
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
        bne     loop_8bppd

        pop     {r0, pc}

        .ltorg