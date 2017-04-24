#include "rpi-base.h"
#include "defs.h"

        
.text
.global rgb_to_fb
        
//  r0 = pointer to frame buffer
//  r1
//  r2
//  r3
//  r4 = GPLEV0
//  r5 = line counter
//  r6 = pixel counter
//  r8 = value read from GPLEV0
//  r9 = extracted pixel
// r10 = block of 8 pixels, to be written to FB 
        
rgb_to_fb:

        push    {r4-r12, lr}

loop0:
        mov     r1, r0
        
        ldr     r4, =GPLEV0
        mov     r5, #256
                
loop1:  
        
        // Wait for active high hsync
wait_for_hsync:
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        beq    wait_for_hsync

        // 80 8-bit characters per line
        mov    r6, #80
loop2:

        // Initialize 8 pixel block
        mov    r10, #0
        
        // Wait for 0-1 edge on PSYNC
wait_psync_01:
        ldr    r8, [r4]
        tst    r8, #PSYNC_MASK
        beq    wait_psync_01

        // Pixel 0 in GPIO  4.. 2 ->  3.. 0
        // Pixel 1 in GPIO  7.. 5 ->  7.. 4
        // Pixel 2 in GPIO 10.. 8 -> 11.. 8
        // Pixel 3 in GPIO 13..11 -> 15..12
        
        and    r9, r8, #(7 << PIXEL_BASE)
        orr    r10, r10, r9, lsr #2
        
        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        orr    r10, r10, r9, lsr #1

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        orr    r10, r10, r9

        and    r9, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #1

        // Wait for 1-0 edge on PSYNC
wait_psync_10:
        ldr    r8, [r4]
        tst    r8, #PSYNC_MASK
        bne    wait_psync_10

        // Pixel 4 in GPIO  4.. 2 -> 19..16
        // Pixel 5 in GPIO  7.. 5 -> 23..20
        // Pixel 6 in GPIO 10.. 8 -> 27..24
        // Pixel 7 in GPIO 13..11 -> 31..28
        
        and    r9, r8, #(7 << PIXEL_BASE)
        orr    r10, r10, r9, lsl #14
        
        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        orr    r10, r10, r9, lsl #15

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        orr    r10, r10, r9, lsl #16

        and    r9, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #17


        str    r10, [r1], #4
        
        
        subs    r6, r6, #1
        bne     loop2


        subs    r5, r5, #1
        bne     loop1


// Temporary hack - skip inactive lines
        ldr     r5, =(312-256)
                
loop3:  
        
wait_for_hsync_a:
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        beq    wait_for_hsync_a

wait_for_hsync_b:
        ldr    r8, [r4]
        tst    r8, #HSYNC_MASK
        bne    wait_for_hsync_b

        subs    r5, r5, #1
        bne     loop3


        b       loop0
        
// Return        
        
        pop     {r4-r12, lr}
        mov     pc, lr
        
