.macro LINE_TIMEOUT_TEST
        READ_CYCLE_COUNTER r8
        subs   r8, r8, r14
        rsbmi  r8, r8, #0
        cmp    r8, #LINE_TIMEOUT
        // Read the GPLEV0
        ldr    r8, [r4]
        eorgt  r8, r8, #CSYNC_MASK        //inverting the value after the timeout will cause the test to pass
        tst    r8, #CSYNC_MASK
.endm

.macro LINE_TIMEOUT_TEST_SKIP_HSYNC
        READ_CYCLE_COUNTER r8
        subs   r8, r8, r14
        rsbmi  r8, r8, #0
        cmp    r8, #LINE_TIMEOUT
        // Read the GPLEV0
        ldr    r8, [r4]
        eorgt  r8, r8, #CSYNC_MASK        //inverting the value after the timeout will cause the test to pass
        tst    r3, #BIT_NO_SKIP_HSYNC
        tstne  r8, #CSYNC_MASK
.endm

.macro WAIT_FOR_CSYNC_0
        READ_CYCLE_COUNTER r14
waitlo\@:
        LINE_TIMEOUT_TEST
        bne    waitlo\@
        LINE_TIMEOUT_TEST
        bne    waitlo\@
        LINE_TIMEOUT_TEST
        bne    waitlo\@
.endm

.macro WAIT_FOR_CSYNC_0_LONG
        READ_CYCLE_COUNTER r14
waitlo_long\@:
        LINE_TIMEOUT_TEST
        bne    waitlo_long\@
        LINE_TIMEOUT_TEST
        bne    waitlo_long\@
        LINE_TIMEOUT_TEST
        bne    waitlo_long\@
        LINE_TIMEOUT_TEST
        bne    waitlo_long\@
        LINE_TIMEOUT_TEST
        bne    waitlo_long\@
        LINE_TIMEOUT_TEST
        bne    waitlo_long\@
.endm

.macro WAIT_FOR_CSYNC_0_FAST_SKIP_HSYNC
        READ_CYCLE_COUNTER r14
waitloF\@:
        LINE_TIMEOUT_TEST_SKIP_HSYNC
        bne    waitloF\@
.endm

.macro WAIT_FOR_CSYNC_0_SKIP_HSYNC
        READ_CYCLE_COUNTER r14
waitlo9\@:
        LINE_TIMEOUT_TEST_SKIP_HSYNC
        bne    waitlo9\@
        LINE_TIMEOUT_TEST_SKIP_HSYNC
        bne    waitlo9\@
        LINE_TIMEOUT_TEST_SKIP_HSYNC
        bne    waitlo9\@
.endm

.macro WAIT_FOR_CSYNC_1_LONG
        READ_CYCLE_COUNTER r14
waithi_long\@:
        LINE_TIMEOUT_TEST
        beq    waithi_long\@
        LINE_TIMEOUT_TEST
        beq    waithi_long\@
        LINE_TIMEOUT_TEST
        beq    waithi_long\@
        LINE_TIMEOUT_TEST
        beq    waithi_long\@
        LINE_TIMEOUT_TEST
        beq    waithi_long\@
        LINE_TIMEOUT_TEST
        beq    waithi_long\@
.endm

.macro WAIT_FOR_CSYNC_1_FAST
        READ_CYCLE_COUNTER r14
waithiF\@:
        LINE_TIMEOUT_TEST
        beq    waithiF\@
.endm

.macro WAIT_FOR_CSYNC_1
        READ_CYCLE_COUNTER r14
waithi\@:
        LINE_TIMEOUT_TEST
        beq    waithi\@
        LINE_TIMEOUT_TEST
        beq    waithi\@
        LINE_TIMEOUT_TEST
        beq    waithi\@
.endm

.macro  SWITCH_PSYNC_TO_VSYNC
        push   {r0-r12, lr}
        mov    r0, #0
        bl     set_vsync_psync
        pop    {r0-r12, lr}
.endm

.macro  SWITCH_VSYNC_TO_PSYNC
        push   {r0-r12, lr}
        mov    r0, #1
        bl     set_vsync_psync
        pop    {r0-r12, lr}
.endm

#ifdef USE_ARM_CAPTURE

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
        tst    r3, #BIT_OLD_FIRMWARE_SUPPORT
        ldrne  r8, [r4]
        eorne  r8, r3
        tstne  r8, #PSYNC_MASK
        bne    wait\@

        // toggle the polarity to look for the opposite edge next time
        eor    r8, r3    // restore r8 value
        eor    r3, #PSYNC_MASK
.endm

.macro  SKIP_PSYNC_COMMON_NO_OLD_CPLD
        // only called if 6 bits/pixel in non-fast mode (old CPLDs v1 & v2 don't work at 6bpp so no need for test)
        WAIT_FOR_CSYNC_0_FAST_SKIP_HSYNC
        bic   r3, r3, #PSYNC_MASK    // wait for zero after CSYNC
        READ_CYCLE_COUNTER r10
        push  {r10}                 //save leading edge timestamp

        tst    r3, #BIT_HSYNC_EDGE    // if leading edge then don't wait for end of hsync (means scroll detection won't work)
        bne    do_skip_psync_no_old\@
        pop   {r10}
        mov   r6, r9, lsr #16        //HSYNC_SCROLL_HI
        bic   r9, r9, #0xff000000
        bic   r9, r9, #0x00ff0000    //HSYNC_SCROLL_LO
        // Wait for the end of hsync
        WAIT_FOR_CSYNC_1_FAST
        READ_CYCLE_COUNTER r14
        push  {r14}                 //save trailing edge timestamp
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
        bic    r3, #BIT_INHIBIT_MODE_DETECT
        // new CPLD code only (not called from CPLD v1 & v2)
        mov    r8, r7
        cmp    r10, r6              //HSYNC_SCROLL_HI
        addlt  r8, r8, #1
        orrgt  r3, r3, #BIT_INHIBIT_MODE_DETECT
        cmp    r10, r9              //HSYNC_SCROLL_LO
        addlt  r8, r8, #1
        orrlt  r3, r3, #BIT_INHIBIT_MODE_DETECT
        tst    r3, #BIT_NO_H_SCROLL
        moveq  r7, r8                      // only allow fine sideways scrolling in bbc / electron mode (causes timing issues in ega mode)
        // Skip the configured number of psync edges (modes 0..6: edges every 250ns, mode 7: edges ever 333ns)
