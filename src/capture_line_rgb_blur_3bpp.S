#include "rpi-base.h"
#include "defs.h"
#include "rgb_blur.h"

#include "macros.S"

// =============================================================================
// RGB Blur: 3-bit RGB capture into a 16bpp framebuffer with a 9-sample
// horizontal Gaussian blur (tables built by rgb_blur.c).
//
// Each psync read gives 4 pixels of 3 bits. For each colour bit we keep a
// 12-bit history (3 blocks of 4 samples, earliest sample in the top bit):
//    r7  = bit 0 history in bits 11..0, bit 1 history in bits 27..16
//    r9  = bit 2 history in bits 11..0 (bits above are don't-care)
// After reading block N we output block N-1, whose samples sit in history
// bits 7..4, so every output pixel sees 4 samples either side of it.
// The first read only primes the history and a final flush outputs the
// last block, so the picture lines up with the normal capture routines.
//
// Register use in the loop:
//    r0  framebuffer pointer          r1  psync reads left
//    r2  pitch                        r3  flags (psync polarity in bit 17)
//    r4  GPLEV0 / GPU data base       r5, r6  output words (2 pixels each)
//    r7  bit 0/1 history              r8  sample from WAIT, then scratch
//    r9  bit 2 history                r10 scratch
//    r11 alpha / marker / invert xor  r12 table base (= bit 1 table)
//    r14 0x1ff window mask
// =============================================================================

.text
.global capture_line_rgb_blur_3bpp
.global capture_line_rgb_blur_3bpp_table

// Byte offsets from r12, which points at the bit 1 (G) table
#define RB_OFF_LO   ((RGB_BLUR_TAB_G - RGB_BLUR_TAB_LO) * 4)     // subtracted
#define RB_OFF_HI   ((RGB_BLUR_TAB_G - RGB_BLUR_TAB_HI) * 4)     // subtracted
#define RB_OFF_R    ((RGB_BLUR_TAB_G - RGB_BLUR_TAB_R) * 4)      // subtracted
#define RB_OFF_B    ((RGB_BLUR_TAB_B - RGB_BLUR_TAB_G) * 4)      // added

