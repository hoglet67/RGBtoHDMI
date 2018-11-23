#include "rpi-base.h"
#include "defs.h"

#define FIELD_TYPE_THRESHOLD 32768

.text
.global rgb_to_fb
.global measure_vsync
.global sw1counter
.global sw2counter
.global sw3counter

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
        mov    r7, #0
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
        mov    r0, r3, lsr #OFFSET_LAST_BUFFER
        and    r0, r0, #3
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

.macro WAIT_FOR_PSYNC_01
        tst    R3, #BIT_PSYNC
        beq    waitpsync01\@
        WAIT_FOR_PSYNC_0
        b      exitpsync01\@
waitpsync01\@:
        WAIT_FOR_PSYNC_1
exitpsync01\@:
.endm

.macro WAIT_FOR_PSYNC_10
        tst    R3, #BIT_PSYNC
        beq    waitpsync10\@
        WAIT_FOR_PSYNC_1
        b      exitpsync10\@
waitpsync10\@:
        WAIT_FOR_PSYNC_0
exitpsync10\@:
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


.macro CAPTURE_LOW_BITS
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 in GPIO  7.. 5 ->  3.. 0
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 in GPIO 13..11 -> 11.. 8

        and    r10, r8, #(7 << PIXEL_BASE)
        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        mov    r10, r10, lsl #(4 - PIXEL_BASE)
        orr    r10, r10, r9, lsr #(3 + PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        orr    r10, r10, r9, lsl #(6 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsr #(1 + PIXEL_BASE)
.endm

.macro CAPTURE_HIGH_BITS
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
.endm


.macro PROCESS_CHARS_LOOP
        tst    r3, #BIT_MODE7
        bne    process_chars_7_simple\@

process_chars_loop\@:

        WAIT_FOR_PSYNC_10

        CAPTURE_LOW_BITS

        WAIT_FOR_PSYNC_01

        CAPTURE_HIGH_BITS

        // Orr in the VSync indicator
        ldr    r8, =0x11111111
        tst    r3, #BIT_VSYNC_MARKER
        orrne  r10, r10, r8

        // Line double always in Modes 0-6 regardless of interlace
        // On the multi core Pi this introduces stalling artefacts
#ifndef HAS_MULTICORE
        tst    r3, #BIT_SCANLINES
        movne  r0, #0
        moveq  r0, r10
        str    r0, [r12, r2]
#endif

        str    r10, [r12], #4
        subs   r6, r6, #1
        bne    process_chars_loop\@

        b      exit_process_chars\@

process_chars_7_simple\@:

        tst    r3, #BIT_NEW_DEINT
        bne    process_chars_7_advanced\@

        // The Simple Motion Adaptive Deinterlacing algorithm below was created
        // by Ian Bradbury (IanB on stardot). Many thanks Ian.
        // Working registers
        //
        //  r0 = pixel value from comparison buffer
        //  r1 = pixel value from other field in comparison buffer
        //  r2 = bytes per line
        //  r3 = field state
        //  r4 = GPLEV0
        //  r5 = pixel value from other field of video buffer (for OSD bits)
        //  r6 = pixel counter
        //  r7 = red overlay for vsync indicator
        //  r8 = value read from GPLEV0
        //  r9 = extracted pixel
        // r10 = block of 8 pixels, to be written to FB
        // r11 = pointer into comparison buffer (moves within line)
        // r12 = pointer into frame buffer (moves within line)

        push   {r1, r5,r11}              // need scratch registers

        ldr    r11, param_fb_height
        
        tst    r3, #BIT_FIELD_TYPE       // test odd or even field
        
        mla    r11, r11, r2, r12         // offset to second buffer used for comparison not for display
                                         // now absolute address of pixel group in comparison buffer
    
        rsbeq  r2, r2,#0                 // negate R2 offset if odd field to write to line above (restored to original value on exit)
             
