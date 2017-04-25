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
//
// Working registers:
//
//  r3 = field offsset
//  r4 = GPLEV0
//  r5 = line counter
//  r6 = pixel counter
//  r8 = value read from GPLEV0
//  r9 = extracted pixel
// r10 = block of 8 pixels, to be written to FB
// r11 = pointer into frame buffer (start of line)
// r12 = pointer into frame buffer (moves within line)
        
rgb_to_fb:

        push   {r4-r12, lr}

        mov    r3, r2
        ldr    r4, =GPLEV0

frame:
        // Set framebuffer address
#ifdef LINE_DOUBLE        
        mov    r11, r0
#else
        add    r11, r0, r3
        eor    r3, r3, r2
#endif
        
        // Wait for start of vsync
wait_for_vsync_a:
        ldr    r8, [r4]
        tst    r8, #VSYNC_MASK
        beq    wait_for_vsync_a
        // Check again in case of noise
        ldr    r8, [r4]
        tst    r8, #VSYNC_MASK
        beq    wait_for_vsync_a

        // Wait for end of vsync
wait_for_vsync_b:
        ldr    r8, [r4]
        tst    r8, #VSYNC_MASK
        bne    wait_for_vsync_b
        // Check again in case of noise
        ldr    r8, [r4]
        tst    r8, #VSYNC_MASK
        bne    wait_for_vsync_b

        // Skip inactive lines
        mov    r5, #28
loop0:

wait_for_hsync_a:
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        beq    wait_for_hsync_a
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        beq    wait_for_hsync_a

wait_for_hsync_b:
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        bne    wait_for_hsync_b
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        bne    wait_for_hsync_b

        subs   r5, r5, #1
        bne    loop0

        // Process active lines
        mov    r5, #256
loop1:

        // Wait for active high hsync
wait_for_hsync:
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        beq    wait_for_hsync
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        beq    wait_for_hsync

        // number of 8-bit characters per line
        mov    r6, r1
        // pointer into frame buffer
        mov    r12, r11
loop2:

        // Initialize 8 pixel block
        mov    r10, #0

        // Wait for 0-1 edge on PSYNC
wait_psync_01:
        ldr    r8, [r4]
        tst    r8, #PSYNC_MASK
        beq    wait_psync_01
        ldr    r8, [r4]
        tst    r8, #PSYNC_MASK
        beq    wait_psync_01

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
wait_psync_10:
        ldr    r8, [r4]
        tst    r8, #PSYNC_MASK
        bne    wait_psync_10
        ldr    r8, [r4]
        tst    r8, #PSYNC_MASK
        bne    wait_psync_10

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
        bne    loop2

        // Skip a whole line to maintain aspect ratio
        add    r11, r11, r2, lsl #1

        subs   r5, r5, #1
        bne    loop1

        b      frame

// Return

        pop    {r4-r12, lr}
        mov    pc, lr

