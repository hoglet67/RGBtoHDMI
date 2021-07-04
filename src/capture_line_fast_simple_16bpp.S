#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_fast_simple_trailing_pos_16bpp
.global capture_line_fast_simple_leading_pos_16bpp
.global capture_line_fast_simple_trailing_neg_16bpp
.global capture_line_fast_simple_leading_neg_16bpp
.global capture_line_fast_simple_trailing_both_16bpp
.global capture_line_fast_simple_leading_both_16bpp

.global capture_line_default_simple_trailing_pos_16bpp
.global capture_line_default_simple_leading_pos_16bpp
.global capture_line_default_simple_trailing_neg_16bpp
.global capture_line_default_simple_leading_neg_16bpp
.global capture_line_default_simple_trailing_both_16bpp
.global capture_line_default_simple_leading_both_16bpp

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
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     loop_16bpp_simple\@

        mov     r0, r2
        pop     {pc}
.endm

.macro  OSD_COMMON_SIMPLE
loop_16bpp_osd_simple\@:
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
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
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
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
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_SIMPLE_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
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
        b       preload_capture_line_fast_simple_trailing_pos_16bpp
capture_line_fast_simple_trailing_pos_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        bic r3, r3, #PSYNC_MASK    // only +ve edge (inverted later)
        tst   r3, #BIT_OSD | BITDUP_ENABLE_GREY_DETECT
        bne   OSD_capture_line_fast_simple_trailing_pos_16bpp
        SKIP_PSYNC_SIMPLE_TRAILING_SINGLE_EDGE_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_trailing_pos_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_trailing_pos_16bpp
OSD_capture_line_fast_simple_trailing_pos_16bpp:
        SKIP_PSYNC_SIMPLE_TRAILING_SINGLE_EDGE_FAST
        OSD_COMMON_SIMPLE



        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_fast_simple_leading_pos_16bpp
capture_line_fast_simple_leading_pos_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        bic r3, r3, #PSYNC_MASK    // only +ve edge (inverted later)
        tst   r3, #BIT_OSD | BITDUP_ENABLE_GREY_DETECT
        bne   OSD_capture_line_fast_simple_leading_pos_16bpp
        SKIP_PSYNC_SIMPLE_LEADING_SINGLE_EDGE_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_leading_pos_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_leading_pos_16bpp
OSD_capture_line_fast_simple_leading_pos_16bpp:
        SKIP_PSYNC_SIMPLE_LEADING_SINGLE_EDGE_FAST
        OSD_COMMON_SIMPLE



        .ltorg
        .align 6
        // *** 16 bit *** *** this one used by amiga
        b       preload_capture_line_fast_simple_trailing_neg_16bpp
capture_line_fast_simple_trailing_neg_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        orr r3, r3, #PSYNC_MASK    // only -ve edge (inverted later)
        tst   r3, #BIT_OSD
        bne   OSD_capture_line_fast_simple_trailing_neg_16bpp
        SKIP_PSYNC_SIMPLE_TRAILING_SINGLE_EDGE_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_trailing_neg_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_trailing_neg_16bpp
OSD_capture_line_fast_simple_trailing_neg_16bpp:
        SKIP_PSYNC_SIMPLE_TRAILING_SINGLE_EDGE_FAST
        OSD_COMMON_SIMPLE


        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_fast_simple_leading_neg_16bpp
capture_line_fast_simple_leading_neg_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        orr r3, r3, #PSYNC_MASK    // only -ve edge (inverted later)
        tst   r3, #BIT_OSD
        bne OSD_capture_line_fast_simple_leading_neg_16bpp
        SKIP_PSYNC_SIMPLE_LEADING_SINGLE_EDGE_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_leading_neg_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_leading_neg_16bpp
OSD_capture_line_fast_simple_leading_neg_16bpp:
        SKIP_PSYNC_SIMPLE_LEADING_SINGLE_EDGE_FAST
        OSD_COMMON_SIMPLE



        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_fast_simple_trailing_both_16bpp
capture_line_fast_simple_trailing_both_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        tst   r3, #BIT_OSD
        bne OSD_capture_line_fast_simple_trailing_both_16bpp
        SKIP_PSYNC_SIMPLE_TRAILING_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_trailing_both_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_trailing_both_16bpp
OSD_capture_line_fast_simple_trailing_both_16bpp:
        SKIP_PSYNC_SIMPLE_TRAILING_FAST
        OSD_COMMON_SIMPLE


        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_fast_simple_leading_both_16bpp
capture_line_fast_simple_leading_both_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        tst   r3, #BIT_OSD
        bne OSD_capture_line_fast_simple_leading_both_16bpp
        SKIP_PSYNC_SIMPLE_LEADING_FAST
        COMMON_SIMPLE
