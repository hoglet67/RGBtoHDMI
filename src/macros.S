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
