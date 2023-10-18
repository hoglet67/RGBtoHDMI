#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.global capture_line_c64lc_sixbits_8bpp
.global capture_line_c64lc_sixbits_double_8bpp
.global capture_line_c64lc_sixbits_double_16bpp
.global capture_line_c64yuv_sixbits_8bpp
.global capture_line_c64yuv_sixbits_double_8bpp
.global capture_line_c64yuv_sixbits_double_16bpp

.global c64_artifact_palette_16
.global c64_YUV_palette_lookup

.macro CAPTURE_0_BITS_8BPP_WIDE_C64LC reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000008                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000004                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000002                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000001                //bit 0 of luma
.endm

.macro CAPTURE_0_BITS_8BPP_WIDE_C64LC_0
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        mov    r10, #0
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000008                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000004                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000002                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000001                //bit 0 of luma
.endm

.macro CAPTURE_1_BITS_8BPP_WIDE_C64LC
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000800                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000400                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000200                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000100                //bit 0 of luma
.endm

.macro CAPTURE_2_BITS_8BPP_WIDE_C64LC
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x080000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x040000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x020000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x010000                //bit 0 of luma
.endm

.macro CAPTURE_3_BITS_8BPP_WIDE_C64LC reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x08000000              //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x04000000              //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x02000000              //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x01000000              //bit 0 of luma

        mov    \reg, r10
.endm


.macro CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000008                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000004                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000002                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000001                //bit 0 of luma

.endm

.macro CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_C64LC reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x080000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x040000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x020000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x010000                //bit 0 of luma

        orr    \reg, r10, r10, lsl #8
.endm

        .ltorg



