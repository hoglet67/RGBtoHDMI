.macro  LINE_TIMEOUT_TEST
        READ_CYCLE_COUNTER r8
        subs   r8, r8, r14
        rsbmi  r8, r8, #0
        cmp    r8, #LINE_TIMEOUT
.endm


.macro WAIT_FOR_CSYNC_0
        READ_CYCLE_COUNTER r14
waitlo\@:
        LINE_TIMEOUT_TEST
        bgt    endlo\@
        // Read the GPLEV0
        ldr    r8, [r4]
        tst    r8, #CSYNC_MASK
        bne    waitlo\@

        // Check again in case of noise
        tst    r3, #BIT_OLD_CPLDV1V2
        ldrne  r8, [r4]
        tstne  r8, #CSYNC_MASK
        bne    waitlo\@
endlo\@:
.endm

.macro WAIT_FOR_CSYNC_1
        READ_CYCLE_COUNTER r14
waithi\@:
        LINE_TIMEOUT_TEST
        bgt    endhi\@
        // Read the GPLEV0
        ldr    r8, [r4]
        tst    r8, #CSYNC_MASK
        beq    waithi\@
        // Check again in case of noise

        tst    r3, #BIT_OLD_CPLDV1V2
        ldrne  r8, [r4]
        eorne  r8, r8, #CSYNC_MASK
        tstne  r8, #CSYNC_MASK
        bne    waithi\@
endhi\@:
.endm

.macro WAIT_FOR_CSYNC_0_R9
        READ_CYCLE_COUNTER r14
waitlo9\@:
        LINE_TIMEOUT_TEST
        bgt    endlo9\@
        // Read the GPLEV0
        ldr    r8, [r4]
        cmp    r9, #0
        tstne  r8, #CSYNC_MASK
        bne    waitlo9\@

        // Check again in case of noise
        cmp    r9, #0
        tstne  r3, #BIT_OLD_CPLDV1V2
        ldrne  r8, [r4]
        tstne  r8, #CSYNC_MASK
        bne    waitlo9\@
endlo9\@:
.endm

.macro WAIT_FOR_CSYNC_0_FAST_R9
        READ_CYCLE_COUNTER r14
waitloF\@:
        LINE_TIMEOUT_TEST
        bgt    endloF9\@
        // Read the GPLEV0
        ldr    r8, [r4]
        cmp    r9, #0
        tstne  r8, #CSYNC_MASK
        bne    waitloF\@
endloF9\@:
.endm

.macro WAIT_FOR_CSYNC_1_FAST
        READ_CYCLE_COUNTER r14
waithiF\@:
        LINE_TIMEOUT_TEST
        bgt    endhiF\@
        // Read the GPLEV0
        ldr    r8, [r4]
        eor    r8, r8, #CSYNC_MASK
        tst    r8, #CSYNC_MASK
        bne    waithiF\@
endhiF\@:
.endm

.macro  SWITCH_PSYNC_TO_VSYNC
        ldr    r8, =GPCLR0
        mov    r9, #1 << VERSION_PIN
        str    r9, [r8]                          // set version = 0
.endm

.macro  SWITCH_VSYNC_TO_PSYNC
        ldr    r8, =GPSET0
        mov    r9, #1 << VERSION_PIN
        str    r9, [r8]                          // set version = 1
.endm

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

        // Read a second time to capture stable data
        // This is executed only if CPLD is V1 or V2
        tst    r3, #BIT_OLD_CPLDV1V2
        ldrne  r8, [r4]
        eorne  r8, r3
        tstne  r8, #PSYNC_MASK
        bne    wait\@

        // toggle the polarity to look for the opposite edge next time
        eor    r8, r3    // restore r8 value
        eor    r3, #PSYNC_MASK
.endm

.macro WAIT_FOR_PSYNC_EDGE_FAST
waitPF\@:
        // Read the GPLEV0
        ldr    r8, [r4]
        eor    r8, r3
        tst    r8, #PSYNC_MASK
        bne    waitPF\@

        // toggle the polarity to look for the opposite edge next time
        eor    r8, r3    // restore r8 value
        eor    r3, #PSYNC_MASK
.endm

.macro WAIT_FOR_PSYNC_EDGE_SLOW
        READ_CYCLE_COUNTER r14
waitPS\@:
        READ_CYCLE_COUNTER r8
        subs   r8, r8, r14
        rsbmi  r8, r8, #0
        cmp    r8, #FRAME_TIMEOUT
        bgt    endPS\@
        // Read the GPLEV0
        ldr    r8, [r4]
        eor    r8, r3
        tst    r8, #PSYNC_MASK
        bne    waitPS\@

        // Read the GPLEV0
        ldr    r8, [r4]
        eor    r8, r3
        tst    r8, #PSYNC_MASK
        bne    waitPS\@

        // Read the GPLEV0
        ldr    r8, [r4]
        eor    r8, r3
        tst    r8, #PSYNC_MASK
        bne    waitPS\@

        // toggle the polarity to look for the opposite edge next time
        eor    r8, r3    // restore r8 value