// Append the 4 pixels in r8 to the histories (corrupts r8, r10).
// The deinterleave tables put bit 2 in bits 31..28, where merging it into r7
// does no harm: r7 bits 31..28 are never read and shift out next time.
.macro RGB_BLUR_GATHER
        and    r10, r8, #(0x3f << PIXEL_BASE)          // pixels 0,1 * 4
        and    r8, r8, #(0x3f << (PIXEL_BASE + 6))     // pixels 2,3 << 8
        add    r10, r12, r10
        add    r8, r12, r8, lsr #6
        ldr    r10, [r10, #-RB_OFF_LO]
        ldr    r8, [r8, #-RB_OFF_HI]
        orr    r8, r8, r10                             // bit0: 3..0, bit1: 19..16, bit2: 31..28
        orr    r7, r8, r7, lsl #4
        mov    r10, r8, lsr #28
        bic    r7, r7, #0x0000f000                     // keep bit 0 history out of bit 1's
        orr    r9, r10, r9, lsl #4
.endm

// Shift in an empty block (used to flush the last block)
.macro RGB_BLUR_GATHER_EMPTY
        mov    r7, r7, lsl #4
        bic    r7, r7, #0x0000f000
        mov    r9, r9, lsl #4
.endm

// Build r5 (pixels 0,1) and r6 (pixels 2,3) from the middle block.
// Pixel j uses windows (r7 >> (3-j)), (r7 >> (19-j)) and (r9 >> (3-j)).
// Loads are interleaved with the next pixel's index maths to avoid stalls.
.macro RGB_BLUR_OUTPUT
        // pixel 0 -> r5
        and    r10, r14, r7, lsr #3
        and    r8,  r14, r7, lsr #19
        add    r10, r12, r10, lsl #2
        ldr    r5,  [r12, r8, lsl #2]
        and    r8,  r14, r9, lsr #3
        ldr    r10, [r10, #-RB_OFF_R]
        add    r8,  r12, r8, lsl #2
        ldr    r8,  [r8, #RB_OFF_B]
        add    r5,  r5, r10
        // pixel 1 -> r5 high half (r6 is free as a temporary)
        and    r10, r14, r7, lsr #2
        add    r5,  r5, r8
        and    r8,  r14, r7, lsr #18
        add    r10, r12, r10, lsl #2
        ldr    r8,  [r12, r8, lsl #2]
        and    r6,  r14, r9, lsr #2
        ldr    r10, [r10, #-RB_OFF_R]
        add    r6,  r12, r6, lsl #2
        ldr    r6,  [r6, #RB_OFF_B]
        add    r10, r10, r8
        add    r5,  r5, r10, lsl #16
        add    r5,  r5, r6, lsl #16
        eor    r5,  r5, r11
        // pixel 2 -> r6
        and    r10, r14, r7, lsr #1
        and    r8,  r14, r7, lsr #17
        add    r10, r12, r10, lsl #2
        ldr    r6,  [r12, r8, lsl #2]
        and    r8,  r14, r9, lsr #1
        ldr    r10, [r10, #-RB_OFF_R]
        add    r8,  r12, r8, lsl #2
        ldr    r8,  [r8, #RB_OFF_B]
        add    r6,  r6, r10
        // pixel 3 -> r6 high half
        and    r10, r14, r7
        add    r6,  r6, r8
        and    r8,  r14, r7, lsr #16
        add    r10, r12, r10, lsl #2
        ldr    r8,  [r12, r8, lsl #2]
        ldr    r10, [r10, #-RB_OFF_R]
        add    r6,  r6, r8, lsl #16
        and    r8,  r14, r9
        add    r8,  r12, r8, lsl #2
        ldr    r8,  [r8, #RB_OFF_B]
        add    r6,  r6, r10, lsl #16
        add    r6,  r6, r8, lsl #16
        eor    r6,  r6, r11
.endm

// Store r5, r6 with line doubling and 16bpp scanlines (as WRITE_R5_R6_R7_R10_16BPP)
.macro RGB_BLUR_WRITE
        stmia   r0, {r5, r6}
        tst     r3, #BIT_NO_SCANLINES | BIT_INTERLACED_VIDEO
        ldreq   r8, =param_intensity
        ldreq   r8, [r8]
        eoreq   r5, r5, r8
        eoreq   r6, r6, r8
        tst     r3, #BIT_NO_LINE_DOUBLE
        subeq   r0, r0, r2
        stmeqia r0, {r5, r6}
        addeq   r0, r0, r2
        add     r0, r0, #8
.endm

        .align 6
        b       preload_capture_line_rgb_blur_3bpp
capture_line_rgb_blur_3bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_16BPP_R11
        SKIP_PSYNC
        ldr     r12, =rgb_blur_tables
        add     r12, r12, #(RGB_BLUR_TAB_G * 4)
        ldr     r14, =0x1ff
        mov     r7, #0
        mov     r9, #0

        // Prime the history with the first block (no output yet)
        WAIT_FOR_PSYNC_EDGE                    // expects GPLEV0 in r4, result in r8
        RGB_BLUR_GATHER
        subs    r1, r1, #1
        beq     rgb_blur_flush

rgb_blur_loop:
        WAIT_FOR_PSYNC_EDGE                    // expects GPLEV0 in r4, result in r8
        RGB_BLUR_GATHER
        RGB_BLUR_OUTPUT
        RGB_BLUR_WRITE
        subs    r1, r1, #1
        bne     rgb_blur_loop

rgb_blur_flush:
        RGB_BLUR_GATHER_EMPTY
        RGB_BLUR_OUTPUT
        RGB_BLUR_WRITE
        pop     {r0, pc}

preload_capture_line_rgb_blur_3bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_rgb_blur_3bpp

        .ltorg

// Dispatch table in the same shape as the other capture tables
// (palette control x bpp, double width, fast): every entry is this routine.
        .align 2
capture_line_rgb_blur_3bpp_table:
        .rept   (NUM_CONTROLS * 4 + 2)
        .word   capture_line_rgb_blur_3bpp
        .endr
