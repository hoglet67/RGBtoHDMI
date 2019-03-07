// Wait for the next edge on psync
//   if r3 bit 17 = 0 - wait for falling edge
//   if r3 bit 17 = 1 - wait for rising edge
.macro WAIT_FOR_PSYNC_EDGE
wait\@:
        // Read the GPLEV0
        ldr    r8, [r4]
        eor    r8, r3
        tst    r8, #PSYNC_MASK
        bne    wait\@
        // Check again in case of noise
        ldr    r8, [r4]
        eor    r8, r3
        tst    r8, #PSYNC_MASK
        bne    wait\@
        // toggle the polarity to look for the opposite edge next time
        eor    r8, r3
        eor    r3, #PSYNC_MASK
.endm

.macro SKIP_PSYNC
        // Skip the configured number of psync edges (modes 0..6: edges every 250ns, mode 7: edges ever 333ns)
        orr    r3, r3, #PSYNC_MASK             // first edge is a 0->1
        cmp    r7, #0
        beq    skip_psync_loop_exit\@
skip_psync_loop\@:
        WAIT_FOR_PSYNC_EDGE                // wait for next edge of psync
        subs   r7, r7, #1
        bne    skip_psync_loop\@
skip_psync_loop_exit\@:
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
        and    r8, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #(6 - PIXEL_BASE)
        orr    r10, r10, r8, lsr #(1 + PIXEL_BASE)
.endm

.macro CAPTURE_HIGH_BITS
        // Pixel 4 in GPIO  4.. 2 -> 23..20
        // Pixel 5 in GPIO  7.. 5 -> 19..16
        // Pixel 6 in GPIO 10.. 8 -> 31..28
        // Pixel 7 in GPIO 13..11 -> 27..24

        and    r9, r8, #(7 << PIXEL_BASE)
        and    r14, r8, #(7 << (PIXEL_BASE + 3))
        orr    r10, r10, r9, lsl #(20 - PIXEL_BASE)
        orr    r10, r10, r14, lsl #(13 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r8, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #(22 - PIXEL_BASE)
        orr    r10, r10, r8, lsl #(15 - PIXEL_BASE)
.endm

.macro CAPTURE_BITS_8BPP
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24

        and    r10, r8, #(7 << PIXEL_BASE)
        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        mov    r10, r10, lsr #(PIXEL_BASE)
        orr    r10, r10, r9, lsl #(8 - (PIXEL_BASE + 3))

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r8, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #(16 - (PIXEL_BASE + 6))
        orr    r10, r10, r8, lsl #(24 - (PIXEL_BASE + 9))
.endm

.macro CAPTURE_LOW_BITS_TRANSLATE
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 in GPIO  7.. 5 ->  3.. 0
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 in GPIO 13..11 -> 11.. 8

        and    r10, r8, #(7 << PIXEL_BASE)
        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        mov    r10, r10, lsl #(4 - PIXEL_BASE)
        orr    r10, r10, r9, lsr #(3 + PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r8, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #(6 - PIXEL_BASE)
        orr    r10, r10, r8, lsr #(1 + PIXEL_BASE)
         
        tst    r3, #BIT_EVEN_SAMPLES
        ldrne  r9, =0x70707070 
        bicne  r10, r10, r9
        orrne  r10, r10, r10, lsl #4
                     
        tst    r3, #BIT_ODD_SAMPLES
        ldrne  r9, =0x07070707 
        bicne  r10, r10, r9
        orrne  r10, r10, r10, lsr #4   
        
        mov    r6, r6, lsl #8   // mode 0 sentinel
        mov    r7, r7, lsl #2   // mode 0-6 sentinel
        mov    r14, #0          // mode 2 translation
        
        tst    r10, #0x00000070       
        orrne  r14, r14, #0x08
        orrne  r6, r6, #0x80
        tst    r10, #0x00000007
        orrne  r14, r14, #0x080000
        orrne  r6, r6, #0x40
        tst    r10, #0x00007000
        orrne  r14, r14, #0x04
        orrne  r6, r6, #0x20
        tst    r10, #0x00000700
        orrne  r14, r14, #0x040000
        orrne  r6, r6, #0x10
        orrne  r7, r7, #2
.endm