endPS\@:
        eor    r3, #PSYNC_MASK
.endm

.macro SKIP_PSYNC
        // called if 4 bits per pixel in non-fast mode so has support for old CPLV v1 & v2
        WAIT_FOR_CSYNC_0_R9
        READ_CYCLE_COUNTER r10
        bic    r3, r3, #PSYNC_MASK             // wait for zero after CSYNC
        // Wait for the end of hsync
        WAIT_FOR_CSYNC_1
        READ_CYCLE_COUNTER r14
        // Calculate length of low hsync pulse (in ARM cycles = ns)
        subs   r10, r14, r10
        rsbmi  r10, r10, #0

        // Start with the configured horizontal offset
        // Implement half character horizontal scrolling:
        // - a "short"  hsync is 3.5us, leave h_offset as-is
        // - a "normal" hsync is 4.0us, increment h_offset by 1
        // - a "long"   hsync is 4.5us, increment h_offset by 2
        // So test against two thresholds inbetween these values

        // old CPLD V1 & V2 code
        mov    r9, r7
        cmp    r10, #(4000 + 224)
        addgt  r9, r9, #1
        cmp    r10, #(4000 - 224)
        addgt  r9, r9, #1

        // new CPLD V3 or later code
        mov    r8, r7
        cmp    r10, #(4000 + 224)
        addlt  r8, r8, #1
        cmp    r10, #(4000 - 224)
        addlt  r8, r8, #1

        tst    r3, #BIT_OLD_CPLDV1V2
        movne  r8, r9

        tst    r3, #BIT_NO_H_SCROLL
        moveq  r7, r8                     // only allow fine sideways scrolling in bbc / electron mode (causes timing issues in ega mode)
        // Skip the configured number of psync edges (modes 0..6: edges every 250ns, mode 7: edges ever 333ns)
skip_psync_loop\@:
        WAIT_FOR_PSYNC_EDGE               // wait for next edge of psync
        subs   r7, r7, #1
        bne    skip_psync_loop\@
.endm

.macro  SKIP_PSYNC_NO_OLD_CPLD
        // only called if 6 bits/pixel in non-fast mode (old CPLDs v1 & v2 don't work at 6bpp so no need for test)
        WAIT_FOR_CSYNC_0_FAST_R9
        READ_CYCLE_COUNTER r10
        bic   r3, r3, #PSYNC_MASK         // wait for zero after CSYNC
        // Wait for the end of hsync
        WAIT_FOR_CSYNC_1_FAST
        READ_CYCLE_COUNTER r14
        // Calculate length of low hsync pulse (in ARM cycles = ns)
        subs   r10, r14, r10
        rsbmi  r10, r10, #0
        // Calculate length of low hsync pulse (in ARM cycles = ns)
        // Start with the configured horizontal offset
        // Implement half character horizontal scrolling:
        // - a "short"  hsync is 3.5us, leave h_offset as-is
        // - a "normal" hsync is 4.0us, increment h_offset by 1
        // - a "long"   hsync is 4.5us, increment h_offset by 2
        // So test against two thresholds inbetween these values

        // new CPLD code only (not called from CPLD v1 & v2)
        mov    r8, r7
        cmp    r10, #(4000 + 224)
        addlt  r8, r8, #1
        cmp    r10, #(4000 - 224)
        addlt  r8, r8, #1

        tst    r3, #BIT_NO_H_SCROLL
        moveq  r7, r8                      // only allow fine sideways scrolling in bbc / electron mode (causes timing issues in ega mode)
        // Skip the configured number of psync edges (modes 0..6: edges every 250ns, mode 7: edges ever 333ns)
skip_psync_loop_no_old\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        subs   r7, r7, #1
        bne    skip_psync_loop_no_old\@
.endm

.macro SKIP_PSYNC_NO_H_SCROLL
        // only called if in fast mode (both 3 & 6 bpp) - fast mode never called from old CPLDs v1 & v2
        WAIT_FOR_CSYNC_0_FAST_R9
        bic   r3, r3, #PSYNC_MASK         // wait for zero after CSYNC
        WAIT_FOR_CSYNC_1_FAST
        READ_CYCLE_COUNTER r14
skip_psync_loop_no_h_scroll\@:
        WAIT_FOR_PSYNC_EDGE_FAST          // wait for next edge of psync
        subs   r7, r7, #1
        bne    skip_psync_loop_no_h_scroll\@
.endm

.macro CAPTURE_0_BITS_WIDE reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 4
        // Pixel 1 in GPIO 13.. 8 ->  3.. 0

        and    r9, r8, #(0x0f << PIXEL_BASE)
        and    r14, r8, #(0x0f << (PIXEL_BASE + 6))
        eor    r10, \reg, r9, lsl #(4 - PIXEL_BASE)
        eor    r10, r10, r14, lsr #(6 + PIXEL_BASE)
