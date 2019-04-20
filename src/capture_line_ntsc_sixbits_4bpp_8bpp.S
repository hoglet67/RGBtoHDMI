#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.macro YCAPTURE_LOW_BITS_8BPP_WIDE reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        mov    r10, #0
        and    r9, r8, #(0x07 << PIXEL_BASE)
        cmp    r9, # (0x06 << PIXEL_BASE)
        orreq  r10, r10, #0x01
        cmp    r9, # (0x05 << PIXEL_BASE)
        orreq  r10, r10, #0x02
        cmp    r9, # (0x07 << PIXEL_BASE)
        orreq  r10, r10, #0x03
.endm


.macro YCAPTURE_HIGH_BITS_8BPP_WIDE reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        and    r9, r8, #(0x07 << PIXEL_BASE)
        cmp    r9, # (0x06 << PIXEL_BASE)
        orreq  r10, r10, #0x04
        cmp    r9, # (0x05 << PIXEL_BASE)
        orreq  r10, r10, #0x05
        cmp    r9, # (0x07 << PIXEL_BASE)
        orreq  r10, r10, #0x0c
        orr    r10, r10, #0x40
        orr    \reg, r10, r10, lsl#8
        orr    \reg, \reg, \reg, lsl#16
.endm

.macro  XCAPTURE_LOW_BITS_8BPP_WIDE reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        mov    r10, #0
        and    r9, r8, #(0x3f << PIXEL_BASE)
        and    r14, r8, #(0x3f << (PIXEL_BASE + 6))
        cmp    r9, #0
        orrne  r10, r10, #8
        cmp    r14, #0
        orrne  r10, r10, #4
.endm


.macro XCAPTURE_HIGH_BITS_8BPP_WIDE reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        and    r9, r8, #(0x3f << PIXEL_BASE)
        and    r14, r8, #(0x3f << (PIXEL_BASE + 6))
        cmp    r9, #0
        orrne  r10, r10, #2
        cmp    r14, #0
        orrne  r10, r10, #1
        orr    r10, r10, #0x40
        orr    \reg, r10, r10, lsl#8
        orr    \reg, \reg, \reg, lsl#16
.endm

.text

.global capture_line_ntsc_sixbits_4bpp
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

        b       preload_capture_line_ntsc_sixbits
capture_line_ntsc_sixbits_4bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11
        SKIP_PSYNC_NO_OLD_CPLD
        push    {r14}
loop:
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_WIDE r11          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_WIDE r7           // input in r8

        WRITE_R7_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_WIDE r11          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_WIDE              // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST         // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_WIDE r10          // input in r8

        WRITE_R7_R10

        subs    r1, r1, #2
        bne     loop

        pop     {r0, pc}


preload_capture_line_ntsc_sixbits:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_4bpp

        .ltorg

        // *** 8 bit ***

        b       preload_capture_line_ntsc_sixbits_8bpp
capture_line_ntsc_sixbits_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12

        ldr     r11, =0x10101010
        ldr     r12, =0x10101010

        SKIP_PSYNC_NO_OLD_CPLD
        push    {r14}
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        XCAPTURE_LOW_BITS_8BPP_WIDE r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        XCAPTURE_HIGH_BITS_8BPP_WIDE r5               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        XCAPTURE_LOW_BITS_8BPP_WIDE r12               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        XCAPTURE_HIGH_BITS_8BPP_WIDE r6               // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        XCAPTURE_LOW_BITS_8BPP_WIDE r11               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        XCAPTURE_HIGH_BITS_8BPP_WIDE r7               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        XCAPTURE_LOW_BITS_8BPP_WIDE r12               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        XCAPTURE_HIGH_BITS_8BPP_WIDE r10              // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp

        pop     {r0, pc}

preload_capture_line_ntsc_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_ntsc_sixbits_8bpp
