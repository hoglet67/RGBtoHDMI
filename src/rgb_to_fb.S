#include "rpi-base.h"
#include "defs.h"

#define FIELD_TYPE_THRESHOLD 32768

.text
.global rgb_to_fb
.global measure_vsync

.macro READ_CYCLE_COUNTER reg
#if defined(RPI2) || defined(RPI3)
        mrc    p15, 0, \reg, c9, c13, 0
#else
        mrc    p15, 0, \reg, c15, c12, 1
#endif
.endm

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

.macro PROCESS_CHARS_LOOP psync_polarity
process_chars_loop\@:

        // Initialize 8 pixel block
        mov    r10, #0

.if \psync_polarity == 1
        // Wait for 0-1 edge on PSYNC
        WAIT_FOR_PSYNC_1
.else
        // Wait for 1-0 edge on PSYNC
        WAIT_FOR_PSYNC_0
.endif

        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 in GPIO  7.. 5 ->  3.. 0
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 in GPIO 13..11 -> 11.. 8

        and    r9, r8, #(7 << PIXEL_BASE)
        orr    r10, r10, r9, lsl #(4 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        orr    r10, r10, r9, lsr #(3 + PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        orr    r10, r10, r9, lsl #(6 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsr #(1 + PIXEL_BASE)

.if \psync_polarity == 1
        // Wait for 1-0 edge on PSYNC
        WAIT_FOR_PSYNC_0
.else
        // Wait for 0-1 edge on PSYNC
        WAIT_FOR_PSYNC_1
.endif

        // Pixel 4 in GPIO  4.. 2 -> 23..20
        // Pixel 5 in GPIO  7.. 5 -> 19..16
        // Pixel 6 in GPIO 10.. 8 -> 31..28
        // Pixel 7 in GPIO 13..11 -> 27..24

        and    r9, r8, #(7 << PIXEL_BASE)
        orr    r10, r10, r9, lsl #(20 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        orr    r10, r10, r9, lsl #(13 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        orr    r10, r10, r9, lsl #(22 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #(15 - PIXEL_BASE)

        // Line double always in Modes 0-6 regardless of interlace
        // On the multi core Pi this introduces stalling artefacts
#ifndef HAS_MULTICORE
        tst    r3, #(BIT_MODE7 | BIT_SCANLINES)
        streq  r10, [r12, r2]
#endif
        tst    r3, #(BIT_MODE7)
        ldrne  r0, [r12]
        bicne  r0, #0x77000000
        bicne  r0, #0x00770000
        bicne  r0, #0x00007700
        bicne  r0, #0x00000077
        orrne  r10, r0
        str    r10, [r12], #4

        subs   r6, r6, #1
        bne    process_chars_loop\@
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

        // Setup R4 as a constant
        ldr    r4, =GPLEV0

        tst    r3, #BIT_CLEAR
        blne   clear_screen

        // clear all the state bits apart from the following:
        bic    r3, r3, #~(BIT_MODE7 | BIT_PROBE | BIT_CALIBRATE | BIT_CAL_COUNT | BIT_INITIALIZE | BIT_ELK | BIT_SCANLINES)

        // write to buffer 0, display buffer 1
        orr    r3, r3, #BIT_DISP_BUFFER
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
        tst    r3, #BIT_CALIBRATE
        bne    skip_switch_test
        tst    r8, #SW1_MASK  // Is the sw1 button pressed (active low)
        orreq  r0, #RET_SW1   // Indicate this in the result and then exit
        beq    exit
        tst    r8, #SW2_MASK  // Is the sw2 button pressed (active low)
        orreq  r0, #RET_SW2   // Indicate this in the result and then exit
        beq    exit
        tst    r8, #SW3_MASK  // Is the sw3 button pressed (active low)
        orreq  r0, #RET_SW3   // Indicate this in the result and then exit
        beq    exit
skip_switch_test:
        tst    r3, #BIT_MODE7
        moveq  r5, #0         // Modes 0-6
        movne  r5, #1         // Mode 7
        cmp    r5, r0         // Check if we have changed mode
        bne    exit           // If so, then bail, as the frame buffer needs to be resized

        // Save a copy of the frame buffer base
        push   {r11}

        // Skip inactive lines
        mov    r5, #NUM_INACTIVE

        // Correct the relative positions of the odd and even frames
        // In Mode 0..6, reduce the number of active lines by one for the even frame
        // In Mode 7, increment the frame buffer pointer by one line for the even field
        tst    r3, #BIT_ELK
        bne    skip_line_loop
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
        READ_CYCLE_COUNTER r10

        // Wait for the end of hsync
        WAIT_FOR_CSYNC_1
        READ_CYCLE_COUNTER r6

        // Calculate length of low hsync pulse (in ARM cycles = ns)
        sub    r10, r6, r10

        // number of 8-bit characters per line
        mov    r6, r1

        // pointer into frame buffer
        mov    r12, r11

        // Implement half character horizontal scrolling:
        // - a "short" hsync is 4.0us
        // - a "long"  hsync is 4.5us
        // - so threshold at about 4.25us, and for short hsync skip the first quad

        cmp    r10, #4224
        bgt    process_long_hsync

process_short_hsync:
        // Skip the first 0->1 edge of psync
        WAIT_FOR_PSYNC_1

        // Start the line on the 1->0 edge of psync
        PROCESS_CHARS_LOOP 0
        b      next_line

process_long_hsync:
        // Start the line on the 0->1 edge of psync
        PROCESS_CHARS_LOOP 1

next_line:
        // Skip a whole line to maintain aspect ratio
        add    r11, r11, r2, lsl #1

        subs   r5, r5, #1
        bne    process_line_loop

        // Update the OSD in Mode 0..6
        pop    {r11}
        tst    r3, #BIT_MODE7
        bne    skip_osd_update
        push   {r0-r12, lr}
        mov    r0, r11        // start of current draw buffer
        mov    r1, r2         // bytes per line
        bl     osd_update
        pop    {r0-r12, lr}
skip_osd_update:

        // Loop back if not calibrate mode...
        tst    r3, #BIT_CALIBRATE
        beq    frame

        // In calibrate mode, have the required number of fields (1 or 2) been captured
        tst    r3, #BIT_CAL_COUNT
        orr    r3, #BIT_CAL_COUNT
        beq    frame

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
        READ_CYCLE_COUNTER r5

        // Wait for the rising edge of hsync
        WAIT_FOR_CSYNC_1

        // Save time of previous rising edge
        mov    r7, r6

        // Record time of the rising edge
        READ_CYCLE_COUNTER r6

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

measure_vsync:
        push    {r4-r12, lr}

        // Setup R4 as a constant
        ldr    r4, =GPLEV0

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