.endm

.macro CAPTURE_1_BITS_WIDE
        // Pixel 0 in GPIO  7.. 2 ->  15.. 12
        // Pixel 1 in GPIO 13.. 8 ->  11.. 8

        and    r9, r8, #(0x0f << (PIXEL_BASE))
        and    r14, r8, #(0x0f << (PIXEL_BASE + 6))
        eor    r10, r10, r9, lsl #(12 - PIXEL_BASE)
        eor    r10, r10, r14, lsr #(2 - PIXEL_BASE)
.endm

.macro CAPTURE_2_BITS_WIDE
        // Pixel 4 in GPIO  7.. 2 -> 23..20
        // Pixel 5 in GPIO 13.. 8 -> 19..16
        and    r9, r8, #(0x0f << PIXEL_BASE)
        and    r14, r8, #(0x0f << (PIXEL_BASE + 6))
        eor    r10, r10, r9, lsl #(20 - PIXEL_BASE)
        eor    r10, r10, r14, lsl #(10 - PIXEL_BASE)
.endm

.macro CAPTURE_3_BITS_WIDE reg
        // Pixel 6 in GPIO 7.. 2 -> 31..28
        // Pixel 7 in GPIO 13..8 -> 27..24
        and    r9, r8, #(0x0f << PIXEL_BASE)
        and    r14, r8, #(0x0f << (PIXEL_BASE + 6))
        eor    r10, r10, r9, lsl #(28 - PIXEL_BASE)
        eor    \reg, r10, r14, lsl #(18 - PIXEL_BASE)
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

.macro CAPTURE_LOW_BITS_8BPP_WIDE reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        and    r9, r8, #(0x3f << PIXEL_BASE)
        and    r14, r8, #(0x3f << (PIXEL_BASE + 6))
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    r10, r10, r14, lsl #(8 - (PIXEL_BASE + 6))
.endm


.macro CAPTURE_HIGH_BITS_8BPP_WIDE reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        and    r9, r8, #(0x3f << PIXEL_BASE)
        and    r14, r8, #(0x3f << (PIXEL_BASE + 6))
        eor    r10, r10, r9, lsl #(16 - PIXEL_BASE)
        eor    \reg, r10, r14, lsl #(24 - (PIXEL_BASE + 6))
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
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE
        ldreq   r8, =0x88888888
        orreq   r10, r10, r8
        tst    r3,  #BIT_NO_LINE_DOUBLE
        streq  r10, [r0, -r2]
        add    r0, r0, #4
.endm

.macro WRITE_WORDS_8BPP_FAST
        eor    r9, r9, r5       //eor in vsync and debug
        eor    r10, r10, r6     //eor in vsync and debug
        stmia  r0, {r9, r10}
        sub    r0, r0, r2
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE
        ldreq   r8, =0x80808080
        orreq   r9, r9, r8
        orreq   r10, r10, r8
        tst    r3, #BIT_NO_LINE_DOUBLE
        stmeqia  r0, {r9, r10}
        add    r0, r0, r2
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
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE
        ldreq   r8, =0x88888888
        orreq   r10, r10, r8
        tst    r3,  #BIT_NO_LINE_DOUBLE
        streq  r10, [r0, -r2]
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
        sub    r0, r0, r2
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE
        ldreq   r8, =0x80808080
        orreq   r9, r9, r8
        orreq   r10, r10, r8
        tst    r3, #BIT_NO_LINE_DOUBLE
        stmeqia  r0, {r9, r10}
        add    r0, r0, r2
        add    r0, r0, #8
.endm

.macro SETUP_DUMMY_PARAMETERS
        ldr     r0, =(dummyscreen + 1024)    //in case data written backwards
        mov     r1, #3
        mov     r2, #0
        orr     r3, r3, #BIT_VSYNC_MARKER    // ensure that constants are in data cache
        orr     r3, r3, #BIT_EVEN_SAMPLES
        orr     r3, r3, #BIT_ODD_SAMPLES
        mov     r5, #1
        mov     r6, #0
        mov     r7, #2
        mov     r8, #1
        mov     r9, #0  //force skip of wait for csync 0
.endm

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
        tst    r3, #(BIT_PROBE)
        bne    novsync\@
        // Poll for the VSYNC interrupt
        ldr    r0, =INTPEND2
        ldr    r0, [r0]
        tst    r0, #(1<<VSYNCINT)
        beq    novsync\@
        // Clear the VSYNC interrupt
        CLEAR_VSYNC
        // If the vsync indicator is enabled, mark the next line in red
        tst    r3, #(BIT_VSYNC)
        orrne  r3, r3, #BIT_VSYNC_MARKER
        // Remember the line where vsync occurred
        str    r5, vsync_line
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
