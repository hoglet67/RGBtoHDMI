#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_fast_simple_16bpp
.global capture_line_default_simple_16bpp
.global capture_line_fast_simple_sixbits_8bpp
.global capture_line_fast_simple_ninebitshi_16bpp
.global capture_line_fast_simple_ninebitslo_16bpp
.global capture_line_fast_simple_ninebitslo_16bpp_blank


.macro SIMPLE_CAPTURE_LOW_BITS_8BPP_WIDE reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        tst    r8, #(0x800 << PIXEL_BASE)
        moveq  r10, \reg
        eorne  r10, \reg, #0x3f
        tst    r8, #(0x080 << PIXEL_BASE)
        eorne  r10, r10, #0x3f00
.endm

.macro SIMPLE_CAPTURE_HIGH_BITS_8BPP_WIDE reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24
        tst    r8, #(0x800 << PIXEL_BASE)
        eorne  r10, r10, #0x3f0000
        tst    r8, #(0x080 << PIXEL_BASE)
        eorne  \reg, r10, #0x3f000000
        moveq  \reg, r10
.endm

.macro BLANK_CAPTURE_NINELO_BITS_16BPP_LO reg
        // Pixel in GPIO 13.. 2 -> 15.. 0
        tst    r8, #(0x008 << PIXEL_BASE)
        moveq  r8, #0
        and    r9, r8, r14
        bic    r8, r8, r14, lsr #1
        eor    r10, \reg, r9, lsr #(PIXEL_BASE - 1)
        and    r8, r8, r14
        eor    r10, r10, r8, lsr #(PIXEL_BASE + 2)
.endm

.macro BLANK_CAPTURE_NINELO_BITS_16BPP_HI reg
        // Pixel in GPIO 13.. 2 -> 31.. 16
        tst    r8, #(0x008 << PIXEL_BASE)
        moveq  r8, #0
        and    r9, r8, r14
        bic    r8, r8, r14, lsr #1
        eor    r10, r10, r9, lsl #(16 - (PIXEL_BASE - 1))
        and    r8, r8, r14
        eor    \reg, r10, r8, lsl #(16 - (PIXEL_BASE + 2))
.endm

.macro SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO reg
        // Pixel in GPIO 13.. 2 -> 15.. 0
        and    r9, r8, r14
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        tst    r8, #MUX_MASK
        orrne  r3, #BITDUP_FFOSD_DETECTED
.endm

.macro SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI reg
        // Pixel in GPIO 13.. 2 -> 31.. 16
        and    r9, r8, r14
        eor    \reg, r10, r9, lsl #(16 - PIXEL_BASE)
.endm

.macro OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO reg
        and    r9, r8, r14
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    r9, r9, #(GREY_PIXELS & 0x0ff) << PIXEL_BASE
        eors   r9, r9, #(GREY_PIXELS & 0xf00) << PIXEL_BASE
        bicne  r3, #BITDUP_LINE_CONDITION_DETECTED
        tst    r8, #MUX_MASK
        orrne  r3, #BITDUP_FFOSD_DETECTED
        orrne  r10, #0xff00
        orrne  r10, #0x00ff
.endm

.macro OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI reg
        // Pixel in GPIO 13.. 2 -> 31.. 16
        and    r9, r8, r14
        eor    \reg, r10, r9, lsl #(16 - PIXEL_BASE)
        eor    r9, r9, #(GREY_PIXELS & 0x0ff) << PIXEL_BASE
        eors   r9, r9, #(GREY_PIXELS & 0xf00) << PIXEL_BASE
        bicne  r3, #BITDUP_LINE_CONDITION_DETECTED
        tst    r8, #MUX_MASK
        orrne  r3, #BITDUP_FFOSD_DETECTED
        orrne  \reg, \reg, #(0xff000000)
        orrne  \reg, \reg, #(0x00ff0000)
.endm


.macro  COMMON_SIMPLE
loop_16bpp_simple\@:
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     loop_16bpp_simple\@

        mov     r0, r2
        pop     {pc}
.endm

.macro  OSD_COMMON_SIMPLE
loop_16bpp_osd_simple\@:
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     loop_16bpp_osd_simple\@

        mov     r0, r2
        pop     {pc}
.endm


.macro  COMMON_DEFAULT
        push  {r2}
        mov   r2, r10
loop_16bpp_default\@:
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r10               // input in r8
        WRITE_R5_R6_R7_R10_16BPP

        subs    r1, r1, #1
        bne     loop_16bpp_default\@

        pop     {r0, pc}
.endm

.macro  OSD_COMMON_DEFAULT
        push  {r2}
        mov   r2, r10
