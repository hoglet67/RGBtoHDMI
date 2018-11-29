#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

#define FIELD_TYPE_THRESHOLD 32768

.text
.global rgb_to_fb
.global measure_vsync
.global sw1counter
.global sw2counter
.global sw3counter
.global vsync_line


// ======================================================================
// Macros
// ======================================================================

// Data Synchronisation Barrier
.macro DSB
        mcr    p15, 0, r0, c7, c10, 4
.endm

// Data Memory Barrier
.macro DMB
        mcr    p15, 0, r0, c7, c10, 5
.endm

.macro READ_CYCLE_COUNTER reg
#if defined(RPI2) || defined(RPI3)
        mrc    p15, 0, \reg, c9, c13, 0
#else
        mrc    p15, 0, \reg, c15, c12, 1
#endif
.endm

.macro CLEAR_VSYNC
        // Clear the VSYNC interrupt
        ldr    r0, =SMICTRL
        bic    r3, r3, #BIT_VSYNC_MARKER
        mov    r7, #0
        str    r7, [r0]
        // Don't proceed until this write is complete
        DSB
.endm

.macro SHOW_VSYNC
        bic    r3, r3, #BIT_VSYNC_MARKER
        tst    r3, #(BIT_VSYNC)
        beq    novsync\@
        tst    r3, #(BIT_PROBE)
        bne    novsync\@
        // Poll for the VSYNC interrupt
        ldr    r0, =INTPEND2
        ldr    r0, [r0]
        tst    r0, #(1<<VSYNCINT)
        beq    novsync\@
        // Clear the VSYNC interrupt
        CLEAR_VSYNC
        // Mark the next line in red
        orr     r3, r3, #BIT_VSYNC_MARKER
        str     r5, vsync_line
novsync\@:
.endm

#ifdef MULTI_BUFFER
.macro FLIP_BUFFER
        // Skip the multi buffering in mode 7 and probe mode
        tst    r3, #(BIT_MODE7 | BIT_PROBE)
        bne    noflip\@
        // Flip to the last completed draw buffer
        // It seems the GPU delays this until the next vsync
        push   {r0-r3}
        mov    r14, r3, lsr #OFFSET_LAST_BUFFER
        and    r0, r14, #3
        bl     swapBuffer
        pop    {r0-r3}
noflip\@:
.endm
#endif

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

.macro KEY_PRESS_DETECT mask, ret, counter
        ldr    r5, \counter    // Load the counter value
        tst    r8, #\mask      // Is the button pressed (active low)?
        movne  r5, #0          // Clear the counter
        addeq  r5, r5, #1      // If pressed, then increment the counter valye
        str    r5, \counter    // And always write back the counter value

        cmp    r5, #1          // Counter goes from 0->1 when key initially
        orreq  r0, #\ret       // Indicate the initial press in the result

        cmp    r5, #32         // 32 = auto repeat delay
        tstge  r5, #7          // 7  = auto repeat rate
        orreq  r0, #\ret       // Indicate the auto repeated press in the result

        cmp    r5, #128        // 128 = auto repeat delay
        tstge  r5, #3          // 3 = auto repeat rate
        orreq  r0, #\ret       // Indicate the auto repeated press in the result

        cmp    r5, #256        // 256 = auto repeat delay
        tstge  r5, #1          // 1 = auto repeat rate
        orreq  r0, #\ret       // Indicate the auto repeated press in the result
.endm