.macro CAPTURE_HIGH_BITS_TRANSLATE
        // Pixel 4 in GPIO  4.. 2 -> 23..20
        // Pixel 5 in GPIO  7.. 5 -> 19..16
        // Pixel 6 in GPIO 10.. 8 -> 31..28
        // Pixel 7 in GPIO 13..11 -> 27..24

        and    r9, r8, #(7 << PIXEL_BASE)           // this block unoptimised to free up r14
        orr    r10, r10, r9, lsl #(20 - PIXEL_BASE)
        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        orr    r10, r10, r9, lsl #(13 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r8, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #(22 - PIXEL_BASE)
        orr    r10, r10, r8, lsl #(15 - PIXEL_BASE)
        
        tst    r3, #BIT_EVEN_SAMPLES
        ldrne  r9, =0x70707070 
        bicne  r10, r10, r9
        orrne  r10, r10, r10, lsl #4
                     
        tst    r3, #BIT_ODD_SAMPLES
        ldrne  r9, =0x07070707 
        bicne  r10, r10, r9
        orrne  r10, r10, r10, lsr #4   
        
        tst    r10, #0x00700000
        orrne  r14, r14, #0x02
        orrne  r6, r6, #0x08
        tst    r10, #0x00070000
        orrne  r14, r14, #0x020000
        orrne  r6, r6, #0x04
        tst    r10, #0x70000000
        orrne  r14, r14, #0x01
        orrne  r6, r6, #0x02
        tst    r10, #0x07000000
        orrne  r14, r14, #0x010000
        orrne  r6, r6, #0x01
        orrne  r7, r7, #1
        
        tst    r3, #BITDUP_MODE2_16COLOUR  
        orrne  r10, r14, r14, lsl #4
        orrne  r10, r10, r10, lsl #8   
        
.endm

.macro CAPTURE_LOW_BITS_TRANSLATE_8BPP
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24
  
        and    r5, r8, #(7 << PIXEL_BASE)
        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        mov    r5, r5, lsr #(PIXEL_BASE)
        orr    r5, r5, r9, lsl #(8 - (PIXEL_BASE + 3))

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r8, r8, #(7 << (PIXEL_BASE + 9))
        orr    r5, r5, r9, lsl #(16 - (PIXEL_BASE + 6))
        orr    r5, r5, r8, lsl #(24 - (PIXEL_BASE + 9))
        
        tst    r3, #BIT_EVEN_SAMPLES
        ldrne  r9, =0x00070007 
        bicne  r5, r5, r9
        orrne  r5, r5, r5, lsr #8
                     
        tst    r3, #BIT_ODD_SAMPLES
        ldrne  r9, =0x07000700 
        bicne  r5, r5, r9
        orrne  r5, r5, r5, lsl #8   

        mov    r6, r6, lsl #8   // mode 0 sentinel
        mov    r7, r7, lsl #2   // mode 0-6 sentinel
        mov    r14, #0          // mode 2 translation (low byte = left pixel, high byte = right pixel
        tst    r5, #0x00000007       
        orrne  r14, r14, #0x08
        orrne  r6, r6, #0x80
        tst    r5, #0x00000700
        orrne  r14, r14, #0x08000000
        orrne  r6, r6, #0x40
        tst    r5, #0x00070000
        orrne  r14, r14, #0x04
        orrne  r6, r6, #0x20
        tst    r5, #0x07000000
        orrne  r14, r14, #0x04000000
        orrne  r6, r6, #0x10
        orrne  r7, r7, #2    
.endm

.macro CAPTURE_HIGH_BITS_TRANSLATE_8BPP
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24

        and    r10, r8, #(7 << PIXEL_BASE)
        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        mov    r10, r10, lsr #(PIXEL_BASE)
        orr    r10, r10, r9, lsl #(8 - (PIXEL_BASE + 3))

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r8, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #(16 - (PIXEL_BASE + 6))
        orr    r10, r10, r8, lsl #(24 - (PIXEL_BASE + 9))
        
        tst    r3, #BIT_EVEN_SAMPLES
        ldrne  r9, =0x00070007 
        bicne  r10, r10, r9
        orrne  r10, r10, r10, lsr #8
                     
        tst    r3, #BIT_ODD_SAMPLES
        ldrne  r9, =0x07000700 
        bicne  r10, r10, r9
        orrne  r10, r10, r10, lsl #8       
        
        tst    r10, #0x00000007
        orrne  r14, r14, #0x02
        orrne  r6, r6, #0x08
        tst    r10, #0x00000700
        orrne  r14, r14, #0x02000000
        orrne  r6, r6, #0x04
        tst    r10, #0x00070000
        orrne  r14, r14, #0x01
        orrne  r6, r6, #0x02
        tst    r10, #0x07000000
        orrne  r14, r14, #0x01000000
        orrne  r6, r6, #0x01
        orrne  r7, r7, #1
        
        tst    r3, #BITDUP_MODE2_16COLOUR
        moveq  r9, r5
        andne  r9, r14, #0xff    
        orrne  r9, r9, r9, lsl #8
        orrne  r9, r9, r9, lsl #16
        andne  r10, r14, #0xff000000  
        orrne  r10, r10, r10, lsr #8
        orrne  r10, r10, r10, lsr #16  
.endm