preload_capture_line_fast_simple_leading_both_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_simple_leading_both_16bpp
OSD_capture_line_fast_simple_leading_both_16bpp:
        SKIP_PSYNC_SIMPLE_LEADING_FAST
        OSD_COMMON_SIMPLE

        //*********************************************

        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_default_simple_trailing_pos_16bpp
capture_line_default_simple_trailing_pos_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        mov   r10, r2
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        bic r3, r3, #PSYNC_MASK    // only +ve edge (inverted later)
        tst   r3, #BIT_OSD
        bne   OSD_capture_line_default_simple_trailing_pos_16bpp
        SKIP_PSYNC_SIMPLE_TRAILING_SINGLE_EDGE_FAST
        COMMON_DEFAULT
preload_capture_line_default_simple_trailing_pos_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_simple_trailing_pos_16bpp
OSD_capture_line_default_simple_trailing_pos_16bpp:
        SKIP_PSYNC_SIMPLE_TRAILING_SINGLE_EDGE_FAST
        OSD_COMMON_DEFAULT
        .ltorg


        .align 6
        // *** 16 bit ***
        b       preload_capture_line_default_simple_leading_pos_16bpp
capture_line_default_simple_leading_pos_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        mov   r10, r2
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        bic r3, r3, #PSYNC_MASK    // only +ve edge (inverted later)
        tst   r3, #BIT_OSD
        bne   OSD_capture_line_default_simple_leading_pos_16bpp
        SKIP_PSYNC_SIMPLE_LEADING_SINGLE_EDGE_FAST
        COMMON_DEFAULT
preload_capture_line_default_simple_leading_pos_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_simple_leading_pos_16bpp
OSD_capture_line_default_simple_leading_pos_16bpp:
        SKIP_PSYNC_SIMPLE_LEADING_SINGLE_EDGE_FAST
        OSD_COMMON_DEFAULT

        .ltorg
        .align 6
        // *** 16 bit *** this one used by amiga
        b       preload_capture_line_default_simple_trailing_neg_16bpp
capture_line_default_simple_trailing_neg_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        mov   r10, r2
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        orr r3, r3, #PSYNC_MASK    // only -ve edge (inverted later)
        tst   r3, #BIT_OSD
        bne   OSD_capture_line_default_simple_trailing_neg_16bpp
        SKIP_PSYNC_SIMPLE_TRAILING_SINGLE_EDGE_FAST
        COMMON_DEFAULT
preload_capture_line_default_simple_trailing_neg_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_simple_trailing_neg_16bpp
OSD_capture_line_default_simple_trailing_neg_16bpp:
        SKIP_PSYNC_SIMPLE_TRAILING_SINGLE_EDGE_FAST
        OSD_COMMON_DEFAULT


        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_default_simple_leading_neg_16bpp
capture_line_default_simple_leading_neg_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        mov   r10, r2
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        orr r3, r3, #PSYNC_MASK    // only -ve edge (inverted later)
        tst   r3, #BIT_OSD
        bne   OSD_capture_line_default_simple_leading_neg_16bpp
        SKIP_PSYNC_SIMPLE_LEADING_SINGLE_EDGE_FAST
        COMMON_DEFAULT
preload_capture_line_default_simple_leading_neg_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_simple_leading_neg_16bpp
OSD_capture_line_default_simple_leading_neg_16bpp:
        SKIP_PSYNC_SIMPLE_LEADING_SINGLE_EDGE_FAST
        OSD_COMMON_DEFAULT

        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_default_simple_trailing_both_16bpp
capture_line_default_simple_trailing_both_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        mov   r10, r2
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        tst   r3, #BIT_OSD
        bne   capture_line_default_simple_trailing_both_16bpp
        SKIP_PSYNC_SIMPLE_TRAILING_FAST
        COMMON_DEFAULT
preload_capture_line_default_simple_trailing_both_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_simple_trailing_both_16bpp
OSD_capture_line_default_simple_trailing_both_16bpp:
        SKIP_PSYNC_SIMPLE_TRAILING_FAST
        OSD_COMMON_DEFAULT


        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_default_simple_leading_both_16bpp
capture_line_default_simple_leading_both_16bpp:
        push    {lr}
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        mov   r10, r2
        SETUP_VSYNC_DEBUG_16BPP_R11
        SETUP_TWELVE_BITS_MASK_R14
        tst   r3, #BIT_OSD
        bne   OSD_capture_line_default_simple_leading_both_16bpp
        SKIP_PSYNC_SIMPLE_LEADING_FAST
        COMMON_DEFAULT
preload_capture_line_default_simple_leading_both_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_simple_leading_both_16bpp
OSD_capture_line_default_simple_leading_both_16bpp:
        SKIP_PSYNC_SIMPLE_LEADING_FAST
        OSD_COMMON_DEFAULT

