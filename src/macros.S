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

.macro CONVERT_WORD_MODE0
        // Any non-zero pixel gets converted sets a bit
        mov    r9, #0
        tst    r10, #0x00000070       
        orrne  r9, r9, #0x80
        tst    r10, #0x00000007
        orrne  r9, r9, #0x40
        tst    r10, #0x00007000
        orrne  r9, r9, #0x20
        tst    r10, #0x00000700
        orrne  r9, r9, #0x10
        tst    r10, #0x00700000
        orrne  r9, r9, #0x08
        tst    r10, #0x00070000
        orrne  r9, r9, #0x04
        tst    r10, #0x70000000
        orrne  r9, r9, #0x02
        tst    r10, #0x07000000
        orrne  r9, r9, #0x01
.endm 
.macro CONVERT_MODE0_MODE2
        tst    r3, #BIT_MODE2_16COLOUR
        beq    standardPalette\@
        //     reinterpret mode 0 bits as 16 colour mode 2
        mov    r10,#0
        tst    r9, #0x01       
        orrne  r10, r10, #0x010000
        tst    r9, #0x04
        orrne  r10, r10, #0x020000
        tst    r9, #0x10
        orrne  r10, r10, #0x040000
        tst    r9, #0x40
        orrne  r10, r10, #0x080000
        tst    r9, #0x02       
        orrne  r10, r10, #0x01
        tst    r9, #0x08
        orrne  r10, r10, #0x02
        tst    r9, #0x20
        orrne  r10, r10, #0x04
        tst    r9, #0x80
        orrne  r10, r10, #0x08
                
        orr    r10, r10, r10, lsl #4
        orr    r10, r10, r10, lsl #8        
standardPalette\@:
.endm

.macro WRITE_WORD

        // eor in the VSync indicator (orr doesn't work on zx80/81 due to white screen)
        eor    r10, r10, r7
        tst    r3, #BIT_DEBUG
        orrne  r10, r10, #0x10         //red in leftmost
        orrne  r10, r10, #0x02000000   //green in rightmost
        // Line double always in Modes 0-6 regardless of interlace
        // On the multi core Pi this introduces stalling artefacts
#ifndef HAS_MULTICORE
        tst    r3, #BIT_SCANLINES
        streq  r10, [r0, r2]
        strne  r6, [r0, r2]
#endif
        str    r10, [r0], #4
.endm        
        