.macro CAPTURE_BITS_DOUBLE
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4 and  3.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15..12 and 11.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..20 and 19..16
        // Pixel 3 in GPIO 13..11 -> 31..28 and 27..24

        and    r10, r8, #(7 << PIXEL_BASE)
        and    r9, r8, #(7 << (PIXEL_BASE + 3))
        mov    r10, r10, lsl #(4 - PIXEL_BASE)
        orr    r10, r10, r9, lsl #(12 - (PIXEL_BASE + 3))

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r8, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r9, lsl #(20 - (PIXEL_BASE + 6))
        orr    r10, r10, r8, lsl #(28 - (PIXEL_BASE + 9))

        // Pixel double
        orr    r10, r10, lsr #4
.endm

.macro CAPTURE_BITS_DOUBLE_8BPP
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24

        and    r9, r8, #(7 << PIXEL_BASE)
        and    r10, r8, #(7 << (PIXEL_BASE + 3))
        mov    r9, r9, lsr #(PIXEL_BASE)
        orr    r9, r9, r10, lsl #(16 - (PIXEL_BASE + 3))

        and    r10, r8, #(7 << (PIXEL_BASE + 6))
        and    r8, r8, #(7 << (PIXEL_BASE + 9))
        orr    r10, r10, r10, lsr #(PIXEL_BASE + 6)
        orr    r10, r10, r8, lsl #(16 - (PIXEL_BASE + 9))
        // Pixel double
        orr    r9, r9, lsl #8
        orr    r10, r10, lsl #8
.endm

.macro WRITE_WORD_FAST
        eor    r10, r10, r6     //eor in vsync and debug
        str    r10, [r0]
        tst    r3,  #BIT_SCANLINES
        movne  r10, #0
        tst    r3,  #BITDUP_NO_LINE_DOUBLE
        streq  r10, [r0, r2]
        add    r0, r0, #4
.endm

.macro WRITE_WORDS_8BPP_FAST
        eor    r9, r9, r5       //eor in vsync and debug
        eor    r10, r10, r6     //eor in vsync and debug
        stmia  r0, {r9, r10}  
        add    r0, r0, r2   
        tst    r3,  #BIT_SCANLINES
        movne  r9, #0
        movne  r10, #0
        tst    r3, #BITDUP_NO_LINE_DOUBLE
        stmeqia  r0, {r9, r10}
        subs   r0, r0, r2
        add    r0, r0, #8 
.endm

.macro WRITE_WORD
        tst     r3, #BIT_VSYNC_MARKER
        ldrne  r8, =0x11111111
        eorne  r10, r10, r8            // eor in the VSync indicator (orr doesn't work on zx80/81 due to white screen)
        tst    r3, #BIT_DEBUG
        eorne  r10, r10, #0x50         //magenta in leftmost
        eorne  r10, r10, #0x02000000   //green in rightmost
        str    r10, [r0]
        tst    r3,  #BIT_SCANLINES
        movne  r10, #0
        tst    r3,  #BITDUP_NO_LINE_DOUBLE
        streq  r10, [r0, r2]
        add    r0, r0, #4
.endm   
        
.macro WRITE_WORDS_8BPP 
        and    r8, r3, #MASKDUP_PALETTE_HIGH_NIBBLE
        mov    r8, r8, lsr #(OFFSETDUP_PALETTE_HIGH_NIBBLE - 4)
        orr    r8, r8, r8, lsl #8
        orr    r8, r8, r8, lsl #16
        orr    r9, r9, r8
        orr    r10, r10, r8
        tst    r3, #BIT_VSYNC_MARKER
        ldrne  r8, =0x01010101
        eorne  r9, r9, r8              // eor in the VSync indicator (orr doesn't work on zx80/81 due to white screen)
        eorne  r10, r10, r8
        tst    r3, #BIT_DEBUG
        eorne  r9, r9, #0x05           //magenta in leftmost
        eorne  r10, r10, #0x02000000   //green in rightmost
        stmia  r0, {r9, r10}  
        add    r0, r0, r2   
        tst    r3,  #BIT_SCANLINES
        movne  r9, #0
        movne  r10, #0
        tst    r3, #BITDUP_NO_LINE_DOUBLE
        stmeqia  r0, {r9, r10}
        subs   r0, r0, r2
        add    r0, r0, #8
.endm 
       
.macro SETUP_DUMMY_PARAMETERS
        ldr     r0, =(dummyscreen + 1024)    //in case data written backwards       
        mov     r1, #1
        mov     r2, #0
        orr     r3, r3, #BIT_VSYNC_MARKER    // ensure that constants are in data cache
        orr     r3, r3, #BIT_EVEN_SAMPLES
        orr     r3, r3, #BIT_ODD_SAMPLES
        mov     r5, #(DUPLICATE_HEIGHT * 2)
        mov     r6, #0
        mov     r7, #1
.endm         