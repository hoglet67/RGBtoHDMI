#include "rpi-base.h"
#include "defs.h"

#define FIELD_TYPE_THRESHOLD 32768

.text
.global rgb_to_fb
.global measure_vsync

.macro POLL_VSYNC
#ifdef DOUBLE_BUFFER
        // Skip the double buffering in mode 7, probe or calibrate modes
        tst    r3, #(BIT_MODE7 | BIT_PROBE | BIT_CALIBRATE)
        bne    novsync\@
        // Poll for the VSYNC interrupt
        ldr    r0, =INTPEND2
        ldr    r0, [r0]
        tst    r0, #(1<<VSYNCINT)
        beq    novsync\@
        // Clear the VSYNC interrupt
        ldr    r0, =SMICTRL
        mov    r10, #0
        str    r10, [r0]
        // Flip the buffers
        eor    r3, r3, #BIT_BOTH_BUFFERS
        push   {r0-r3}
        tst    r3, #BIT_DISP_BUFFER
        moveq  r0, #0
        movne  r0, #1
        bl     swapBuffer
        pop    {r0-r3}
novsync\@:
#endif
.endm

.macro WAIT_FOR_CSYNC_0
wait\@:
        // Read the GPLEV0
        ldr    r8, [r4]
        tst    r8, #CSYNC_MASK
        bne    wait\@
        // Check again in case of noise
        ldr    r8, [r4]
        tst    r8, #CSYNC_MASK
        bne    wait\@
.endm

.macro WAIT_FOR_CSYNC_1
wait\@:
        // Read the GPLEV0
        ldr    r8, [r4]
        tst    r8, #CSYNC_MASK
        beq    wait\@
        // Check again in case of noise
        ldr    r8, [r4]
        tst    r8, #CSYNC_MASK
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

// Parameter registers:
//
//  r0 = frame buffer base
//  r1 = 8-pixel characters per line (normally 80, but 62.5 (63) in Mode 7
//  r2 = bytes per line
//  r3 = mode 7
//
rgb_to_fb:
        push   {r4-r12, lr}

        // Save the parameters so the registers can be reused
        str    r0, param_framebuffer

#ifdef DOUBLE_BUFFER
        ldr    r10, =SCREEN_HEIGHT
        mul    r10, r10, r2
        add    r0, r10
        str    r0, param_framebuffer2
        // Default to displaying buffer 0
        push   {r0-r3}
        mov    r0, #0
        bl     swapBuffer
        pop    {r0-r3}
#endif

        bl     init_timer_and_io

        tst    r3, #(BIT_PROBE | BIT_CALIBRATE)
        bleq   clear_screen

        // clear all the state bits apart from Mode7
        bic    r3, r3, #~(BIT_MODE7 | BIT_PROBE | BIT_CALIBRATE)

        // write to buffer 0, display buffer 1
        // initialize flag causes it to sync to an even frame
        orr    r3, r3, #(BIT_DISP_BUFFER | BIT_INITIALIZE)
frame:

        bl     wait_for_vsync

        // Working registers in the second half
        //
        //  r0 = scratch register
        //  r1 = 8-pixel characters per line (normally 80, but 62.5 (63) in Mode 7
        //  r2 = bytes per line
        //  r3 = field state
        //  r4 = GPLEV0
        //  r5 = line counter
        //  r6 = pixel counter
        //  r8 = value read from GPLEV0
        //  r9 = extracted pixel
        // r10 = block of 8 pixels, to be written to FB
        // r11 = pointer into frame buffer (start of line)
        // r12 = pointer into frame buffer (moves within line)

        // Set framebuffer address to the opposite of the buffer being displayed
#ifdef DOUBLE_BUFFER
        tst    r3, #BIT_DRAW_BUFFER
        ldreq  r11, param_framebuffer
        ldrne  r11, param_framebuffer2
#else
        ldr    r11, param_framebuffer
#endif

        // The odd vs even field can be distinguished by the time between
        // the last two rising edges:
        // odd field (first field) should be 21/23us
        // even field (second field) should be 53/55us
        sub    r6, r6, r7

        // Save the current field type
        cmp    r6, #FIELD_TYPE_THRESHOLD
        biclt  r3, r3, #BIT_FIELD_TYPE  // Odd, clear bit
        orrge  r3, r3, #BIT_FIELD_TYPE  // Even, set bit

        // If just starting, then sync to an even frame
        and    r0, r3, #(BIT_INITIALIZE | BIT_FIELD_TYPE)
        cmp    r0, #BIT_INITIALIZE
        beq    frame
        
        // Clear the initiaize flag so this is only done once
        bic    r3, r3, #BIT_INITIALIZE        

        // Check for mode change:
        // Odd: Mode 0..6 should be 21us, Mode 7 should be 23us
        // Even: Mode 0..6 should be 53us, Mode 7 should be 55us
        tst    r3, #BIT_FIELD_TYPE
        ldreq  r5, =22000     // Use 22us threshold in odd field
        ldrne  r5, =54000     // Use 54us threshold in even field
        cmp    r6, r5
        movlt  r0, #0         // Modes 0-6
        movge  r0, #1         // Mode 7
        tst    r3, #BIT_PROBE
        bne    exit
        tst    r3, #BIT_MODE7
        moveq  r5, #0         // Modes 0-6
        movne  r5, #1         // Mode 7
        cmp    r5, r0         // Check if we have changed mode
        bne    exit           // If not, then bail, as clock needs to be changed

        // Skip inactive lines
        mov    r5, #NUM_INACTIVE

        // Correct the relative positions of the odd and even frames
        // In Mode 0..6, reduce the number of active lines by one for the even frame
        // In Mode 7, increment the frame buffer pointer by one line for the even field
        tst    r3, #BIT_FIELD_TYPE
        beq    skip_line_loop
        tst    r3, #BIT_MODE7
        subeq  r5, r5, #1     // Modes 0-6
        addne  r11, r11, r2   // Mode 7