do_skip_psync_no_old\@:
.endm

.macro  SKIP_PSYNC_NO_OLD_CPLD
        SKIP_PSYNC_COMMON_NO_OLD_CPLD
skip_psync_loop_no_old\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        subs   r7, r7, #1
        bne    skip_psync_loop_no_old\@
.endm

.macro SKIP_PSYNC_COMMON
        // called if 4 bits per pixel in non-fast mode so has support for old CPLV v1 & v2
        WAIT_FOR_CSYNC_0_SKIP_HSYNC
        bic    r3, r3, #PSYNC_MASK             // wait for zero after CSYNC
        READ_CYCLE_COUNTER r10
        push   {r10}

        tst    r3, #BIT_HSYNC_EDGE    // if leading edge then don't wait for end of hsync (means scroll detection won't work)
        bne    do_skip_psync\@

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
        bic    r3, #BIT_INHIBIT_MODE_DETECT
        mov    r8, r7
        tst    r3, #BIT_OLD_FIRMWARE_SUPPORT
        beq    notoldfirmwarescroll\@

        // old CPLD V1 & V2 code
        cmp    r10, r9, lsr #16     //HSYNC_SCROLL_HI
        addgt  r8, r8, #1
        orrgt  r3, r3, #BIT_INHIBIT_MODE_DETECT
        bic    r9, r9, #0xff000000
        bic    r9, r9, #0x00ff0000
        cmp    r10, r9              //HSYNC_SCROLL_LO
        addgt  r8, r8, #1
        orrlt  r3, r3, #BIT_INHIBIT_MODE_DETECT
        b      doneoldfirmwarescroll\@
notoldfirmwarescroll\@:
        // new CPLD V3 or later code
        cmp    r10, r9, lsr #16     //HSYNC_SCROLL_HI
        addlt  r8, r8, #1
        orrgt  r3, r3, #BIT_INHIBIT_MODE_DETECT
        bic    r9, r9, #0xff000000
        bic    r9, r9, #0x00ff0000
        cmp    r10, r9              //HSYNC_SCROLL_LO
        addlt  r8, r8, #1
        orrlt  r3, r3, #BIT_INHIBIT_MODE_DETECT
doneoldfirmwarescroll\@:
        tst    r3, #BIT_NO_H_SCROLL
        moveq  r7, r8                     // only allow fine sideways scrolling in bbc / electron mode (causes timing issues in ega mode)
        // Skip the configured number of psync edges (modes 0..6: edges every 250ns, mode 7: edges ever 333ns)
do_skip_psync\@:
.endm

.macro  SKIP_PSYNC
        SKIP_PSYNC_COMMON
skip_psync_loop\@:
        WAIT_FOR_PSYNC_EDGE               // wait for next edge of psync
        subs   r7, r7, #1
        bne    skip_psync_loop\@
.endm

.macro  SKIP_PSYNC_SIMPLE_FAST
        ldr    r8, =param_delay
        ldr    r8, [r8]
        add    r7, r7, r8
        ldr    r8, =param_sync_edge
        ldr    r8, [r8]
        orr    r3, r3, #PSYNC_MASK    // only -ve edge (inverted later)
        cmp    r8, #0
        beq    edge_trail_neg\@
        cmp    r8, #1
        beq    edge_lead_neg\@
        bic    r3, r3, #PSYNC_MASK    // only +ve edge (inverted later)
        cmp    r8, #2
        beq    edge_trail_pos\@
        cmp    r8, #3
        beq    edge_lead_pos\@
        cmp    r8, #4
        beq    edge_trail_both\@
//        cmp    r8, #5
//        beq    edge_lead_both\@
//edge_lead_both\@:
//        bic   r3, r3, #PSYNC_MASK    // wait for zero
wait_csync_lo_fast3\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        tst    r3, #BIT_NO_SKIP_HSYNC
        tstne  r8, #CSYNC_MASK
        bne    wait_csync_lo_fast3\@
        READ_CYCLE_COUNTER r10        //store timestamp in R2 instead of stack for fast mode

        b      skip_psync_loop_simple_fast\@

edge_trail_both\@:
//        bic   r3, r3, #PSYNC_MASK    // wait for zero
wait_csync_lo_fast4\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        tst    r3, #BIT_NO_SKIP_HSYNC
        tstne  r8, #CSYNC_MASK
        bne    wait_csync_lo_fast4\@
        READ_CYCLE_COUNTER r10        //store timestamp in R2 instead of stack for fast mode
        eor    r3, r3, #BIT_NO_SKIP_HSYNC
wait_csync_hi_fast4\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        tst    r3, #BIT_NO_SKIP_HSYNC
        tsteq  r8, #CSYNC_MASK
        beq    wait_csync_hi_fast4\@
        eor    r3, r3, #BIT_NO_SKIP_HSYNC
        b      skip_psync_loop_simple_fast\@