process_chars_loop_7_simple\@:

        WAIT_FOR_PSYNC_10
        ldr    r0, [r11]            // preload old pixel value from comparison buffer
        CAPTURE_LOW_BITS
        ldr    r1, [r11,r2]         // preload other field old pixel value from comparison buffer
        WAIT_FOR_PSYNC_01
        tst    r3, #BIT_OSD
        moveq  r5,#0
        ldrne  r5, [r12,r2]         // preload old pixel value from other field of video buffer
        CAPTURE_HIGH_BITS

        ldr    r8, =0x77777777      // mask to extract OSD

        tst    r3, #(BIT_OSD | BIT_CALIBRATE)
        moveq  r9,#0
        ldrne  r9, [r12]            // preload old pixel value from video buffer

        and    r14, r8, r0          // mask out old flag bits from comparison buffer value
        cmp    r14, r10             // is old value same as new value?
        orrne  r10, #0x80000000     // set 1st flag if different
        tst    r1,  #0x80000000     // test motion flag in last field (R1 finished with after this)
        orrne  r10, #0x00800000     // set 2nd flag if other field had motion
        tst    r0,  #0x80000000
        orrne  r10, #0x00008000     // set 3rd flag as old 1st flag
        tst    r0,  #0x00800000
        orrne  r10, #0x00000080     // set 4th flag as old 2nd flag
        str    r10, [r11]           // save new value to comparison buffer including flag bit

                                    // test for calibration or deinterlace disabled
                                    // if either branch to skip_deinterlace\@ as deinterlace code messes up the calibration
        tst    r3, #(BIT_CALIBRATE | BIT_NO_DEINT)
        and    r1, r3, #MASK_INTERLACE
        bne skip_deinterlace_simple\@
        
        movs   r1, r1, lsr #OFFSET_INTERLACE   // put interlace setting in R1 0-3: 1 field - 4 fields

                                    // if setting =  0 then clear 2nd motion flag
        biceq  r10, r10, #0x00800000
        cmp    r1,#2                // if setting =< 2 then clear 4th motion flag
        bicle  r10, r10, #0x00000080
        cmp    r1,#1                // if setting =< 1 then clear 3rd motion flag
        bicle  r10, r10, #0x00008000

        bics   r1, r10, r8          // extract motion flags
                                    // if no motion then don't deinterlace
        bicne  r5, r5, r8           // extract the OSD bits from old pixel value
        andne  r10, r10, r8         // clear motion flags
        orrne  r5, r5, r10          // merge new pixel data
        strne  r5, [r12,r2]         // save new pixel data in other field
skip_deinterlace_simple\@:

        bic    r9, r9, r8           // extract the OSD bits from old pixel value
        and    r10, r10, r8         // clear motion flags
        orr    r10, r10, r9         // OR in OSD bits from old pixel value

        orr    r10, r7              // Orr in the VSync indicator

        str    r10, [r12], #4       // write new pixel value to video buffer
        add    r11,r11,#4
        subs   r6, r6, #1
        bne    process_chars_loop_7_simple\@

        tst    r3, #BIT_FIELD_TYPE  // test odd or even field
        rsbeq  r2, r2,#0            // negate offset to restore to old value before exit

        pop    {r1, r5, r11}        // restore scratch registers
        b      exit_process_chars\@

process_chars_7_advanced\@:

        // The Advanced Motion Adaptive Deinterlacing algorithm below was created
        // by Ian Bradbury (IanB on stardot). Many thanks Ian.
        //
        // Working registers
        //
        //  r0 = storage for 1st then 2nd word of 3 word pixel capture
        //  r1 = misc
        //  r2 = bytes per line
        //  r3 = field state
        //  r4 = GPLEV0
        //  r5 = first word of three word char group (comparison buffer - other field)
        //  r6 = second word of three word char group (comparison buffer - other field)
        //  r7 = third word of three word char group (comparison buffer - other field)
        //  r8 = misc
        //  r9 = misc
        // r10 = current pixel capture
        // r11 = pointer into comparison buffer (moves within line)
        // r12 = pointer into frame buffer (moves within line)
        // r14 = misc

        push   {r1, r2, r5 ,r11, r14}         // need scratch registers

        ldr    r11, param_fb_height
        mla    r11, r11, r2 , r12   // offset to second buffer used for comparison not for display
                                    // now absolute address of pixel group in comparison buffer

        tst    r3, #BIT_FIELD_TYPE  // test odd or even field
        rsbeq  r2, r2,#0            // negate R2 offset if odd field to write to line above (restored to original value on exit)

