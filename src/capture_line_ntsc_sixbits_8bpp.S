#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.macro  SKIP_PSYNC_NO_OLD_CPLD_NTSC
        // only called if 6 bits/pixel in non-fast mode (old CPLDs v1 & v2 don't work at 6bpp so no need for test)
        WAIT_FOR_CSYNC_0_FAST_SKIP_HSYNC
        READ_CYCLE_COUNTER r10
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
        mov    r11, #0
skip_psync_loop_no_old\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        and    r12, r8, #(0x10 << PIXEL_BASE)
        orr    r11, r11, r12
        and    r12, r8, #(0x10 << (PIXEL_BASE + 6))
        orr    r11, r11, r12
        subs   r7, r7, #1
        bne    skip_psync_loop_no_old\@
.endm


.macro NTSC_CAPTURE_BITS_8BPP_MONO
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24
        mov    r11, r11, lsr #4
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

        and    r9, r8, #(0x07 << PIXEL_BASE)
        and    r14, r8, #(0x07 << (PIXEL_BASE + 6))

        cmp    r9, #(0x07 << PIXEL_BASE)
        cmpne  r9, #(0x02 << PIXEL_BASE)
        orreq  r11, r11, #0x40000000

        cmp    r14, #(0x07 << (PIXEL_BASE + 6))
        cmpne  r14, #(0x02 << (PIXEL_BASE + 6))
        orreq  r11, r11, #0x80000000
.endm

.macro  NO_BURST_NTSC_DECODE reg
        and    r8, r12, #3              //shift pixels by low 2 bits of NTSC phase
        eor    r8, r8, #3
        mov    r14, r11, lsr r8
        mov    \reg, #0
        tst    r14, #0x01000000
        orrne  \reg, \reg, #0x2f
        tst    r14, #0x02000000
        orrne  \reg, \reg, #0x2f00
        tst    r14, #0x04000000
        orrne  \reg, \reg, #0x2f0000
        tst    r14, #0x08000000
        orrne  \reg, \reg, #0x2f000000
.endm


.macro  NTSC_DECODE reg
        and    r8, r12, #3              //shift pixels by low 2 bits of NTSC phase
        eor    r8, r8, #3
        mov    r14, r11, lsr r8

        and    r9, r14, #0x0e000000
        and    r8, r14, #0x10000000
        mov    \reg, r9
        orr    \reg, \reg, r8, lsr #4
        and    r9, r14, #0x0f000000
        orr    \reg, \reg, r9, lsr #8
        and    r9, r14, #0x07000000
        and    r8, r14, #0x00800000
        orr    r9, r8, lsl #4
        orr    \reg, \reg, r9, lsr #16
        and    r9, r14, #0x03000000
        and    r8, r14, #0x00c00000
        orr    r9, r8, lsl #4
        orr    \reg, \reg, r9, lsr #24

        mov    r8, #0
        mov    r9, #0
        tst    r14, #0x00400000
        addne  r8, r8, #1
        tst    r14, #0x00800000
        addne  r9, r9, #1
        tst    r14, #0x01000000
        addne  r9, r9, #1
        tst    r14, #0x02000000
        addne  r9, r9, #1
        add    r8, r8, r9
        cmp    r8, #3
        movge  r8, #3
        subs   r8, r8, #1
        orrpl  \reg, \reg, r8, lsl #4

        tst    r14, #0x04000000
        addne  r9, r9, #1
        cmp    r9, #3
        movge  r9, #3
        subs   r9, r9, #1
        orrpl  \reg, \reg, r9, lsl #12

        mov    r8, #0
        mov    r9, #0
        tst    r14, #0x01000000
        addne  r8, r8, #1
        tst    r14, #0x02000000
        addne  r9, r9, #1
        tst    r14, #0x04000000
        addne  r9, r9, #1
        tst    r14, #0x08000000
        addne  r9, r9, #1
        add    r8, r8, r9
        cmp    r8, #3
        movge  r8, #3
        subs   r8, r8, #1
        orrpl  \reg, \reg, r8, lsl #20

        tst    r14, #0x10000000
        addne  r9, r9, #1
        cmp    r9, #3
        movge  r9, #3
        subs   r9, r9, #1
        orrpl  \reg, \reg, r9, lsl #28
.endm

.macro  NTSC_DECODE_CGA reg
        tst    r12, #0xf0000000
        bne    not_white\@
        NTSC_DECODE \reg
        b      done_white\@