edge_lead_neg\@:
edge_lead_pos\@:
        //incoming psync state controls edge
wait_csync_lo_fast\@:
        WAIT_FOR_PSYNC_EDGE_FAST
        WAIT_FOR_PSYNC_EDGE_FAST
        tst    r3, #BIT_NO_SKIP_HSYNC
        tstne  r8, #CSYNC_MASK
        bne    wait_csync_lo_fast\@
        READ_CYCLE_COUNTER r10        //store timestamp in R2 instead of stack for fast mode
        b      skip_psync_loop_simple_fast\@

edge_trail_neg\@:
edge_trail_pos\@:
        //incoming psync state controls edge *** this one used by amiga
wait_csync_lo_fast2\@:
        WAIT_FOR_PSYNC_EDGE_FAST
        WAIT_FOR_PSYNC_EDGE_FAST
        tst    r3, #BIT_NO_SKIP_HSYNC
        tstne  r8, #CSYNC_MASK
        bne    wait_csync_lo_fast2\@
        READ_CYCLE_COUNTER r10        //store timestamp in R2 instead of stack for fast mode
        eor    r3, r3, #BIT_NO_SKIP_HSYNC
wait_csync_hi_fast\@:
        WAIT_FOR_PSYNC_EDGE_FAST
        WAIT_FOR_PSYNC_EDGE_FAST
        tst    r3, #BIT_NO_SKIP_HSYNC
        tsteq  r8, #CSYNC_MASK
        beq    wait_csync_hi_fast\@
        eor    r3, r3, #BIT_NO_SKIP_HSYNC
        // Skip the configured number of psync edges (modes 0..6: edges every 250ns, mode 7: edges ever 333ns)

skip_psync_loop_simple_fast\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        subs   r7, r7, #1
        bne    skip_psync_loop_simple_fast\@
        push  {r10}
.endm

#else

//**********************GPU CAPTURE**********************

.macro WAIT_FOR_PSYNC_EDGE_FAST
        push   {r9-r10}
        ldr    r10, =GPU_workspace
        ldmia  r10, {r8-r9}         //r10 is now GPU_workspace
        tst    r8, #0x00000001      //first or second sample?
        movne  r8, r8, lsr #16
        beq    read_registers\@
        b      got_sample\@
        .ltorg
read_registers\@:
    //    tst    r3, #BIT_NO_SKIP_HSYNC
    //    beq    got_sample\@
wait\@:
        ldr    r8, [r4, r9]
        eor    r8, r3
        tst    r8, #PSYNC_MASK
        tsteq  r8, #0x80000000
        beq    wait\@
        eor    r8, r3

        add    r9, r9, #4
        cmp    r9, #(GPU_SYNC_offset - GPU_DATA_0_offset)
        addeq  r9, r9, #4
        cmp    r9, #GPU_DATA_5_offset
        moveq  r9, #0
        eoreq  r3, r3, #PSYNC_MASK

got_sample\@:
        stmia  r10, {r8,r9}
        bic    r8, r8, #0x01000000
        tst    r8, #0x4000
        orrne  r8, r8, #0x01000000

        pop    {r9-r10}
.endm

.macro WAIT_FOR_PSYNC_EDGE
        WAIT_FOR_PSYNC_EDGE_FAST
.endm

.macro  SETUP_GPU_CAPTURE
        ldr    r10, =GPU_COMMAND
capturebusy\@:
        ldr    r8, [r10]
        cmp    r8, #0
        bne    capturebusy\@
        //zero out ram copy and GPU data registers (r8 is already zero)
        mov    r14, #GPU_DATA_5_offset