loop_16bpp_osd_default\@:
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r10               // input in r8
        WRITE_R5_R6_R7_R10_16BPP

        subs    r1, r1, #1
        bne     loop_16bpp_osd_default\@

        pop     {r0, pc}
.endm

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
        // *** 16 bit ***
        b       preload_capture_line_fast_simple_16bpp
capture_line_fast_simple_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        tst   r3, #BIT_OSD | BITDUP_ENABLE_GREY_DETECT
        bne   OSD_capture_line_fast_simple_16bpp
        SKIP_PSYNC_SIMPLE_FAST
        mov    r1, r1, lsr #3
        COMMON_SIMPLE
preload_capture_line_fast_simple_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_16bpp
OSD_capture_line_fast_simple_16bpp:
        SKIP_PSYNC_SIMPLE_FAST
        mov    r1, r1, lsr #3
        OSD_COMMON_SIMPLE

        //*********************************************

        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_default_simple_16bpp
capture_line_default_simple_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        mov   r10, r2
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        tst   r3, #BIT_OSD
        bne   OSD_capture_line_default_simple_16bpp
        SKIP_PSYNC_SIMPLE_FAST
        mov    r1, r1, lsr #3
        COMMON_DEFAULT
preload_capture_line_default_simple_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_simple_16bpp
OSD_capture_line_default_simple_16bpp:
        SKIP_PSYNC_SIMPLE_FAST
        mov    r1, r1, lsr #3
        OSD_COMMON_DEFAULT
        .ltorg


        // *** 8 bit ***
        .align 6
        b       preload_capture_line_fast_sixbits_8bpp
capture_line_fast_simple_sixbits_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_SIMPLE_FAST
        mov    r1, r1, lsr #2
loop_8bpp:
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_HIGH_BITS_8BPP_WIDE r5                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_LOW_BITS_8BPP_WIDE r12                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_HIGH_BITS_8BPP_WIDE r6                // input in r8

        cmp     r1, #1
        stmeqia r0, {r5, r6}
        moveq   r0, r2
        popeq   {pc}

        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_LOW_BITS_8BPP_WIDE r11                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_HIGH_BITS_8BPP_WIDE r7                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_LOW_BITS_8BPP_WIDE r12                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_HIGH_BITS_8BPP_WIDE r10               // input in r8

        stmia   r0!, {r5, r6, r7, r10}
        subs    r1, r1, #2
        bne     loop_8bpp

        mov     r0, r2
        pop     {pc}

preload_capture_line_fast_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_sixbits_8bpp

        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_fast_ninebitslo_16bpp
capture_line_fast_simple_ninebitslo_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11
        SKIP_PSYNC_SIMPLE_FAST
        mov    r1, r1, lsr #3
        SETUP_NINELO_BITS_MASK_R14
loop_16lobpp:
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_HI r5                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_LO r11                 // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_HI r6                // input in r8

        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_LO r11                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_HI r7                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     loop_16lobpp

        mov     r0, r2
        pop     {pc}

preload_capture_line_fast_ninebitslo_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_ninebitslo_16bpp


        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_fast_ninebitslo_16bpp_blank
capture_line_fast_simple_ninebitslo_16bpp_blank:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11
        SKIP_PSYNC_SIMPLE_FAST
        mov    r1, r1, lsr #3
        SETUP_NINELO_BITS_MASK_R14
loop_16lobpp_blank:
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        BLANK_CAPTURE_NINELO_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        BLANK_CAPTURE_NINELO_BITS_16BPP_HI r5                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        BLANK_CAPTURE_NINELO_BITS_16BPP_LO r11                 // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        BLANK_CAPTURE_NINELO_BITS_16BPP_HI r6                // input in r8

        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        BLANK_CAPTURE_NINELO_BITS_16BPP_LO r11                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        BLANK_CAPTURE_NINELO_BITS_16BPP_HI r7                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        BLANK_CAPTURE_NINELO_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        BLANK_CAPTURE_NINELO_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     loop_16lobpp_blank

        mov     r0, r2
        pop     {pc}

preload_capture_line_fast_ninebitslo_16bpp_blank:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_ninebitslo_16bpp_blank


        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_fast_ninebitshi_16bpp
capture_line_fast_simple_ninebitshi_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11
        SKIP_PSYNC_SIMPLE_FAST
        mov    r1, r1, lsr #3
        SETUP_NINEHI_BITS_MASK_R14
loop_16hibpp:
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_HI r5                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_LO r11                 // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_HI r6                // input in r8

        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_LO r11                 // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_HI r7                // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_LO r11               // input in r8
        ACTUAL_WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     loop_16hibpp

        mov     r0, r2
        pop     {pc}

preload_capture_line_fast_ninebitshi_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_ninebitshi_16bpp