not_white\@:
        and    r8, r12, #3              //shift pixels by low 2 bits of NTSC phase
        eor    r8, r8, #3
        mov    r14, r11, lsr r8

        and    r9, r14, #0x0e000000
        and    r8, r14, #0x10000000
        mov    \reg, r9
        orr    \reg, \reg, r8, lsr #4
        and    r9, r14, #0x0f000000
        orr    \reg, \reg, r9, lsr #8
        and    r9, r14, #0x07000000
        and    r8, r14, #0x00800000
        orr    r9, r8, lsl #4
        orr    \reg, \reg, r9, lsr #16
        and    r9, r14, #0x03000000
        and    r8, r14, #0x00c00000
        orr    r9, r8, lsl #4
        orr    \reg, \reg, r9, lsr #24

        and    r8, r12, #0xf0000000
        cmp    r8, #0x10000000
        orreq  \reg, \reg, #0x30000000
        orreq  \reg, \reg, #0x00300000
        orreq  \reg, \reg, #0x00003000
        orreq  \reg, \reg, #0x00000030

        cmp    r8, #0x20000000
        orreq  \reg, \reg, #0x40000000
        orreq  \reg, \reg, #0x00400000
        orreq  \reg, \reg, #0x00004000
        orreq  \reg, \reg, #0x00000040
done_white\@:
.endm

.macro NTSC_CAPTURE_BITS_8BPP
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24
        mov    r11, r11, lsr #4
        bic    r12, #0xf0000000

        and    r9, r8, #(0x07 << PIXEL_BASE)

        cmp    r9, #(0x01 << PIXEL_BASE) //red?
        cmpne  r9, #(0x03 << PIXEL_BASE) //yellow?
        orreq  r11, r11, #0x10000000
        cmpne  r9, #(0x02 << PIXEL_BASE) //green?
        orreq  r12, #0x10000000          //palette 0

        cmp    r9, #(0x05 << PIXEL_BASE) //magenta?
        orreq  r11, r11, #0x10000000
        cmpne  r9, #(0x06 << PIXEL_BASE) //cyan?
        orreq  r12, #0x20000000          //palette 1

        cmp    r9, #(0x07 << PIXEL_BASE) //white?
        orreq  r11, r11, #0x10000000


        and    r9, r8, #(0x07 << (PIXEL_BASE + 3))

        cmp    r9, #(0x02 << (PIXEL_BASE + 3)) //green?
        cmpne  r9, #(0x03 << (PIXEL_BASE + 3)) //yellow?
        orreq  r11, r11, #0x20000000
        cmpne  r9, #(0x01 << (PIXEL_BASE + 3)) //red?
        orreq  r12, #0x10000000                //palette 0

        cmp    r9, #(0x06 << (PIXEL_BASE + 3)) //cyan?
        orreq  r11, r11, #0x20000000
        cmpne  r9, #(0x05 << (PIXEL_BASE + 3)) //magenta?
        orreq  r12, #0x20000000                //palette 1

        cmp    r9, #(0x07 << (PIXEL_BASE + 3)) //white?
        orreq  r11, r11, #0x20000000


        and    r9, r8, #(0x07 << (PIXEL_BASE + 6))

        cmp    r9, #(0x01 << (PIXEL_BASE + 6)) //red?
        cmpne  r9, #(0x03 << (PIXEL_BASE + 6)) //yellow?
        orreq  r11, r11, #0x40000000
        cmpne  r9, #(0x02 << (PIXEL_BASE + 6)) //green?
        orreq  r12, #0x10000000                //palette 0

        cmp    r9, #(0x05 << (PIXEL_BASE + 6)) //magenta?
        orreq  r11, r11, #0x40000000
        cmpne  r9, #(0x06 << (PIXEL_BASE + 6)) //cyan?
        orreq  r12, #0x20000000                //palette 1

        cmp    r9, #(0x07 << (PIXEL_BASE + 6)) //white?
        orreq  r11, r11, #0x40000000


        and    r9, r8, #(0x07 << (PIXEL_BASE + 9))

        cmp    r9, #(0x02 << (PIXEL_BASE + 9)) //green?
        cmpne  r9, #(0x03 << (PIXEL_BASE + 9)) //yellow?
        orreq  r11, r11, #0x80000000
        cmpne  r9, #(0x01 << (PIXEL_BASE + 9)) //red?
        orreq  r12, #0x10000000                //palette 0

        cmp    r9, #(0x06 << (PIXEL_BASE + 9)) //cyan?
        orreq  r11, r11, #0x80000000
        cmpne  r9, #(0x05 << (PIXEL_BASE + 9)) //magenta?
        orreq  r12, #0x20000000                //palette 1

        cmp    r9, #(0x07 << (PIXEL_BASE + 9)) //white?
        orreq  r11, r11, #0x80000000

.endm