.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_0
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8


        mov    r9, r9, lsl #4
        bic    r9, r9, #0x00000f00
        bic    r9, r9, #0x000f0000
        bic    r9, r9, #0x0f000000

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r9, r9, #0x00000008                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r9, r9, #0x00000004                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r9, r9, #0x00000002                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r9, r9, #0x00000001                //bit 0 of luma

        and     r5, r9, #0xff
        ldr     r5, [r10, r5, lsl #2]
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        eor     r5, r5, r11
        moveq   r5, r5, lsl #16
        movne   r5, r5, lsr #16
        orreq   r5, r5, r5, lsr #16
        orrne   r5, r5, r5, lsl #16

.endm

.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_1
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r9, r9, #0x00000800                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r9, r9, #0x00000400                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r9, r9, #0x00000200                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r9, r9, #0x00000100                //bit 0 of luma

        and     r8, r9, #0xff00
        ldr     r6, [r10, r8, lsr #6]
        eor     r6, r6, r11
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        moveq   r6, r6, lsl #16
        movne   r6, r6, lsr #16
        orreq   r6, r6, r6, lsr #16
        orrne   r6, r6, r6, lsl #16

.endm

.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r9, r9, #0x00080000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r9, r9, #0x00040000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r9, r9, #0x00020000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r9, r9, #0x00010000                //bit 0 of luma

        and     r8, r9, #0xff0000
        ldr     r7, [r10, r8, lsr #14]
        eor     r7, r7, r11
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        moveq   r7, r7, lsl #16
        movne   r7, r7, lsr #16
        orreq   r7, r7, r7, lsr #16
        orrne   r7, r7, r7, lsl #16

.endm

.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_3
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r9, r9, #0x08000000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r9, r9, #0x04000000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r9, r9, #0x02000000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r9, r9, #0x01000000                //bit 0 of luma
        str     r9, [r14], #4

        and     r8, r9, #0xff000000
        ldr     r10, [r10, r8, lsr #22]
        eor     r10, r10, r11
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        moveq   r10, r10, lsl #16
        movne   r10, r10, lsr #16
        orreq   r10, r10, r10, lsr #16
        orrne   r10, r10, r10, lsl #16
.endm



        // *** 16 bit ***
        .align 6
        b       preload_capture_line_c64lc_sixbits_double_16bpp
capture_line_c64lc_sixbits_double_16bpp:
        push    {lr}
        tst    r6, #1  //CHECK ODD/EVEN LINE
        biceq  r3, r3, #BITDUP_LINE_CONDITION_DETECTED //use spare flag
        orrne  r3, r3, #BITDUP_LINE_CONDITION_DETECTED

        mov    r1, r1, lsl #1
        SETUP_VSYNC_DEBUG_16BPP_R11

        SKIP_PSYNC_NO_OLD_CPLD
        adrl   r14, bufferc64
        mov    r1, r1, lsr #2

loop_16bpplc:
        ldr    r9, [r14]
        adrl r10, c64_artifact_palette_16

        WAIT_FOR_PSYNC_EDGE_FAST        // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST        // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_1                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_2                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST        // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_3                 // input in r8


        WRITE_R5_R6_R7_R10_16BPP

        subs    r1, r1, #1
        bne     loop_16bpplc
        bic     r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        pop     {r0, pc}

preload_capture_line_c64lc_sixbits_double_16bpp:
        ldr    r0, =c64_artifact_palette_16
        mov    r1, #256
preload16lc_loop:
        ldr    r2, [r0], #4
        subs   r1, r1, #1
        bne    preload16lc_loop
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_sixbits_double_16bpp

        .ltorg

        .align 6
c64_artifact_palette_16:
        .space (256*4), 0
c64_YUV_palette_lookup:
        .space (64), 0
bufferc64:
        .space 2048, 0

        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_c64lc_sixbits_8bpp
capture_line_c64lc_sixbits_8bpp:
        push    {lr}
        mov    r1, r1, lsl #1
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #3
loop6_8bpp_c64lc:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_C64LC r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_C64LC r5               // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_C64LC_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_C64LC r6               // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_C64LC_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_C64LC r7               // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_C64LC r12            // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_C64LC                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_C64LC                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_C64LC r10            // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6_8bpp_c64lc

        pop     {r0, pc}


preload_capture_line_c64lc_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_c64lc_sixbits_8bpp


        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_c64lc_sixbits_double_8bpp
capture_line_c64lc_sixbits_double_8bpp:
        push    {lr}
        mov    r1, r1, lsl #1
        SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #2
loop6d_8bpp_c64lc:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_C64LC r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_C64LC r6                // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_C64LC r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_C64LC r10               // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6d_8bpp_c64lc

        pop     {r0, pc}


preload_capture_line_c64lc_sixbits_double_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_c64lc_sixbits_double_8bpp




.macro CAPTURE_LOW_BITS_8BPP_WIDE_YUV reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        and    r9, r8, #(0x3f << PIXEL_BASE)
        ldrb   r9, [r14, r9, lsr #PIXEL_BASE]
        eor    r10, \reg, r9

        and    r9, r8, #(0x3f << (PIXEL_BASE + 6))
        ldrb   r9, [r14, r9, lsr #(PIXEL_BASE + 6)]
        eor    r10, r10, r9, lsl #8
.endm


.macro CAPTURE_HIGH_BITS_8BPP_WIDE_YUV reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        and    r9, r8, #(0x3f << PIXEL_BASE)
        ldrb   r9, [r14, r9, lsr #PIXEL_BASE]
        eor    r10, r10, r9, lsl #16

        and    r9, r8, #(0x3f << (PIXEL_BASE + 6))
        ldrb   r9, [r14, r9, lsr #(PIXEL_BASE + 6)]
        eor    \reg, r10, r9, lsl #24
.endm


        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_c64yuv_sixbits_8bpp
capture_line_c64yuv_sixbits_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD
        adrl  r14, c64_YUV_palette_lookup
        mov    r1, r1, lsr #2
loop6_8bpp_c64yuv:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE_YUV r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE_YUV r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE_YUV r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE_YUV r6                // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE_YUV r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE_YUV r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_WIDE_YUV r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_WIDE_YUV r10               // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6_8bpp_c64yuv

        pop     {r0, pc}

preload_capture_line_c64yuv_sixbits_8bpp:
        ldr    r0, =c64_YUV_palette_lookup
        mov    r1, #16
preload16yuv_sixbits_loop:
        ldr    r2, [r0], #4
        subs   r1, r1, #1
        bne    preload16yuv_sixbits_loop
        SETUP_DUMMY_PARAMETERS
        b       capture_line_c64yuv_sixbits_8bpp


.macro CAPTURE_BITS_DOUBLE_8BPP_WIDE_YUV reg reg2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        and    r9, r8, #(0x3f << PIXEL_BASE)
        ldrb   r9, [r14, r9, lsr #PIXEL_BASE]
        eor    r10, \reg, r9

        and    r9, r8, #(0x3f << (PIXEL_BASE + 6))
        ldrb   r9, [r14, r9, lsr #(PIXEL_BASE + 6)]
        eor    r10, r10, r9, lsl #16
        // Pixel double
        orr    \reg2, r10, r10, lsl #8
.endm

        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_c64yuv_sixbits_double_8bpp
capture_line_c64yuv_sixbits_double_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        SKIP_PSYNC_NO_OLD_CPLD
        adrl  r14, c64_YUV_palette_lookup
        mov    r1, r1, lsr #1
loop6d_8bpp_c64yuv:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_WIDE_YUV r11 r5          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_WIDE_YUV r12 r6          // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_WIDE_YUV r11 r7          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_WIDE_YUV r12 r10         // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6d_8bpp_c64yuv

        pop     {r0, pc}


preload_capture_line_c64yuv_sixbits_double_8bpp:
        ldr    r0, =c64_YUV_palette_lookup
        mov    r1, #16
preload16yuv_sixbits_double_loop:
        ldr    r2, [r0], #4
        subs   r1, r1, #1
        bne    preload16yuv_sixbits_double_loop
        SETUP_DUMMY_PARAMETERS
        b       capture_line_c64yuv_sixbits_double_8bpp





.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64YUV_0
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        mov     r9, r9, lsl #4
        bic     r9, r9, #0x00000f00
        bic     r9, r9, #0x000f0000
        bic     r9, r9, #0x0f000000

        add     r10, #(c64_YUV_palette_lookup - c64_artifact_palette_16)
        and     r5, r8, #(0x3f << PIXEL_BASE)
        ldrb    r5, [r10, r5, lsr #PIXEL_BASE]
        orr     r9, r9, r5
        sub     r10, #(c64_YUV_palette_lookup - c64_artifact_palette_16)

        and     r5, r9, #0xff
        ldr     r5, [r10, r5, lsl #2]
        eor     r5, r5, r11
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        moveq   r5, r5, lsl #16
        movne   r5, r5, lsr #16
        orreq   r5, r5, r5, lsr #16
        orrne   r5, r5, r5, lsl #16

.endm

.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64YUV_1
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        add     r10, #(c64_YUV_palette_lookup - c64_artifact_palette_16)
        and     r6, r8, #(0x3f << (PIXEL_BASE + 6))
        ldrb    r6, [r10, r6, lsr #(PIXEL_BASE + 6)]
        orr     r9, r9, r6, lsl #8
        sub     r10, #(c64_YUV_palette_lookup - c64_artifact_palette_16)

        and     r6, r9, #0xff00
        ldr     r6, [r10, r6, lsr #6]
        eor     r6, r6, r11
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        moveq   r6, r6, lsl #16
        movne   r6, r6, lsr #16
        orreq   r6, r6, r6, lsr #16
        orrne   r6, r6, r6, lsl #16

.endm

.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64YUV_2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        add     r10, #(c64_YUV_palette_lookup - c64_artifact_palette_16)
        and     r7, r8, #(0x3f << PIXEL_BASE)
        ldrb    r7, [r10, r7, lsr #PIXEL_BASE]
        orr     r9, r9, r7, lsl #16
        sub     r10, #(c64_YUV_palette_lookup - c64_artifact_palette_16)

        and     r7, r9, #0xff0000
        ldr     r7, [r10, r7, lsr #14]
        eor     r7, r7, r11
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        moveq   r7, r7, lsl #16
        movne   r7, r7, lsr #16
        orreq   r7, r7, r7, lsr #16
        orrne   r7, r7, r7, lsl #16

.endm

.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64YUV_3
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        add     r10, #(c64_YUV_palette_lookup - c64_artifact_palette_16)
        and     r8, r8, #(0x3f << (PIXEL_BASE + 6))
        ldrb    r8, [r10, r8, lsr #(PIXEL_BASE + 6)]
        orr     r9, r9, r8, lsl #24
        sub     r10, #(c64_YUV_palette_lookup - c64_artifact_palette_16)

        str     r9, [r14], #4

        and     r8, r9, #0xff000000
        ldr     r10, [r10, r8, lsr #22]
        eor     r10, r10, r11
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        moveq   r10, r10, lsl #16
        movne   r10, r10, lsr #16
        orreq   r10, r10, r10, lsr #16
        orrne   r10, r10, r10, lsl #16
.endm


        b       preload_capture_line_c64yuv_sixbits_double_16bpp
capture_line_c64yuv_sixbits_double_16bpp:
        push    {lr}
        tst    r6, #1  //CHECK ODD/EVEN LINE
        biceq  r3, r3, #BITDUP_LINE_CONDITION_DETECTED //use spare flag
        orrne  r3, r3, #BITDUP_LINE_CONDITION_DETECTED


        SETUP_VSYNC_DEBUG_16BPP_R11
        SKIP_PSYNC_NO_OLD_CPLD

        adrl   r14, bufferc64
        mov    r1, r1, lsr #1

loop_16bppyuv:
        ldr    r9, [r14]
        adrl   r10, c64_artifact_palette_16

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64YUV_0                // input in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64YUV_1                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64YUV_2                 // input in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64YUV_3                 // input in r8

        WRITE_R5_R6_R7_R10_16BPP

        subs    r1, r1, #1
        bne     loop_16bppyuv
        bic     r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        pop     {r0, pc}

preload_capture_line_c64yuv_sixbits_double_16bpp:
        ldr    r0, =c64_artifact_palette_16
        mov    r1, #256 + 16
preload16_loop_c64yuv_sixbits_double_16bpp:
        ldr    r2, [r0], #4
        subs   r1, r1, #1
        bne    preload16_loop_c64yuv_sixbits_double_16bpp
        SETUP_DUMMY_PARAMETERS
        b       capture_line_c64yuv_sixbits_double_16bpp
