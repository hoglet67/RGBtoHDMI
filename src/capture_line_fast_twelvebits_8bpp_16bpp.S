#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_fast_eightbits_8bpp
.global capture_line_fast_ninebitslo_16bpp
.global capture_line_fast_ninebitshi_16bpp
.global capture_line_fast_twelvebits_16bpp

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

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_fast_eightbits_8bpp
capture_line_fast_eightbits_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD_HIGH_LATENCY
        mov    r1, r1, lsr #3
        SETUP_EIGHT_BITS_MASK_R14
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_8BPP_0 r11             // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_8BPP_1                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_8BPP_2                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_8BPP_3 r5              // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_8BPP_0 r12             // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_8BPP_1                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_8BPP_2                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                  // expects GPLEV0 in r4, result in r8
        CAPTURE_EIGHT_BITS_8BPP_3 r6              // input in r8

        stmia   r0!, {r5, r6}
        subs    r1, r1, #1
        bne     loop_8bpp

        pop     {r0, pc}

preload_capture_line_fast_eightbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_eightbits_8bpp

        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_fast_twelvebits_16bpp
capture_line_fast_twelvebits_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11
        tst   r3, #BITDUP_ENABLE_FFOSD | BITDUP_ENABLE_GREY_DETECT
        bne   TEST_capture_line_fast_twelvebits_16bpp

        SKIP_PSYNC_NO_OLD_CPLD_HIGH_LATENCY
        mov    r1, r1, lsr #3
        SETUP_TWELVE_BITS_MASK_R14
loop_16bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_TWELVE_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     loop_16bpp

        pop     {r0, pc}

TEST_capture_line_fast_twelvebits_16bpp:
        tst   r3, #BIT_OSD | BITDUP_ENABLE_GREY_DETECT
        bne   OSD_capture_line_fast_twelvebits_16bpp

        SKIP_PSYNC_NO_OLD_CPLD_HIGH_LATENCY
        mov    r1, r1, lsr #3
        SETUP_TWELVE_BITS_MASK_R14
TEST_loop_16bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        TEST_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        TEST_CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        TEST_CAPTURE_TWELVE_BITS_16BPP_LO r11                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        TEST_CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        TEST_CAPTURE_TWELVE_BITS_16BPP_LO r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        TEST_CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        TEST_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        TEST_CAPTURE_TWELVE_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     TEST_loop_16bpp

        pop     {r0, pc}

OSD_capture_line_fast_twelvebits_16bpp:
        tst   r3, #BITDUP_ENABLE_GREY_DETECT
        orrne r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        SKIP_PSYNC_NO_OLD_CPLD_HIGH_LATENCY
        mov    r1, r1, lsr #3
        SETUP_TWELVE_BITS_MASK_R14
OSD_loop_16bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_CAPTURE_TWELVE_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_CAPTURE_TWELVE_BITS_16BPP_LO r11                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_CAPTURE_TWELVE_BITS_16BPP_HI r6                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_CAPTURE_TWELVE_BITS_16BPP_LO r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_CAPTURE_TWELVE_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_CAPTURE_TWELVE_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        OSD_CAPTURE_TWELVE_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     OSD_loop_16bpp

        pop     {r0, pc}

preload_capture_line_fast_twelvebits_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_twelvebits_16bpp


        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_fast_ninebitslo_16bpp
capture_line_fast_ninebitslo_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11
        SKIP_PSYNC_NO_OLD_CPLD_HIGH_LATENCY
        mov    r1, r1, lsr #3
        SETUP_NINELO_BITS_MASK_R14
loop_16lobpp:
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_LO r11                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_HI r6                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_LO r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINELO_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     loop_16lobpp

        pop     {r0, pc}

preload_capture_line_fast_ninebitslo_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_ninebitslo_16bpp


        .ltorg
        .align 6
        // *** 16 bit ***
        b       preload_capture_line_fast_ninebitshi_16bpp
capture_line_fast_ninebitshi_16bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11
        SKIP_PSYNC_NO_OLD_CPLD_HIGH_LATENCY
        mov    r1, r1, lsr #3
        SETUP_NINEHI_BITS_MASK_R14
loop_16hibpp:
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_HI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_LO r11                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_HI r6                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_LO r11                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_HI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_LO r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                       // expects GPLEV0 in r4, result in r8
        CAPTURE_NINEHI_BITS_16BPP_HI r10               // input in r8
        stmia   r0!, {r5, r6, r7, r10}

        subs    r1, r1, #1
        bne     loop_16hibpp

        pop     {r0, pc}

preload_capture_line_fast_ninebitshi_16bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_fast_ninebitshi_16bpp