.macro  NTSC_CAPTURE_BITS_8BPP_WIDE
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        mov    r11, r11, lsr #2
        bic    r12, #0xf0000000

        and    r9, r8, #(0x07 << PIXEL_BASE)

        cmp    r9, #(0x01 << PIXEL_BASE) //red?
        cmpne  r9, #(0x03 << PIXEL_BASE) //yellow?
        orreq  r11, r11, #0x40000000
        cmpne  r9, #(0x02 << PIXEL_BASE) //green?
        orreq  r12, #0x10000000           //palette 0

        cmp    r9, #(0x05 << PIXEL_BASE) //magenta?
        orreq  r11, r11, #0x40000000
        cmpne  r9, #(0x06 << PIXEL_BASE) //cyan?
        orreq  r12, #0x20000000                   //palette 1

        cmp    r9, #(0x07 << PIXEL_BASE) //white?
        orreq  r11, r11, #0x40000000


        and    r9, r8, #(0x07 << (PIXEL_BASE + 6))

        cmp    r9, #(0x02 << (PIXEL_BASE + 6)) //green?
        cmpne  r9, #(0x03 << (PIXEL_BASE + 6)) //yellow?
        orreq  r11, r11, #0x80000000
        cmpne  r9, #(0x01 << (PIXEL_BASE + 6)) //red?
        orreq  r12, #0x10000000                   //palette 0

        cmp    r9, #(0x06 << (PIXEL_BASE + 6)) //cyan?
        orreq  r11, r11, #0x80000000
        cmpne  r9, #(0x05 << (PIXEL_BASE + 6)) //magenta?
        orreq  r12, #0x20000000                   //palette 1

        cmp    r9, #(0x07 << (PIXEL_BASE + 6)) //white?
        orreq  r11, r11, #0x80000000
.endm


.text

.global capture_line_ntsc_8bpp_mono
.global capture_line_ntsc_8bpp
.global capture_line_ntsc_sixbits_8bpp_mono
.global capture_line_ntsc_sixbits_8bpp_mono_auto
.global capture_line_ntsc_sixbits_8bpp

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
        b       preload_capture_line_ntsc_8bpp_mono
capture_line_ntsc_8bpp_mono:
        push    {lr}
        SKIP_PSYNC
        ldr    r12, =param_ntscphase
        push   {r14}
        mov    r11, #0
        ldr    r12, [r12]
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

.align 6
preload_capture_line_ntsc_8bpp_mono:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_8bpp_mono

        .ltorg

        // *** 8 bit mono ***

        b       preload_capture_line_ntsc_sixbits_8bpp_mono
capture_line_ntsc_sixbits_8bpp_mono:
        push    {lr}
        SKIP_PSYNC_NO_OLD_CPLD
        ldr    r12, =param_ntscphase
        push   {r14}
        mov    r11, #0
        ldr    r12, [r12]
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

preload_capture_line_ntsc_sixbits_8bpp_mono:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_8bpp_mono

        .ltorg
.align 6
        // *** 8 bit ***

        b       preload_capture_line_ntsc_8bpp
capture_line_ntsc_8bpp:
        push    {lr}
        SKIP_PSYNC
        ldr    r12, =param_ntscphase
        push   {r14}
        mov    r11, #0
        ldr    r12, [r12]
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


preload_capture_line_ntsc_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_8bpp


        .ltorg

.align 6
        b       preload_capture_line_ntsc_sixbits_8bpp
capture_line_ntsc_sixbits_8bpp:
        push    {lr}
        SKIP_PSYNC_NO_OLD_CPLD
        ldr    r12, =param_ntscphase
        push   {r14}
        mov    r11, #0
        ldr    r12, [r12]
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

preload_capture_line_ntsc_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_8bpp

        .ltorg


        // *** 8 bit mono auto ***

        b       preload_capture_line_ntsc_sixbits_8bpp_mono_auto
capture_line_ntsc_sixbits_8bpp_mono_auto:
        push    {lr}
        SKIP_PSYNC_NO_OLD_CPLD_NTSC
        ldr    r12, =param_ntscphase
        push   {r14}
        cmp    r11, #0
        mov    r11, #0
        ldr    r12, [r12]
        beq    no_ntsc_burst_detected
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

no_ntsc_burst_detected:
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
        WAIT_FOR_PSYNC_EDGE_FAST
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE
loop_8bpp_mono6_auto_noburst:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NO_BURST_NTSC_DECODE r5
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NO_BURST_NTSC_DECODE r6

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NO_BURST_NTSC_DECODE r7
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        NTSC_CAPTURE_BITS_8BPP_MONO_WIDE              // input in r8
        NO_BURST_NTSC_DECODE r10

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp_mono6_auto_noburst

        pop     {r0, pc}


preload_capture_line_ntsc_sixbits_8bpp_mono_auto:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_8bpp_mono_auto

        .ltorg