rgb_to_fb:

        push   {r4-r12, lr}

        // Save the capture_info_t parameters to absolute addresses
        ldr    r2, [r0, #O_FB_PITCH]
        str    r2, param_fb_pitch
        ldr    r2, [r0, #O_FB_WIDTH]
        str    r2, param_fb_width
        ldr    r2, [r0, #O_FB_HEIGHT]
        str    r2, param_fb_height
        ldr    r2, [r0, #O_FB_BPP]
        str    r2, param_fb_bpp
        ldr    r2, [r0, #O_CHARS_PER_LINE]
        str    r2, param_chars_per_line
        ldr    r2, [r0, #O_NLINES]
        str    r2, param_nlines
        ldr    r2, [r0, #O_H_OFFSET]
        str    r2, param_h_offset
        ldr    r2, [r0, #O_V_OFFSET]
        str    r2, param_v_offset
        ldr    r2, [r0, #O_NCAPTURE]
        str    r2, param_ncapture
        ldr    r2, [r0, #O_CAPTURE_LINE]
        str    r2, param_capture_line
        ldr    r2, [r0, #O_FB_BASE]
        str    r2, param_framebuffer0

        // Sanity check chars_per_line <= fb_width / 8
        ldr    r3, param_fb_width
        lsr    r3, r3, #3
        ldr    r2, param_chars_per_line
        cmp    r2, r3
        strgt  r3, param_chars_per_line

        // Sanity check nlines <= fb_height / 2
        ldr    r3, param_fb_height
        ldr    r2, param_nlines
        lsr    r3, r3, #1
        cmp    r2, r3
        strgt  r3, param_nlines

#ifdef MULTI_BUFFER
        // Calculate the base address of each of the 4 frame buffers
        ldr    r10, param_fb_height
        ldr    r11, param_fb_pitch
        ldr    r2, param_framebuffer0
        mul    r10, r10, r11
        add    r2, r10
        str    r2, param_framebuffer1
        add    r2, r10
        str    r2, param_framebuffer2
        add    r2, r10
        str    r2, param_framebuffer3
        // Default to displaying buffer 0 in Mode 7 (or on probe)
        tst    r1, #(BIT_MODE7 | BIT_PROBE) // options currently in r1!
        beq    skip_swap
        push   {r0-r3}
        mov    r0, #0
        bl     swapBuffer
        pop    {r0-r3}
skip_swap:
#endif
        // Setup r4 as a constant
        ldr    r4, =GPLEV0

        // Setup r3 with the flags/options parameter (as per before)
        mov    r3, r1

        // Setup r2 with the framebuffer pitch (as per before)
        ldr    r2, param_fb_pitch

        // Setup r2 with the number of active characters per line (as per before)
        ldr    r1, param_chars_per_line

        tst    r3, #BIT_CLEAR
        blne   clear_screen

        // clear all the state bits apart from the following:
        bic    r3, r3, #(BIT_FIELD_TYPE | BIT_CLEAR)

        // In Mode 7 (or on probe) write to buffer 0, display buffer 0
        bic    r3, r3, #(MASK_LAST_BUFFER | MASK_CURR_BUFFER)
#ifdef MULTI_BUFFER
        tst    r3, #(BIT_MODE7 | BIT_PROBE)
        // In modes 0..6, restore the previous buffer state
        ldreq  r10, buffer_state
        orreq  r3, r3, r10
#endif

frame:

        bl     wait_for_vsync

        // Working registers while frame is being captured
        //
        //  r0 = scratch register
        //  r1 = number of 8-pixel blocks to capture (=param_chars_per_line)
        //  r2 = frame buffer line pitch in bytes (=param_fb_pitch)
        //  r3 = flags register
        //  r4 = GPLEV0 constant
        //  r5 = line counter (counts down to 0)
        //  r6 = scratch register
        //  r7 = scratch register
        //  r8 = value read from GPLEV0
        //  r9 = scratch register
        // r10 = scratch register
        // r11 = pointer to current line in frame buffer

        // Pick the next draw buffer
        // In Mode 7, or if MULTI_BUFFER disabled, than draw to 0
        // else draw to the "spare" buffer
        mov    r0, #0
#ifdef MULTI_BUFFER
        tst    r3, #(BIT_MODE7 | BIT_PROBE)
        bne    buffer_chosen
        // Draw to the buffers cyclically, i.e. pick the one
        // after the last completed buffer, modulo <nbuffers + 1>
        // r8 and r9 are free at this point
        mov    r8, r3, lsr #OFFSET_LAST_BUFFER
        and    r8, r8, #3
        mov    r9, r3, lsr #OFFSET_NBUFFERS
        and    r9, r9, #3
        cmp    r8, r9
        beq    buffer_chosen
        add    r0, r8, #1
buffer_chosen:
#endif
        ldr    r8, =param_framebuffer0
        ldr    r11, [r8, r0, lsl #2]

        // remember this as the current buffer
        bic    r3, r3, #MASK_CURR_BUFFER
        orr    r3, r3, r0, lsl #OFFSET_CURR_BUFFER

        // The odd vs even field can be distinguished by the time between
        // the last two rising edges:
        // odd field (first field) should be 21/23us
        // even field (second field) should be 53/55us
        sub    r6, r6, r7

        // Save the current field type
        cmp    r6, #FIELD_TYPE_THRESHOLD
        biclt  r3, r3, #BIT_FIELD_TYPE  // Odd, clear bit
        orrge  r3, r3, #BIT_FIELD_TYPE  // Even, set bit

        // Check for mode change:
        // Odd: Mode 0..6 should be 21us, Mode 7 should be 23us
        // Even: Mode 0..6 should be 53us, Mode 7 should be 55us
        //
        // The above changes with smooth horizontal scrolling
        // - with R3= 6: 20.0us/52.0us
        // - with R3= 7: 20.5us/52.5us
        // - with R3= 8: 21.0us/53.0us <<< "Normal" case
        // - with R3= 9: 21.5us/53.5us
        // - with R3=10: 22.0us/54.0us
        //
        // Hence we use thresholds of 22.5us and 54.5us
        tst    r3, #BIT_FIELD_TYPE
        ldreq  r5, =22500     // Use 22.5us threshold in odd field
        ldrne  r5, =54500     // Use 54.5us threshold in even field
        cmp    r6, r5
        movlt  r0, #0         // Modes 0-6
        movge  r0, #1         // Mode 7
        tst    r3, #BIT_PROBE
        bne    exit
        tst    r3, #BIT_CALIBRATE
        bne    skip_switch_test

        // Test for keys being pressed, with variable rate auto repeat
        // Note: macro uses r5 as a scratch register
        ldr    r8, [r4]
        KEY_PRESS_DETECT SW1_MASK, RET_SW1, sw1counter
        KEY_PRESS_DETECT SW2_MASK, RET_SW2, sw2counter
        KEY_PRESS_DETECT SW3_MASK, RET_SW3, sw3counter
        tst    r0, #(RET_SW1 | RET_SW2 | RET_SW3)
        bne    exit

skip_switch_test:
        tst    r3, #BIT_MODE7
        moveq  r5, #0         // Modes 0-6
        movne  r5, #1         // Mode 7
        tst    r3, #BIT_MODE_DETECT // Have we been told to exit on mode change
        cmpne  r5, r0         // Check if we have changed mode
        bne    exit           // If so, then bail, as the frame buffer needs to be resized

        // Save a copy of the frame buffer base
        push   {r11}

        // Skip inactive lines
        ldr    r5, param_v_offset

        // Correct the relative positions of the odd and even frames
        // In Mode 0..6, reduce the number of active lines by one for the even frame
        // In Mode 7, increment the frame buffer pointer by one line for the even field
        tst    r3, #BIT_ELK
        bne    skip_line_loop
        tst    r3, #BIT_MODE7
        beq    fixupmodes
        tst    r3, #BIT_FIELD_TYPE
        addeq  r11, r11, r2
fixupmodes:
        tst    r3, #BIT_FIELD_TYPE
        subne  r5, r5, #1     // Modes 0-6 + 7
skip_line_loop:
        cmp    r5, #0
        ble    skip_line_loop_exit
        WAIT_FOR_CSYNC_0
        WAIT_FOR_CSYNC_1
        subs   r5, r5, #1
        b      skip_line_loop
skip_line_loop_exit:

        // Compute the current scanline mod 10
        ldr    r5, param_v_offset
        CLEAR_VSYNC
        add    r5, r5, #1
mod10:
        subs   r5, r5, #10
        bpl    mod10
        add    r5, r5, #10
        str    r5, linecountmod10

        // Process active lines
        ldr    r5, param_nlines
process_line_loop:

        SHOW_VSYNC

        // Preserve the state used by the outer code
        push   {r1-r5, r11}

        // Wait for the start of hsync
        WAIT_FOR_CSYNC_0
        READ_CYCLE_COUNTER r10

        // Wait for the end of hsync
        WAIT_FOR_CSYNC_1
        READ_CYCLE_COUNTER r6

        // Calculate length of low hsync pulse (in ARM cycles = ns)
        sub    r10, r6, r10

        // Start with the configured horizontal offset
        ldr    r6, param_h_offset

        // Implement half character horizontal scrolling:
        // - a "short"  hsync is 3.5us, leave h_offset as-is
        // - a "normal" hsync is 4.0us, increment h_offset by 1
        // - a "long"   hsync is 4.5us, increment h_offset by 2
        // So test against two thresholds inbetween these values
        cmp    r10, #(4000 + 224)
        addgt  r6, r6, #1
        cmp    r10, #(4000 - 224)
        addgt  r6, r6, #1

        // Load the address of the capture_line function into r10
        ldr    r10, param_capture_line

        // Skip the configured number of psync edges (modes 0..6: edges every 250ns, mode 7: edges ever 333ns)
skip_psync_loop:
        cmp    r6, #0
        orreq  r3, r3, #BIT_PSYNC
        beq    skip_psync_loop_exit
        WAIT_FOR_PSYNC_1                   // Skip a 0->1 edge of psync
        subs   r6, r6, #1
        biceq  r3, r3, #BIT_PSYNC
        beq    skip_psync_loop_exit
        sub    r6, r6, #1
        WAIT_FOR_PSYNC_0                   // Skip a 1->0 edge of psync
        b      skip_psync_loop

skip_psync_loop_exit:

        // The capture line function is provided the following:
        //   r0 = pointer to current line in frame buffer
        //   r1 = number of complete psync cycles to capture (=param_chars_per_line)
        //   r2 = frame buffer line pitch in bytes (=param_fb_pitch)
        //   r3 = flags register
        //   r4 = GPLEV0 constant
        //   r5 = frame buffer height (=param_fb_height)
        //   r6 = scan line count modulo 10
        //
        // All registers are available as scratch registers (i.e. nothing needs to be preserved)

        // Setup parameters
        mov    r0, r11
        ldr    r5, param_fb_height
        ldr    r6, linecountmod10

        // Call capture line function
        blx    r10

        // Restore the state used by the outer code
        pop    {r1-r5, r11}

        // Skip a whole line to maintain aspect ratio
        ldr    r0, linecountmod10
        add    r11, r11, r2, lsl #1
        add    r0, r0, #1
        cmp    r0, #10
        moveq  r0, #0
        str    r0, linecountmod10

        subs   r5, r5, #1
        bne    process_line_loop

        // Update the OSD in Mode 0..6
        pop    {r11}
        tst    r3, #BIT_MODE7
        bne    skip_osd_update
        push   {r0-r12, lr}
        mov    r0, r11        // start of current draw buffer
        mov    r1, r2         // bytes per line
        bl     osd_update_fast
        pop    {r0-r12, lr}
skip_osd_update:

#ifdef MULTI_BUFFER
        // Update the last drawn buffer
        mov    r0, r3, lsr #OFFSET_CURR_BUFFER
        and    r0, #3
        bic    r3, r3, #MASK_LAST_BUFFER
        orr    r3, r3, r0, lsl #OFFSET_LAST_BUFFER
        // Flip to it on next V SYNC
        FLIP_BUFFER
#endif

        push   {r0-r12, lr}
        bl     recalculate_hdmi_clock_line_locked_update
        pop    {r0-r12, lr}

        // Loop back if required number of fields has not been reached
        // or if negative (capture forever)
        ldr    r5, param_ncapture
        cmp    r5, #0
        blt    frame
        sub    r5, #1
        str    r5, param_ncapture
        bne    frame

        // Setup the response code
        mov    r0, r3
        and    r0, #BIT_MODE7
        orr    r0, #RET_EXPIRED

// Return
exit:
#ifdef MULTI_BUFFER
        // Save the old buffer state before exiting
        and    r3, r3, #MASK_LAST_BUFFER
        str    r3, buffer_state
        // Return the current buffer state
        orr    r0, r0, r3
#endif
        pop    {r4-r12, lr}
        mov    pc, lr

// ======================================================================
// WAIT_FOR_VSYNC
// ======================================================================

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

        // Compare with 8us to descriminate short from long
        // - normal hsync pulses are 4us
        // - during vsync everything is either inverted, or clamped to zero
        // - this results in hsync pulses between 9us and 128us
        cmp    r5, #8000
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

// ======================================================================
// MEASURE_VSYNC
// ======================================================================

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

        pop    {r4-r12, pc}

// ======================================================================
// CLEAR_SCREEN
// ======================================================================

clear_screen:
        ldr    r5, param_fb_height
        ldr    r6, param_fb_pitch
        ldr    r11, param_framebuffer0
        ldr    r8, =0x88888888
        mul    r6, r5, r6

#ifdef MULTI_BUFFER
        mov    r5, #NBUFFERS
        mul    r6, r5, r6
#endif

clear_loop:
        ldr    r7, [r11]
        subs   r6, r6, #4
        and    r7, r8
        str    r7, [r11], #4
        bne    clear_loop
        mov    pc, lr

// ======================================================================
// Local Variables
// ======================================================================

sw1counter:
        .word 0

sw2counter:
        .word 0

sw3counter:
        .word 0

param_framebuffer0:
        .word  0

#ifdef MULTI_BUFFER
param_framebuffer1:
        .word  0

param_framebuffer2:
        .word  0

param_framebuffer3:
        .word  0

buffer_state:
        .word  0
#endif

param_fb_pitch:
        .word  0

param_fb_width:
        .word  0

param_fb_height:
        .word  0

param_fb_bpp:
        .word  0

param_chars_per_line:
        .word  0

param_nlines:
        .word  0

param_h_offset:
        .word  0

param_v_offset:
        .word  0

param_ncapture:
        .word  0

param_capture_line:
        .word 0

linecountmod10:
        .word 0

vsync_line:
        .word 0