clear_regs\@:
        str    r8, [r10, r14]
        subs   r14, r14, #4
        bpl    clear_regs\@
        ldr    r10, =GPU_workspace
        str    r8, [r10]
        str    r8, [r10, #4]
.endm

.macro  SKIP_PSYNC_COMMON_NO_OLD_CPLD
        SETUP_GPU_CAPTURE
        WAIT_FOR_CSYNC_0_FAST_SKIP_HSYNC
        bic   r3, r3, #PSYNC_MASK    // wait for zero after CSYNC
        READ_CYCLE_COUNTER r10
        push  {r10}                 //save leading edge timestamp

        tst    r3, #BIT_HSYNC_EDGE    // if leading edge then don't wait for end of hsync (means scroll detection won't work)
        bne    do_skip_psync_no_old\@
        pop   {r10}
        mov   r6, r9, lsr #16        //HSYNC_SCROLL_HI
        bic   r9, r9, #0xff000000
        bic   r9, r9, #0x00ff0000    //HSYNC_SCROLL_LO
        // Wait for the end of hsync
        WAIT_FOR_CSYNC_1_FAST
        READ_CYCLE_COUNTER r14
        push  {r14}                 //save trailing edge timestamp
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
        bic    r3, #BIT_INHIBIT_MODE_DETECT
        // new CPLD code only (not called from CPLD v1 & v2)
        mov    r8, r7
        cmp    r10, r6              //HSYNC_SCROLL_HI
        addlt  r8, r8, #1
        orrgt  r3, r3, #BIT_INHIBIT_MODE_DETECT
        cmp    r10, r9              //HSYNC_SCROLL_LO
        addlt  r8, r8, #1
        orrlt  r3, r3, #BIT_INHIBIT_MODE_DETECT
        tst    r3, #BIT_NO_H_SCROLL
        moveq  r7, r8                      // only allow fine sideways scrolling in bbc / electron mode (causes timing issues in ega mode)
        // Skip the configured number of psync edges (modes 0..6: edges every 250ns, mode 7: edges ever 333ns)
do_skip_psync_no_old\@:
.endm

.macro  SKIP_PSYNC_NO_OLD_CPLD
        SKIP_PSYNC_COMMON_NO_OLD_CPLD
        ldr    r4, =GPU_DATA_0
        add    r8, r7, r1
        add    r8, r8, #1
        mov    r8, r8, lsr #1
        str    r8, [r4, #(GPU_COMMAND_offset - GPU_DATA_0_offset)]  //command register
skip_psync_no_old_loop\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        subs   r7, r7, #1
        bne    skip_psync_no_old_loop\@
.endm

.macro  SKIP_PSYNC
        SKIP_PSYNC_NO_OLD_CPLD
.endm

.macro  SKIP_PSYNC_SIMPLE_FAST
        SETUP_GPU_CAPTURE
        ldr    r8, =param_delay
        ldr    r8, [r8]
        add    r7, r7, r8
        ldr    r4, =GPU_DATA_0
        add    r8, r7, r1
        add    r8, r8, #1
        mov    r8, r8, lsr #1
        ldr    r9, =param_sync_edge
        ldr    r9, [r9]
        tst    r3, #BIT_NO_SKIP_HSYNC
        orrne  r8, r8, #0x00008000         //flag sync command
        orrne  r8, r9, lsl #16             //or in sync command
        str    r8, [r4, #(GPU_COMMAND_offset - GPU_DATA_0_offset)]  //command register
        beq    skip_psync_simple_fast\@
wait_for_simple_sync\@:
        ldr    r8, [r4, #(GPU_SYNC_offset - GPU_DATA_0_offset)]     //sync register
        tst    r8, #1
        beq    wait_for_simple_sync\@
skip_psync_simple_fast\@:
        READ_CYCLE_COUNTER r10
        bic   r3, r3, #PSYNC_MASK    // wait for zero after CSYNC
skip_psync_loop_simple_fast_loop\@:
        WAIT_FOR_PSYNC_EDGE_FAST           // wait for next edge of psync
        subs   r7, r7, #1
        bne    skip_psync_loop_simple_fast_loop\@
        push  {r10}
.endm

//**********************GPU CAPTURE END**********************

#endif

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

.macro CAPTURE_LOW_BITS_NORMAL reg
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4
        // Pixel 1 in GPIO  7.. 5 ->  3.. 0
        // Pixel 2 in GPIO 10.. 8 -> 15..12
        // Pixel 3 in GPIO 13..11 -> 11.. 8

        and    r9, r8, #(7 << PIXEL_BASE)
        and    r14, r8, #(7 << (PIXEL_BASE + 3))
        eor    r10, \reg, r9, lsl #(4 - PIXEL_BASE)
        eor    r10, r10, r14, lsr #(3 + PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r14, r8, #(7 << (PIXEL_BASE + 9))
        eor    r10, r10, r9, lsl #(6 - PIXEL_BASE)
        eor    r10, r10, r14, lsr #(1 + PIXEL_BASE)
.endm

.macro CAPTURE_HIGH_BITS_NORMAL reg
        // Pixel 4 in GPIO  4.. 2 -> 23..20
        // Pixel 5 in GPIO  7.. 5 -> 19..16
        // Pixel 6 in GPIO 10.. 8 -> 31..28
        // Pixel 7 in GPIO 13..11 -> 27..24

        and    r9, r8, #(7 << PIXEL_BASE)
        and    r14, r8, #(7 << (PIXEL_BASE + 3))
        eor    r10, r10, r9, lsl #(20 - PIXEL_BASE)
        eor    r10, r10, r14, lsl #(13 - PIXEL_BASE)

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r14, r8, #(7 << (PIXEL_BASE + 9))
        eor    r10, r10, r9, lsl #(22 - PIXEL_BASE)
        eor    \reg, r10, r14, lsl #(15 - PIXEL_BASE)
.endm

.macro CAPTURE_BITS_DOUBLE reg reg2
        // Pixel 0 in GPIO  4.. 2 ->  7.. 4 and  3.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15..12 and 11.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..20 and 19..16
        // Pixel 3 in GPIO 13..11 -> 31..28 and 27..24

        and    r9, r8, #(7 << PIXEL_BASE)
        and    r14, r8, #(7 << (PIXEL_BASE + 3))
        eor    r10, \reg, r9, lsl #(4 - PIXEL_BASE)
        eor    r10, r10, r14, lsl #(12 - (PIXEL_BASE + 3))

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r14, r8, #(7 << (PIXEL_BASE + 9))
        eor    r10, r10, r9, lsl #(20 - (PIXEL_BASE + 6))
        eor    r10, r10, r14, lsl #(28 - (PIXEL_BASE + 9))

        // Pixel double
        orr    \reg2, r10, r10, lsr #4
.endm

.macro CAPTURE_0_BITS_WIDE reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 4
        // Pixel 1 in GPIO 13.. 8 ->  3.. 0

        and    r9, r8, #(0x07 << PIXEL_BASE)
        and    r14, r8, #(0x07 << (PIXEL_BASE + 6))
        eor    r10, \reg, r9, lsl #(4 - PIXEL_BASE)
        eor    r10, r10, r14, lsr #(6 + PIXEL_BASE)
.endm

.macro CAPTURE_1_BITS_WIDE
        // Pixel 0 in GPIO  7.. 2 ->  15.. 12
        // Pixel 1 in GPIO 13.. 8 ->  11.. 8

        and    r9, r8, #(0x07 << (PIXEL_BASE))
        and    r14, r8, #(0x07 << (PIXEL_BASE + 6))
        eor    r10, r10, r9, lsl #(12 - PIXEL_BASE)
        eor    r10, r10, r14, lsr #(2 - PIXEL_BASE)
.endm

.macro CAPTURE_2_BITS_WIDE
        // Pixel 4 in GPIO  7.. 2 -> 23..20
        // Pixel 5 in GPIO 13.. 8 -> 19..16
        and    r9, r8, #(0x07 << PIXEL_BASE)
        and    r14, r8, #(0x07 << (PIXEL_BASE + 6))
        eor    r10, r10, r9, lsl #(20 - PIXEL_BASE)
        eor    r10, r10, r14, lsl #(10 - PIXEL_BASE)
.endm

.macro CAPTURE_3_BITS_WIDE reg
        // Pixel 6 in GPIO 7.. 2 -> 31..28
        // Pixel 7 in GPIO 13..8 -> 27..24
        and    r9, r8, #(0x07 << PIXEL_BASE)
        and    r14, r8, #(0x07 << (PIXEL_BASE + 6))
        eor    r10, r10, r9, lsl #(28 - PIXEL_BASE)
        eor    \reg, r10, r14, lsl #(18 - PIXEL_BASE)
.endm

.macro CAPTURE_LOW_BITS_DOUBLE_WIDE reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 4
        // Pixel 1 in GPIO 13.. 8 ->  15.. 12

        and    r9, r8, #(0x07 << PIXEL_BASE)
        and    r14, r8, #(0x07 << (PIXEL_BASE + 6))
        eor    r10, \reg, r9, lsl #(4 - PIXEL_BASE)
        eor    r10, r10, r14, lsl #(2 + PIXEL_BASE)
.endm

.macro CAPTURE_HIGH_BITS_DOUBLE_WIDE reg
        // Pixel 2 in GPIO  7.. 2 -> 23..20
        // Pixel 3 in GPIO 13.. 8 -> 31..28
        and    r9, r8, #(0x07 << PIXEL_BASE)
        and    r14, r8, #(0x07 << (PIXEL_BASE + 6))
        eor    r10, r10, r9, lsl #(20 - PIXEL_BASE)
        eor    r10, r10, r14, lsl #(22 - PIXEL_BASE)

        // Pixel double
        orr    \reg, r10, r10, lsr #4
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

.macro CAPTURE_BITS_8BPP_NORMAL reg reg2
        // Pixel 0 in GPIO  4.. 2 ->  7.. 0
        // Pixel 1 in GPIO  7.. 5 -> 15.. 8
        // Pixel 2 in GPIO 10.. 8 -> 23..16
        // Pixel 3 in GPIO 13..11 -> 31..24

        and    r9, r8, #(7 << PIXEL_BASE)
        and    r14, r8, #(7 << (PIXEL_BASE + 3))
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    r10, r10, r14, lsl #(8 - (PIXEL_BASE + 3))

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r14, r8, #(7 << (PIXEL_BASE + 9))
        eor    r10, r10, r9, lsl #(16 - (PIXEL_BASE + 6))
        eor    \reg2, r10, r14, lsl #(24 - (PIXEL_BASE + 9))
.endm

.macro CAPTURE_LOW_BITS_DOUBLE_8BPP reg reg2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        and    r9, r8, #(7 << PIXEL_BASE)
        and    r14, r8, #(7 << (PIXEL_BASE + 3))
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    r10, r10, r14, lsl #(16 - (PIXEL_BASE + 3))
        // Pixel double
        orr    \reg2, r10, r10, lsl #8
.endm

.macro CAPTURE_HIGH_BITS_DOUBLE_8BPP reg reg2
        // Pixel 2 in GPIO  7.. 2 ->  7.. 0
        // Pixel 3 in GPIO 13.. 8 -> 23..16

        and    r9, r8, #(7 << (PIXEL_BASE + 6))
        and    r14, r8, #(7 << (PIXEL_BASE + 9))
        eor    r10, \reg, r9, lsr #(PIXEL_BASE + 6)
        eor    r10, r10, r14, lsl #(16 - (PIXEL_BASE + 9))
        // Pixel double
        orr    \reg2, r10, r10, lsl #8
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

.macro CAPTURE_BITS_DOUBLE_8BPP_WIDE reg reg2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        and    r9, r8, #(0x3f << PIXEL_BASE)
        and    r14, r8, #(0x3f << (PIXEL_BASE + 6))
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    r10, r10, r14, lsl #(16 - (PIXEL_BASE + 6))
        // Pixel double
        orr    \reg2, r10, r10, lsl #8
.endm

.macro SETUP_EIGHT_BITS_MASK_R14
        tst    r3, #BIT_OSD
        movne  r14, #0x7f
        moveq  r14, #0xff
        mov    r14, r14, lsl #3
.endm

// This extracts 8 bits from 9 or 12 bpp capture to be written to an 8 bit buffer
// the bits are in the order R3,R2,R1,R0,G3,G2,G1,G0,B3,B2,B1,B0 so the correct 8 bits have to be extracted
// which would be B1,R1,B2,G2,R2,B3,G3,R3 however there isn't enough time to rearrange all bits
// so just mask out the unwanted ones and move the wanted ones in their place.
// This means the bit order is different from 3 bpp and 6 bpp but that can be fixed by reordering the bits
// in the palette lookup table


.macro  BIT_SHIFT_EIGHT_BITS
        and    r9, r8, #(0xCC << PIXEL_BASE)  // extract 0,G3,G2,0,0,B3,B2,0,x,x,x (shifted left by PIXEL_BASE + 1 to put red lsb rather than green msb in top bit)
        tst    r8, #(0x02 << PIXEL_BASE)      // move B1
        orrne  r9, r9, #(0x100 << PIXEL_BASE)
        tst    r8, #(0x800 << PIXEL_BASE)     // move R3
        orrne  r9, r9, #(0x20 << PIXEL_BASE)
        tst    r8, #(0x400 << PIXEL_BASE)     // move R2
        orrne  r9, r9, #(0x10 << PIXEL_BASE)
        tst    r8, #(0x200 << PIXEL_BASE)     // move R1
        orrne  r9, r9, #(0x02 << PIXEL_BASE)  // order is now B1,G3,G2,R3,R2,B3,B2,R1,x,x,x
        and    r9, r9, r14                    // mask out top bit if OSD is on (B1)
        tst    r3, #BITDUP_ENABLE_FFOSD       // code for FFOSD in 8bpp mode
        tstne  r8, #MUX_MASK
        orrne  r3, r3, #BITDUP_FFOSD_DETECTED
        orrne  r9, r9, #(0x100 << PIXEL_BASE)
.endm

.macro CAPTURE_EIGHT_BITS_8BPP_0 reg
        // Pixel 0 in GPIO ->  7.. 0

        BIT_SHIFT_EIGHT_BITS
        eor    r10, \reg, r9, lsr #(PIXEL_BASE + 1)
.endm

.macro CAPTURE_EIGHT_BITS_8BPP_1
        // Pixel 0 in GPIO ->  15.. 8

        BIT_SHIFT_EIGHT_BITS
        eor    r10, r10, r9, lsl #(7 - PIXEL_BASE)
.endm

.macro CAPTURE_EIGHT_BITS_8BPP_2
        // Pixel 0 in GPIO ->  23.. 16

        BIT_SHIFT_EIGHT_BITS
        eor    r10, r10, r9, lsl #(15 - PIXEL_BASE)
.endm

.macro CAPTURE_EIGHT_BITS_8BPP_3 reg
        // Pixel 0 in GPIO ->  31.. 24

        BIT_SHIFT_EIGHT_BITS
        eor    \reg, r10, r9, lsl #(23 - PIXEL_BASE)
.endm

.macro CAPTURE_EIGHT_BITS_DOUBLE_8BPP_LO reg
        // Pixel 0 in GPIO ->  7.. 0

        BIT_SHIFT_EIGHT_BITS
        eor    r10, \reg, r9, lsr #(PIXEL_BASE + 1)
.endm

.macro CAPTURE_EIGHT_BITS_DOUBLE_8BPP_HI reg
        // Pixel 0 in GPIO ->  23.. 16
        BIT_SHIFT_EIGHT_BITS
        eor    r10, r10, r9, lsl #(15 - PIXEL_BASE)
        // Pixel double
        orr    \reg, r10, r10, lsl #8
.endm


.macro SETUP_NINELO_BITS_MASK_R14
        mov     r14, #0x77 << PIXEL_BASE
        orr     r14, r14, #0x700 << PIXEL_BASE
.endm

.macro CAPTURE_NINELO_BITS_16BPP_LO reg
        // Pixel in GPIO 13.. 2 -> 15.. 0
        and    r9, r8, r14
        bic    r8, r8, r14, lsr #1
        eor    r10, \reg, r9, lsr #(PIXEL_BASE - 1)
        and    r8, r8, r14
        eor    r10, r10, r8, lsr #(PIXEL_BASE + 2)
.endm

.macro CAPTURE_NINELO_BITS_16BPP_HI reg
        // Pixel in GPIO 13.. 2 -> 31.. 16
        and    r9, r8, r14
        bic    r8, r8, r14, lsr #1
        eor    r10, r10, r9, lsl #(16 - (PIXEL_BASE - 1))
        and    r8, r8, r14
        eor    \reg, r10, r8, lsl #(16 - (PIXEL_BASE + 2))
.endm

.macro CAPTURE_NINELO_BITS_DOUBLE_16BPP reg reg2
        // Pixel in GPIO 13.. 2 -> 15.. 0
        and    r9, r8, r14
        bic    r8, r8, r14, lsr #1
        eor    r10, \reg, r9, lsr #(PIXEL_BASE - 1)
        and    r8, r8, r14
        eor    r10, r10, r8, lsr #(PIXEL_BASE + 2)
        eor    \reg2, r10, r10, lsl #16
.endm

.macro SETUP_NINEHI_BITS_MASK_R14
        mov     r14, #0xee << PIXEL_BASE
        orr     r14, r14, #0xe00 << PIXEL_BASE
.endm

.macro CAPTURE_NINEHI_BITS_16BPP_LO reg
        // Pixel in GPIO 13.. 2 -> 15.. 0
        and    r9, r8, r14
        bic    r8, r8, r14, lsr #1
        eor    r10, \reg, r9, lsr #PIXEL_BASE
        and    r8, r8, r14
        eor    r10, r10, r8, lsr #(PIXEL_BASE + 3)
.endm

.macro CAPTURE_NINEHI_BITS_16BPP_HI reg
        // Pixel in GPIO 13.. 2 -> 31.. 16
        and    r9, r8, r14
        bic    r8, r8, r14, lsr #1
        eor    r10, r10, r9, lsl #(16 - PIXEL_BASE)
        and    r8, r8, r14
        eor    \reg, r10, r8, lsl #(16 - (PIXEL_BASE + 3))
.endm

.macro CAPTURE_NINEHI_BITS_DOUBLE_16BPP reg reg2
        // Pixel in GPIO 13.. 2 -> 15.. 0
        and    r9, r8, r14
        bic    r8, r8, r14, lsr #1
        eor    r10, \reg, r9, lsr #PIXEL_BASE
        and    r8, r8, r14
        eor    r10, r10, r8, lsr #(PIXEL_BASE + 3)
        eor    \reg2, r10, r10, lsl #16
.endm

.macro SETUP_TWELVE_BITS_MASK_R14
        mov     r14, #0xff << PIXEL_BASE
        orr     r14, r14, #0xf00 << PIXEL_BASE
.endm

.macro CAPTURE_TWELVE_BITS_16BPP_LO reg
        // Pixel in GPIO 13.. 2 -> 15.. 0
        and    r9, r8, r14
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
.endm

.macro CAPTURE_TWELVE_BITS_16BPP_HI reg
        // Pixel in GPIO 13.. 2 -> 31.. 16
        and    r9, r8, r14
        eor    \reg, r10, r9, lsl #(16 - PIXEL_BASE)
.endm

.macro TEST_CAPTURE_TWELVE_BITS_16BPP_LO reg
        // Pixel in GPIO 13.. 2 -> 15.. 0
        and    r9, r8, r14
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        tst    r8, #MUX_MASK
        orrne  r3, #BITDUP_FFOSD_DETECTED
.endm

.macro TEST_CAPTURE_TWELVE_BITS_16BPP_HI reg
        // Pixel in GPIO 13.. 2 -> 31.. 16
        and    r9, r8, r14
        eor    \reg, r10, r9, lsl #(16 - PIXEL_BASE)
        tst    r8, #MUX_MASK
        orrne  r3, #BITDUP_FFOSD_DETECTED
.endm

.macro OSD_CAPTURE_TWELVE_BITS_16BPP_LO reg
        // Pixel in GPIO 13.. 2 -> 15.. 0
        and    r9, r8, r14
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    r9, r9, #(GREY_PIXELS & 0x0ff) << PIXEL_BASE
        eors   r9, r9, #(GREY_PIXELS & 0xf00) << PIXEL_BASE
        bicne  r3, #BITDUP_LINE_CONDITION_DETECTED
        tst    r8, #MUX_MASK
        orrne  r3, #BITDUP_FFOSD_DETECTED
        orrne  r10, #0xff00
        orrne  r10, #0x00ff
.endm

.macro OSD_CAPTURE_TWELVE_BITS_16BPP_HI reg
        // Pixel in GPIO 13.. 2 -> 31.. 16
        and    r9, r8, r14
        eor    \reg, r10, r9, lsl #(16 - PIXEL_BASE)
        eor    r9, r9, #(GREY_PIXELS & 0x0ff) << PIXEL_BASE
        eors   r9, r9, #(GREY_PIXELS & 0xf00) << PIXEL_BASE
        bicne  r3, #BITDUP_LINE_CONDITION_DETECTED
        tst    r8, #MUX_MASK
        orrne  r3, #BITDUP_FFOSD_DETECTED
        orrne  \reg, \reg, #(0xff000000)
        orrne  \reg, \reg, #(0x00ff0000)
.endm

.macro CAPTURE_TWELVE_BITS_DOUBLE_16BPP reg reg2
        // Pixel in GPIO 13.. 2 -> 15.. 0
        and    r9, r8, r14
        eor    r10, \reg, r9, lsr #(PIXEL_BASE)
        eor    \reg2, r10, r10, lsl #16
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

.macro  SETUP_VSYNC_DEBUG_R11
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x11111111
        moveq   r11, #0
        tst     r3, #BIT_DEBUG
        eorne   r11, r11, #0x50         //magenta in leftmost
        eorne   r11, r11, #0x02000000   //green in rightmost
.endm

.macro  SETUP_VSYNC_DEBUG_R11_DOUBLE
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x10101010
        moveq   r11, #0
        tst     r3, #BIT_DEBUG
        eorne   r11, r11, #0x50         //magenta in leftmost
        eorne   r11, r11, #0x20000000   //green in rightmost << 4
.endm

.macro  WRITE_R7_IF_LAST
        cmp     r1, #1
        stmeqia r0, {r7}
        tsteq   r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE
        ldreq   r8, =0x88888888
        orreq   r7, r7, r8
        cmp     r1, #1
        tsteq   r3,  #BIT_NO_LINE_DOUBLE
        subeq   r0, r0, r2
        stmeqia r0, {r7}
.endm

.macro  WRITE_R7_R10
        stmia   r0, {r7, r10}
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE
        ldreq   r8, =0x88888888
        orreq   r7, r7, r8
        orreq   r10, r10, r8
        tst     r3,  #BIT_NO_LINE_DOUBLE
        subeq   r0, r0, r2
        stmeqia r0, {r7, r10}
        addeq   r0, r0, r2
        add     r0, r0, #8
.endm

.macro  SETUP_VSYNC_DEBUG_R11_R12
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x01010101
        moveq   r11, #0
        movne   r12, r11
        moveq   r12, #0
        tst     r3, #BIT_DEBUG
        eorne   r11, r11, #0x05           //magenta in leftmost
        eorne   r12, r12, #0x02000000     //green in rightmost
.endm

.macro  SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r11, =0x00010001
        moveq   r11, #0
        movne   r12, r11
        moveq   r12, #0
        tst     r3, #BIT_DEBUG
        eorne   r11, r11, #0x05           //magenta in leftmost
        eorne   r12, r12, #0x00020000     //green in rightmost >> 8
.endm

.macro  SETUP_VSYNC_DEBUG_16BPP_R11
        tst     r3, #BIT_OSD
        ldreq   r11, =#0xf000f000
        ldrne   r11, =#0x70007000
        tst     r3, #BIT_VSYNC_MARKER
        eorne   r11, r11, #0x0f000000
        eorne   r11, r11, #0x00000f00
        tst     r3, #BIT_NO_SCANLINES | BIT_INTERLACED_VIDEO
        ldreq   r12, =param_intensity
        ldreq   r12, [r12]
.endm

.macro  WRITE_R5_R6
        stmia   r0, {r5, r6}
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE | BIT_INTERLACED_VIDEO
        ldreq   r8, =0x80808080
        orreq   r5, r5, r8
        orreq   r6, r6, r8
        tst     r3, #BIT_NO_LINE_DOUBLE
        subeq   r0, r0, r2
        stmeqia r0, {r5, r6}
        addeq   r0, r0, r2
        add     r0, r0, #8
.endm

.macro  WRITE_R5_R6_IF_LAST_16BPP
        cmp     r1, #1
        stmeqia r0, {r5, r6}
        tsteq   r3, #BIT_NO_SCANLINES | BIT_INTERLACED_VIDEO
        eoreq   r5, r5, r12
        eoreq   r6, r6, r12
        cmp     r1, #1
        tsteq   r3, #BIT_NO_LINE_DOUBLE
        subeq   r0, r0, r2
        stmeqia r0, {r5, r6}
.endm

.macro  WRITE_R5_R6_R7_R10_16BPP
        stmia   r0, {r5, r6, r7, r10}
        tst     r3, #BIT_NO_SCANLINES | BIT_INTERLACED_VIDEO
        eoreq   r5, r5, r12
        eoreq   r6, r6, r12
        eoreq   r7, r7, r12
        eoreq   r10, r10, r12

        tst     r3, #BIT_NO_LINE_DOUBLE
        subeq   r0, r0, r2
        stmeqia r0, {r5, r6, r7, r10}
        addeq   r0, r0, r2
        add     r0, r0, #16
.endm


.macro  WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        stmeqia r0, {r5, r6}
        tsteq   r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE | BIT_INTERLACED_VIDEO
        ldreq   r8, =0x80808080
        orreq   r5, r5, r8
        orreq   r6, r6, r8
        cmp     r1, #1
        tsteq   r3, #BIT_NO_LINE_DOUBLE
        subeq   r0, r0, r2
        stmeqia r0, {r5, r6}
.endm

.macro  WRITE_R5_R6_R7_R10
        stmia   r0, {r5, r6, r7, r10}
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE | BIT_INTERLACED_VIDEO
        ldreq   r8, =0x80808080
        orreq   r5, r5, r8
        orreq   r6, r6, r8
        orreq   r7, r7, r8
        orreq   r10, r10, r8
        tst     r3, #BIT_NO_LINE_DOUBLE
        subeq   r0, r0, r2
        stmeqia r0, {r5, r6, r7, r10}
        addeq   r0, r0, r2
        add     r0, r0, #16
.endm




.macro WRITE_WORD_FAST
        eor    r10, r10, r6     //eor in vsync and debug
        str    r10, [r0]
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE | BIT_INTERLACED_VIDEO
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
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE | BIT_INTERLACED_VIDEO
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
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE | BIT_INTERLACED_VIDEO
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
        tst     r3, #BIT_NO_SCANLINES | BIT_OSD | BIT_NO_LINE_DOUBLE | BIT_INTERLACED_VIDEO
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
        mov     r1, #24
        mov     r2, #0
        orr     r3, r3, #BIT_VSYNC_MARKER    // ensure that constants are in data cache
        bic     r3, r3, #BIT_NO_SKIP_HSYNC
        mov     r5, #1
        mov     r6, #0
        mov     r7, #8
        mov     r8, #256
        mov     r9, #0  //force skip of wait for csync 0
.endm

// ======================================================================
// Macros
// ======================================================================


#if defined(RPI2) || defined(RPI3) || defined(RPI4)
//
#else
// Data Synchronisation Barrier
.macro DSB
        mcr    p15, 0, r0, c7, c10, 4
.endm
// Data Memory Barrier
.macro DMB
        mcr    p15, 0, r0, c7, c10, 5
.endm
#endif


.macro READ_CYCLE_COUNTER reg
#if defined(RPI2) || defined(RPI3) || defined(RPI4)
        mrc    p15, 0, \reg, c9, c13, 0
#else
        mrc    p15, 0, \reg, c15, c12, 1
#endif
.endm

.macro CLEAR_VSYNC
        // Clear the VSYNC interrupt
        ldr    r0, =SMICTRL
        bic    r3, r3, #BIT_VSYNC_MARKER
        mov    r10, #0
        str    r10, [r0]
        // Don't proceed until this write is complete
        DSB
.endm

.macro SHOW_VSYNC
        bic    r3, r3, #BIT_VSYNC_MARKER
    //    tst    r3, #(BIT_PROBE)
    //    bne    novsync\@
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
        tst    r3, #(BIT_INTERLACED_VIDEO | BIT_PROBE)
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
        //enters with sw1_power_up in r9
        ldr    r6, =\counter    // Load the counter value
        ldr    r5, [r6]
        cmp    r9, #0
        movne  r5, #0
        tst    r8, #\mask      // Is the button pressed (active low)?
        movne  r5, #0          // Clear the counter
        addeq  r5, r5, #1      // If pressed, then increment the counter valye
        str    r5, [r6]        // And always write back the counter value

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
