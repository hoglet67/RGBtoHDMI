#include "rpi-base.h"
#include "defs.h"

// If LINE_DOUBLE is set, then each line in the field
// is written to two adjacent lines. This greatly
// improved the look of 50Hz games like Rocket Raid.
//
// At the moment, this only works with non-interlace
// video, as there is a bug in the CPLD that introduces
// line jitter with interlaced video.

// #define LINE_DOUBLE

.text
.global rgb_to_fb

// Parameter registers:
//
//  r0 = frame buffer base
//  r1 = 8-pixel characters per line (normally 80, but 62.5 (63) in Mode 7
//  r2 = bytes per line
//  r3 = unused
//

// Note, in this version we are using HSYNC which is the inverse of CSYNC

.macro WAIT_FOR_CSYNC_0
        WAIT_FOR_HSYNC_1
.endm

.macro WAIT_FOR_CSYNC_1
        WAIT_FOR_HSYNC_0
.endm

.macro WAIT_FOR_HSYNC_0
wait\@:
        // Read the GPLEV0
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        bne    wait\@
        // Check again in case of noise
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        bne    wait\@
.endm

.macro WAIT_FOR_HSYNC_1
wait\@:
        // Read the GPLEV0
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        beq    wait\@
        // Check again in case of noise
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        beq    wait\@
.endm

.macro WAIT_FOR_PSYNC_0
wait\@:
        // Read the GPLEV0
        ldr    r8, [r4]
        tst    r8, #PSYNC_MASK
        bne    wait\@
        // Check again in case of noise
        ldr    r8, [r4]
        tst    r8, #PSYNC_MASK
        bne    wait\@
.endm

.macro WAIT_FOR_PSYNC_1
wait\@:
        // Read the GPLEV0
        ldr    r8, [r4]
        tst    r8, #PSYNC_MASK
        beq    wait\@
        // Check again in case of noise
        ldr    r8, [r4]
        tst    r8, #PSYNC_MASK
        beq    wait\@
.endm

rgb_to_fb:
        push   {r4-r12, lr}
        ldr    r4, =GPLEV0

frame:
        // Wait for start of vsync

        // Working registers in the first half
        //
        // r4 = GPLEV0
        // r5 = time of falling edge
        // r6 = time of rising edge
        // r7 = time of previous rising edge
        // r8 = value read from GPLEV0
        // r9 = state variable (1 = seen a long pulse

        // Enable the cycle counter, and run at the ARM clock rate
        mov    r9, #7
        mcr    p15, 0, r9, c15, c12, 0

        // Initialize "seen long pulse" to false (0)
        mov    r9, #0

        // Wait for csync to be high
        WAIT_FOR_CSYNC_1

vsync_loop:
        // Wait for the falling edge of csync
        WAIT_FOR_CSYNC_0

        // Record time of the falling edge
        mrc    p15, 0, r5, c15, c12, 1

        // Wait for the rising edge of hsync
        WAIT_FOR_CSYNC_1

        // Save time of previous rising edge
        mov    r7, r6

        // Record time of the rising edge
        mrc    p15, 0, r6, c15, c12, 1

        // Calculate length of low hsync pulse (in ARM cycles = ns)
        sub    r5, r6, r5

        // Compare with 6us to descriminate short from long
        // - normal hsync pulses are 4us
        // - during vsync everything is either inverted, or clamped to zero
        // - this results in hsync pulses between 9us and 128us
        cmp    r5, #6144
        blt    seen_short

        // Set the state variable to indicate we are in the vsync
        mov    r9, #1

        // Loop back to wait for the next pulse
        b      vsync_loop

seen_short:
        // Test to see if we've seen any long pulses
        cmp    r9, #1
        // No, so look back for the next pulse
        bne    vsync_loop

        // Working registers in the second half
        //
        //  r4 = GPLEV0
        //  r5 = line counter
        //  r6 = pixel counter
        //  r8 = value read from GPLEV0
        //  r9 = extracted pixel
        // r10 = block of 8 pixels, to be written to FB
        // r11 = pointer into frame buffer (start of line)
        // r12 = pointer into frame buffer (moves within line)

        // Set framebuffer address
        mov    r11, r0

        // The odd vs even field can be distinguished by the time between
        // the last two rising edges:
        // odd field (first field) should be 23us
        // even field (second field) should be 51us
        sub    r6, r6, r7
        cmp    r6, #32768

        // For even field, increment the frame buffer pointer by one line
        addge  r11, r11, r2

        // Skip inactive lines
        mov    r5, #28
skip_line_loop:

        WAIT_FOR_CSYNC_0
        WAIT_FOR_CSYNC_1

        subs   r5, r5, #1
        bne    skip_line_loop

        // Process active lines
        mov    r5, #256
process_line_loop:

        // Wait for the start of hsync
        WAIT_FOR_CSYNC_0

        // number of 8-bit characters per line
        mov    r6, r1
        // pointer into frame buffer
        mov    r12, r11
process_chars_loop:

        // Initialize 8 pixel block
        mov    r10, #0

        // Wait for 0-1 edge on PSYNC
        WAIT_FOR_PSYNC_1

        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 in GPIO  7.. 5 ->  3.. 0
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 in GPIO 13..11 -> 11.. 8

        and    r9, r8, #(7 << PIXEL_BASE)
        orr    r10, r10, r9, lsl #2

        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        orr    r10, r10, r9, lsr #5

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        orr    r10, r10, r9, lsl #4

        and    r9, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsr #3

        // Wait for 1-0 edge on PSYNC
        WAIT_FOR_PSYNC_0

        // Pixel 4 in GPIO  4.. 2 -> 23..20
        // Pixel 5 in GPIO  7.. 5 -> 19..16
        // Pixel 6 in GPIO 10.. 8 -> 31..28
        // Pixel 7 in GPIO 13..11 -> 27..24

        and    r9, r8, #(7 << PIXEL_BASE)
        orr    r10, r10, r9, lsl #18

        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        orr    r10, r10, r9, lsl #11

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        orr    r10, r10, r9, lsl #20

        and    r9, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #13

#ifdef LINE_DOUBLE
        str    r10, [r12, r2]
#endif
        str    r10, [r12], #4

        subs   r6, r6, #1
        bne    process_chars_loop

        // Skip a whole line to maintain aspect ratio
        add    r11, r11, r2, lsl #1

        subs   r5, r5, #1
        bne    process_line_loop

        b      frame

// Return

        pop    {r4-r12, lr}
        mov    pc, lr