skip_line_loop:

        POLL_VSYNC

        WAIT_FOR_CSYNC_0
        WAIT_FOR_CSYNC_1

        subs   r5, r5, #1
        bne    skip_line_loop

        // Process active lines
        ldr    r5, =NUM_ACTIVE
process_line_loop:

        POLL_VSYNC

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

        // Line double always in Modes 0-6 regardless of interlace
        // On the multi core Pi this introduces stalling artefacts
#ifndef HAS_MULTICORE
        tst    r3, #BIT_MODE7
        streq  r10, [r12, r2]
#endif
        str    r10, [r12], #4

        subs   r6, r6, #1
        bne    process_chars_loop

        // Skip a whole line to maintain aspect ratio
        add    r11, r11, r2, lsl #1

        subs   r5, r5, #1
        bne    process_line_loop

        // Loop back if not calibrate mode...
        tst    r3, #BIT_CALIBRATE
        beq    frame

        // If mode 7, only exit if frame type is 0 (odd), so two complete frames are compared
        tst    r3, #BIT_MODE7 
        tstne  r3, #BIT_FIELD_TYPE
        bne    frame
        
// Return
exit:
        pop    {r4-r12, lr}
        mov    pc, lr


wait_for_vsync:
        // Wait for end of vsync
        //
        // Returns:
        // r5 = duration of last csync low pulse
        // r6 = time of last rising edge of csync
        // r7 = time of last-but-one rising edge of csync

        // Working registers in the first half
        //
        // r4 = GPLEV0
        // r5 = time of falling edge
        // r6 = time of rising edge
        // r7 = time of previous rising edge
        // r8 = value read from GPLEV0
        // r9 = state variable (1 = seen a long pulse

        // Initialize "seen long pulse" to false (0)
        mov    r9, #0

        // Wait for csync to be high
        WAIT_FOR_CSYNC_1

vsync_loop:
        // Wait for the falling edge of csync
        WAIT_FOR_CSYNC_0

        // Record time of the falling edge
#if defined(RPI2) || defined(RPI3)
        mrc    p15, 0, r5, c9, c13, 0
#else
        mrc    p15, 0, r5, c15, c12, 1
#endif
        // Wait for the rising edge of hsync
        WAIT_FOR_CSYNC_1

        // Save time of previous rising edge
        mov    r7, r6

        // Record time of the rising edge
#if defined(RPI2) || defined(RPI3)
        mrc    p15, 0, r6, c9, c13, 0
#else
        mrc    p15, 0, r6, c15, c12, 1
#endif

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

        mov    pc, lr

init_timer_and_io:
        // Enable the cycle counter, and run at the ARM clock rate
#if defined(RPI2) || defined(RPI3)
        mov    r4, #7
        mcr    p15, 0, r4, c9, c12, 0
        mov    r4, #(1 << 31)
        mcr    p15, 0, r4, c9, c12, 1
#else
        mov    r4, #7
        mcr    p15, 0, r4, c15, c12, 0
#endif

        // Setup R4 as a constant
        ldr    r4, =GPLEV0

        mov    pc, lr


measure_vsync:
        push    {r4-r12, lr}

        // Initialize the cycle counter, and set r4=GPLEV0
        bl     init_timer_and_io

        // wait for vsync, r6 contains the time of the subsequent hsync rising edge
        bl     wait_for_vsync
        mov    r0, r6

        // Wait for a first field of frame
        bl     wait_for_vsync

        // Record field type
        sub    r6, r6, r7
        cmp    r6, #FIELD_TYPE_THRESHOLD
        movlt  r3, #0         // Odd
        movge  r3, #1         // Even

        // Wait for a second field of frame
        bl     wait_for_vsync

        // Return the time for a complete frame (should be 40ms)
        sub    r0, r6, r0

        // Test for non-interlaced by looking for two successive fields of the same type
        sub    r6, r6, r7
        cmp    r6, #FIELD_TYPE_THRESHOLD
        eorlt  r3, r3, #1     // Odd
        tst    r3, #1
        // Set bit 31 of result if frame was interlaced
        orreq  r0, r0, #INTERLACED_FLAG

        pop    {r4-r12, lr}
        mov    pc, lr

clear_screen:
        ldr    r11, param_framebuffer
        mov    r7, #0
#ifdef DOUBLE_BUFFER
        ldr    r5, =SCREEN_HEIGHT * 2 // double the number of active lines
#else
        ldr    r5, =SCREEN_HEIGHT // double the number of active lines
#endif
clear_loop1:
        mov    r6, r2             // r2 is bytes per line
clear_loop2:
        str    r7, [r11], #4
        subs   r6, r6, #4
        bne    clear_loop2
        subs   r5, r5, #1
        bne    clear_loop1
        mov    pc, lr


param_framebuffer:
        .word  0

#ifdef DOUBLE_BUFFER
param_framebuffer2:
        .word  0
#endif