process_chars_loop_7\@:

        WAIT_FOR_PSYNC_10
        push   {r6}
        CAPTURE_LOW_BITS
        tst    r3, #(BIT_OSD | BIT_CALIBRATE)
        ldmneia r12, {r5, r6, r7}           // preload current field old screen values (three words) if OSD on or calibrating
        moveq  r5,#0
        moveq  r6,#0
        moveq  r7,#0
        ldr    r1, =0x77777777              // osd bitmask
        WAIT_FOR_PSYNC_01  
        adr    r14, osdbuffer1
        bic    r5, r5, r1                   // extract OSD bits
        bic    r6, r6, r1
        bic    r7, r7, r1
        stmia  r14!, {r5, r6, r7}           // save for later in cached memory afterwards r14 points to osdbufferA1
        CAPTURE_HIGH_BITS
        add    r12, r12, r2
        tst    r3, #BIT_OSD
        ldmneia r12, {r5, r6, r7}           // preload other field old screen values (three words) if OSD on
        ldreq  r6, [r12, #4]                // always load middle word as sometimes half has to be written back to screen during deinterlace
        moveq  r5,#0
        moveq  r7,#0
        mov    r0, r10                      // save left pixel data for later
        WAIT_FOR_PSYNC_10
        sub    r12, r12, r2
        bic    r5, r5, r1                   // extract OSD bits
        bic    r7, r7, r1                   // extract OSD bits
        stmia  r14, {r5, r6, r7}            // save for later in osdbufferA1 but don't extract OSD bits on r6 as might need half old pixel data
        CAPTURE_LOW_BITS
        add    r11, r11, r2                 // r11 points to other field
        ldmia  r11, {r5, r6, r7}            // preload other field old values from comparison buffer (3 words)
        WAIT_FOR_PSYNC_01
        sub    r11, r11, r2                 // restore r11
        CAPTURE_HIGH_BITS

        //  r0 = left 8 pixels of 1st char
        //  r10 bottom half = right 4 pixels of 1st char
        //  r5 = left 8 pixels of 1st char from other field comparison buffer
        //  r6 bottom half = right 4 pixels of 1st char from other field comparison buffer
        
        ldr    r9, osdbuffer1
        ldr    r8, =0x11111111

        str    r0, [r11]                     // save new value in comparison buffer

        orr    r9, r9, r0                    // merge OSD with new value
        tst    r3, #BIT_VSYNC_MARKER
        orrne  r9, r9, r8                    // add red vsync bar
        str    r9, [r12]                     // save to screen
        ldr    r9, osdbuffer2
        str    r10, [r11, #4]                // save new value in comparison buffer
        tst    r3, #BIT_VSYNC_MARKER
        orr    r9, r9, r10                   // merge OSD with new value
        orrne  r9, r9, r8                    // add red vsync bar
        str    r9, [r12, #4]                 // save to screen

        // test for calibration or deinterlace disabled
        // if either branch to skip_deinterlace\@ as deinterlace code messes up the calibration
        tst    r3, #(BIT_CALIBRATE | BIT_NO_DEINT)
        bne    nodeinterlace1\@

        // deinterlace 1st char here
        eor    r8, r0, r5            // r8 = difference with other field comparison buffer
        eor    r9, r10, r6           // r9 = difference with other field comparison buffer

        tst    r8,#0x00007000
        bne    deinterlace1\@        // leftmost char column
        tst    r9,#0x00000700
        bne    deinterlace1\@        // rightmost char column


        and    r8, r0, #0x77         // r8 is now two left pixels which are always background with text
        mov    r1,#0
        eor    r9, r0, r8, lsl #8    // eor with rest of pixels in character to detect non-background colour pixels
        eor    r9, r9, r8, lsl #16
        eor    r9, r9, r8, lsl #24
        tst    r9, #0x00000700       // 12 pixels wide in a char, ignore left 3 and right 1 and convert the rest to a byte
        orrne  r1, r1, #0x01
        tst    r9, #0x00700000
        orrne  r1, r1, #0x02
        tst    r9, #0x00070000
        orrne  r1, r1, #0x04
        tst    r9, #0x70000000
        orrne  r1, r1, #0x08
        tst    r9, #0x07000000
        orrne  r1, r1, #0x10
        eor    r9, r10, r8           // repeat the above with the last four pixels of char
        eor    r9, r9, r8, lsl #8
        tst    r9, #0x00000070
        orrne  r1, r1, #0x20
        tst    r9, #0x00000007
        orrne  r1, r1, #0x40
        tst    r9, #0x00007000
        orrne  r1, r1, #0x80

        and    r8, r5, #0x77          // do the same 8 bit extraction as above on the comparison buffer values
        mov    r14, #0
        eor    r9, r5, r8, lsl #8
        eor    r9, r9, r8, lsl #16
        eor    r9, r9, r8, lsl #24
        tst    r9, #0x00000700
        orrne  r14, r14, #0x01
        tst    r9, #0x00700000
        orrne  r14, r14, #0x02
        tst    r9, #0x00070000
        orrne  r14, r14, #0x04
        tst    r9, #0x70000000
        orrne  r14, r14, #0x08
        tst    r9, #0x07000000
        orrne  r14, r14, #0x10
        eor    r9, r6, r8             // repeat the above with the last four pixels of char
        eor    r9, r9, r8, lsl #8
        tst    r9, #0x00000070
        orrne  r14, r14, #0x20
        tst    r9, #0x00000007
        orrne  r14, r14, #0x40
        tst    r9, #0x00007000
        orrne  r14, r14, #0x80

        tst    r3, #BIT_FIELD_TYPE  // test odd or even field and swap comparison pair if required
        moveq  r9, r1
        moveq  r1, r14
        moveq  r14, r9

        ldr    r9, charline         // get current vertical line pair in character (0-9)
        subs   r9, r9, #1
        bmi    deinterlace1\@

        //r9 = current line pair for a character
        //r1 & r14 = 1 bit per pixel representation of 2 lines of a character

        cmp    r9,#0x02             // do some exception testing to save 7K of lookup table
        cmpne  r9,#0x05
        cmpeq  r1,#0x81
        cmpeq  r14,#0xc3
        beq    nodeinterlace1\@

        cmp    r14,#0x7f             // more exception testing
        cmpne  r14,#0x9e
        cmpne  r14,#0xfe
        cmpeq  r1,#0xff
        cmpeq  r9,#0x06
        beq    nodeinterlace1\@

        ldr    r8, =rounding_lookup  // use lookup table to determine if new value and old comparison value are two lines of a rounded character
        add    r8, r8, r9, lsl #9
        ldrb   r9, [r8, r1]
        cmp    r9, #0
        beq    deinterlace1\@
        cmp    r9, r14
        beq    nodeinterlace1\@      // if rounding pair then don't deinterlace
        add    r8, r8, #0x100
        ldrb   r9, [r8, r1]          // second lookup table
        cmp    r9, #0
        beq    deinterlace1\@
        cmp    r9, r14
        beq    nodeinterlace1\@      // if rounding pair then don't deinterlace

deinterlace1\@:
        ldr    r9, osdbufferA1       // get OSD bits
        orr    r9, r9, r0
        str    r9, [r12,r2]          // save to other line of screen to deinterlace

        ldr    r9, osdbufferA2       // get OSD bits
        add    r12, r12, #4
        uxth   r8, r10               // clear top 16 bits of r10 put result in r8
        bic    r9, r9, #0x00000077
        bic    r9, r9, #0x00007700
        orr    r9, r9, r8
        str    r9, osdbufferA2
        str    r9, [r12,r2]           // save to other line of screen to deinterlace
        sub    r12, r12, #4

nodeinterlace1\@:
        mov    r0, r10                // save middle word in r0
        
        WAIT_FOR_PSYNC_10
        CAPTURE_LOW_BITS
        WAIT_FOR_PSYNC_01
        CAPTURE_HIGH_BITS

        // deinterlace 2nd char here

        //  all the above code is repeated for the 2nd character but is slightly different
        //  r0 top half = left 4 pixels of 2nd char
        //  r10 = right 8 pixels of 2nd char
        //  r6 top half = left 4 pixels of 2nd char from other field comparison buffer
        //  r7 = right 8 pixels of 2nd char from other field comparison buffer

        ldr    r9, osdbuffer3
        str    r10, [r11, #8]           // save new value in comparison buffer
        ldr    r8, =0x11111111          // add red vsync bar
        orr    r9, r9, r10              // merge OSD with new value
        tst    r3, #BIT_VSYNC_MARKER
        orrne  r9, r9, r8
        str    r9, [r12, #8]            // save to screen

        // test for calibration or deinterlace disabled
        // if either branch to skip_deinterlace\@ as deinterlace code messes up the calibration
        tst    r3, #(BIT_CALIBRATE | BIT_NO_DEINT)
        bne    nodeinterlace2\@

        // deinterlace 2nd char here
        eor    r8, r10, r7           // r8 = difference with other field comparison buffer
        eor    r9, r0, r6            // r9 = difference with other field comparison buffer

        tst    r9,#0x70000000
        bne    deinterlace2\@       // leftmost char column
        tst    r8,#0x07000000
        bne    deinterlace2\@       // rightmost char column


        and    r8, r0, #0x770000    // r8 is now two left pixels which are always background with text
        mov    r1,#0
        eor    r9, r0, r8, lsl #8   // eor with rest of pixels in character to detect non-background colour pixels
        tst    r9, #0x07000000      // 12 pixels wide in a char, ignore left 3 and right 1 and convert the rest to a byte
        orrne  r1, r1, #0x01
        eor    r9, r10, r8, lsr #16 // repeat the above with the last eight pixels of char
        eor    r9, r9, r8, lsr #8
        eor    r9, r9, r8
        eor    r9, r9, r8, lsl #8
        tst    r9, #0x00000070
        orrne  r1, r1, #0x02
        tst    r9, #0x00000007
        orrne  r1, r1, #0x04
        tst    r9, #0x00007000
        orrne  r1, r1, #0x08
        tst    r9, #0x00000700
        orrne  r1, r1, #0x10
        tst    r9, #0x00700000
        orrne  r1, r1, #0x20
        tst    r9, #0x00070000
        orrne  r1, r1, #0x40
        tst    r9, #0x70000000
        orrne  r1, r1, #0x80

        and    r8, r6, #0x770000    // do the same 8 bit extraction as above on the comparison buffer values
        mov    r14,#0
        eor    r9, r6, r8, lsl #8
        tst    r9, #0x07000000
        orrne  r14, r14, #0x01
        eor    r9, r7, r8, lsr #16  // repeat the above with the last eight pixels of char
        eor    r9, r9, r8, lsr #8
        eor    r9, r9, r8
        eor    r9, r9, r8, lsl #8
        tst    r9, #0x00000070
        orrne  r14, r14, #0x02
        tst    r9, #0x00000007
        orrne  r14, r14, #0x04
        tst    r9, #0x00007000
        orrne  r14, r14, #0x08
        tst    r9, #0x00000700
        orrne  r14, r14, #0x10
        tst    r9, #0x00700000
        orrne  r14, r14, #0x20
        tst    r9, #0x00070000
        orrne  r14, r14, #0x40
        tst    r9, #0x70000000
        orrne  r14, r14, #0x80

        tst    r3, #BIT_FIELD_TYPE   // test odd or even field and swap comparison pair if required

        moveq  r9, r1
        moveq  r1, r14
        moveq  r14, r9

        ldr    r9, charline          // get current vertical line pair in character (0-9)
        subs   r9, r9, #1
        bmi    deinterlace2\@

        //r9 = current line pair for a character
        //r1 & r14 = 1 bit per pixel representation of 2 lines of a character

        cmp    r9,#0x02              // do some exception testing to save 7K of lookup table
        cmpne  r9,#0x05
        cmpeq  r1,#0x81
        cmpeq  r14,#0xc3
        beq    nodeinterlace2\@

        cmp    r14,#0x7f              // more exception testing
        cmpne  r14,#0x9e
        cmpne  r14,#0xfe
        cmpeq  r1,#0xff
        cmpeq  r9,#0x06
        beq    nodeinterlace2\@

        ldr    r8, =rounding_lookup  // use lookup table to determine if new value and old comparison value are two lines of a rounded character
        add    r8, r8, r9, lsl #9
        ldrb   r9, [r8, r1]
        cmp    r9, #0
        beq    deinterlace2\@
        cmp    r9, r14
        beq    nodeinterlace2\@      // if rounding pair then don't deinterlace
        add    r8, r8, #0x100
        ldrb   r9, [r8, r1]          // second lookup table
        cmp    r9, #0
        beq    deinterlace2\@
        cmp    r9, r14
        beq    nodeinterlace2\@      // if rounding pair then don't deinterlace

deinterlace2\@:

        add    r12, r12, #4
        ldr    r9, osdbufferA2       // get OSD bits
        bic    r8, r0, #0x000000ff
        bic    r8, r8, #0x0000ff00
        bic    r9, r9, #0x00770000
        bic    r9, r9, #0x77000000
        orr    r9, r9, r8
        str    r9, [r12,r2]          // save to other line of screen to deinterlace

        ldr    r9, osdbufferA3
        add    r12, r12, #4
        orr    r9, r9, r10
        str    r9, [r12,r2]          // save to other line of screen to deinterlace
        sub    r12, r12, #8

nodeinterlace2\@:

        pop    {r6}

        add    r11, r11, #12
        add    r12, r12, #12
        subs   r6, r6, #3
        bgt    process_chars_loop_7\@

        pop    {r1, r2, r5, r11, r14}        // restore scratch registers


exit_process_chars\@:

.endm

       // Insert the current literal pool, otherwise constants are to far away and you get a build error
        .ltorg

rgb_to_fb:

        push   {r4-r12, lr}

        // Save the capture_info_t parameters to absolute addresses
        ldr    r2, [r0, #O_FB_PITCH]
        str    r2, param_fb_pitch
        ldr    r2, [r0, #O_FB_WIDTH]
        str    r2, param_fb_width
        ldr    r2, [r0, #O_FB_HEIGHT]
        str    r2, param_fb_height
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
        ldr    r2, [r0, #O_FB_BASE]
        str    r2, param_framebuffer0

        // Sanity check chars_per_line <= fb_width / 8
        
        ldr    r3, param_fb_width        
        ldr    r2, param_chars_per_line
        lsr    r3, r3, #3
        cmp    r2, r3
        strgt  r3, param_chars_per_line

        // Sanity check nlines <= fb_height / 2
        ldr    r2, param_nlines
        ldr    r3, param_fb_height
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

        // Working registers in the second half
        //
        //  r0 = scratch register
        //  r1 = 8-pixel characters per line (normally 80, but 62.5 (63) in Mode 7
        //  r2 = bytes per line
        //  r3 = field state
        //  r4 = GPLEV0
        //  r5 = line counter
        //  r6 = pixel counter
        //  r7 = red overlay for vsync indicator
        //  r8 = value read from GPLEV0
        //  r9 = extracted pixel
        // r10 = block of 8 pixels, to be written to FB
        // r11 = pointer into frame buffer (start of line)
        // r12 = pointer into frame buffer (moves within line)

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
        CLEAR_VSYNC

        ldr    r5, param_v_offset
        add    r5, r5, #1
mod10:
        subs   r5, r5, #10
        bpl    mod10
        add    r5,r5, #10
        str    r5, charline
        // Process active lines
        ldr    r5, param_nlines

process_line_loop:

        SHOW_VSYNC

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

        mov    r12, r11                    // pointer into frame buffer

        // Skip the configured number of psync edges (modes 0..6: edges every 250ns, mode 7: edges ever 333ns)
skip_psync_loop:
        cmp    r6, #0
        orreq  r3, r3, #BIT_PSYNC
        beq    skip_psync_loop_exit
        sub    r6, r6, #1
        WAIT_FOR_PSYNC_1                   // Skip a 0->1 edge of psync
        cmp    r6, #0
        biceq  r3, r3, #BIT_PSYNC
        beq    skip_psync_loop_exit
        sub    r6, r6, #1
        WAIT_FOR_PSYNC_0                   // Skip a 1->0 edge of psync
        b      skip_psync_loop

osdbuffer1:                                // all 6 osd buffers must be sequential
        .word 0
osdbuffer2:
        .word 0
osdbuffer3:
        .word 0
osdbufferA1:
        .word 0
osdbufferA2:
        .word 0
osdbufferA3:
        .word 0

skip_psync_loop_exit:
        mov    r6, r1                      // number of 8-bit characters per line
        PROCESS_CHARS_LOOP                 // expect a 1->0 edge of psync
        b      next_line


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

charline:
        .word 0

       // Insert the current literal pool, otherwise constants are to far away and you get a build error
        .ltorg

next_line:
        // Skip a whole line to maintain aspect ratio
        ldr    r0, charline
        add    r11, r11, r2, lsl #1
        add    r0, r0, #1
        cmp    r0, #10
        moveq  r0, #0
        str    r0, charline

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
        ldr    r11, param_framebuffer0
        ldr    r8, =0x88888888
        ldr    r5, param_fb_height
#ifdef MULTI_BUFFER
        mov    r6, #NBUFFERS
        mul    r5, r5, r6
#endif
clear_loop1:
        ldr    r6, param_fb_pitch
clear_loop2:
        ldr    r7, [r11]
        and    r7, r8
        str    r7, [r11], #4
        subs   r6, r6, #4
        bne    clear_loop2
        subs   r5, r5, #1
        bne    clear_loop1
        mov    pc, lr


           // Insert the current literal pool, otherwise constants are to far away and you get a build error
        .ltorg

rounding_lookup:

//Line pair:0/0
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x0E
        .byte 0x0F
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x3C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x70
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x7C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFF
        .byte 0xFF
        .byte 0x00
        .byte 0xC1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00

//Line pair:0/1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x0F
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFC
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00

//Line pair:1/0
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x07
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x1C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x38
        .byte 0x00
        .byte 0x00
        .byte 0x19
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x18
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x7C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xE7
        .byte 0x00
        .byte 0xC0
        .byte 0xC3
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xC7
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xDB
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x81
        .byte 0x00
        .byte 0x81
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x86
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x99
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x71
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFF
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00

//Line pair:1/1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x38
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x1C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xC1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00

//Line pair:2/0
        .byte 0x00
        .byte 0x03
        .byte 0x00
        .byte 0x01
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x06
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x1B
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x1D
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFF
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x67
        .byte 0x00
        .byte 0x60
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFE
        .byte 0xFF
        .byte 0xC0
        .byte 0x83
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xC6
        .byte 0x8F
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x99
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x81
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x60
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xF6
        .byte 0x7E
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFF
        .byte 0x7E

//Line pair:2/1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x07
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x1F
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xE0
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFF
        .byte 0x00
        .byte 0x00
        .byte 0xC1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x70
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00

//Line pair:3/0
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x01
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x18
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x18
        .byte 0x00
        .byte 0x1E
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x70
        .byte 0x00
        .byte 0x00
        .byte 0x61
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xF8
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFE
        .byte 0x00
        .byte 0xC3
        .byte 0x00
        .byte 0x81
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xB9
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x80
        .byte 0x81
        .byte 0x00
        .byte 0x81
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x99
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x76
        .byte 0x66
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x7C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x99
        .byte 0x00
        .byte 0x7F

//Line pair:3/1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x1C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFF
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x7E
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFE

//Line pair:4/0
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x0E
        .byte 0x0E
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x06
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x39
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x39
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x1C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x70
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x7E
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xF9
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xE7
        .byte 0xFE
        .byte 0xC0
        .byte 0xC3
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x87
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xF9
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xF9
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x80
        .byte 0x81
        .byte 0x00
        .byte 0x81
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x98
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x70
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x66
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xE1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFF
        .byte 0x00

//Line pair:4/1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x07
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFF
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00

//Line pair:5/0
        .byte 0x00
        .byte 0x03
        .byte 0x00
        .byte 0x01
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x06
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x38
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x18
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x18
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x18
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x7E
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xE1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xE6
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xE7
        .byte 0x00
        .byte 0x00
        .byte 0x83
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xDB
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x81
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x99
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x70
        .byte 0x00
        .byte 0x00
        .byte 0xE1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x7E
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xF3
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00

//Line pair:5/1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x38
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x1C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xC1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xFF
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xF0
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00

//Line pair:6/0
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x06
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x1C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x18
        .byte 0x00
        .byte 0x1E
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x60
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x7E
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x98
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x81
        .byte 0x00
        .byte 0x81
        .byte 0x00
        .byte 0x00
        .byte 0x86
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x70
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x66

//Line pair:6/1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x7E

//Line pair:7/0
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x06
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x1C
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x18
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0xC0
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00

//Line pair:7/1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00

//Line pair:8/0
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x06
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x7E
        .byte 0x00

//Line pair:8/1
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
        .byte 0x00
