// capture_line_mc6847_8bpp.S - MC6847 LumaCode capture with NTSC artifact colours
//
// The MC6847digitizer sends one 4-bit LumaCode colour per 6847 dot. On an
// NTSC composite monitor two adjacent dots form one colour clock, so in
// PMODE 4 (RG6) a pair made of one black and one green/buff dot shows as an
// artifact colour. Only RG6 ever puts two different colours inside one dot
// pair (all other modes change colour on even dot boundaries), so applying
// a per-pair lookup table to every pair gives artifacts in RG6 and leaves
// everything else untouched - no mode detection needed.
//
// This file provides capture_line_c64lc6847_double_8bpp. The capture tables
// in rgb_to_fb.S always name this routine for the C64 LumaCode double 8bpp
// slot; rgb_to_fb resolves it back to the plain
// capture_line_c64lc_sixbits_double_8bpp once per frame, before the line
// loop starts, whenever mc6847_artifact_enable is 0 - so this routine itself
// is only ever entered when artifacts are on, and never has to check the
// setting on every scanline (an earlier version did check it here on every
// line; a data cache miss on that check could delay the start of a line
// enough to catch the wrong PSYNC edge for that frame, which showed up as
// occasional flicker). The lookup table maps (first dot << 4 | second dot)
// to the 32 bit frame buffer word for the pair (double width: 4 pixels) and
// is filled in by mc6847_artifact_update() in rgb_to_hdmi.c.

#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_c64lc6847_double_8bpp
.global mc6847_artifact_table
.global mc6847_artifact_enable
.global mc6847_table_active

// Same as CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC: first dot of the pair in
// bits 3..0 of r10
.macro CAPTURE_LOW_BITS_DOUBLE_8BPP_6847 reg
        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000008                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000004                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second sample
        eorne  r10, r10, #0x000002                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second sample
        eorne  r10, r10, #0x000001                //bit 0 of luma
.endm

// Second dot of the pair in bits 19..16 of r10, then the whole word comes
// from the pair table (r14) instead of being duplicated.
.macro CAPTURE_HIGH_BITS_DOUBLE_8BPP_6847 reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x080000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x040000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second sample
        eorne  r10, r10, #0x020000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second sample
        eorne  r10, r10, #0x010000                //bit 0 of luma

        and    r9, r10, #0x0F                     //first dot
        and    r10, r10, #0x0F0000                //second dot
        orr    r9, r9, r10, lsr #12               //pair index = first | second << 4
        ldr    \reg, [r14, r9, lsl #2]
.endm

        .align 6
        b       preload_capture_line_c64lc6847_double_8bpp
capture_line_c64lc6847_double_8bpp:
        push    {lr}
        mov     r1, r1, lsl #1
        mov     r11, #0                           // vsync marker / debug colours not supported here
        mov     r12, #0
        SKIP_PSYNC_NO_OLD_CPLD
        ldr     r14, =mc6847_artifact_table
        mov     r1, r1, lsr #2
loop6d_8bpp_6847:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_6847 r11         // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_6847 r5         // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_6847 r12         // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_6847 r6         // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_6847 r11         // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_6847 r7         // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_6847 r12         // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_6847 r10        // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6d_8bpp_6847

        pop     {r0, pc}

preload_capture_line_c64lc6847_double_8bpp:
        // called once per frame, before the line loop: tell the CoCoVGA hook
        // that this frame goes through the pair table (it clears the flag
        // after each frame; its text colours need the table)
        ldr     r0, =mc6847_table_active
        mov     r1, #1
        str     r1, [r0]
        // pull the pair table into the data cache, then run the capture code
        // once with dummy parameters (as the other preload routines do)
        ldr     r0, =mc6847_artifact_table
        mov     r1, #256
preload6847_loop:
        ldr     r2, [r0], #4
        subs    r1, r1, #1
        bne     preload6847_loop
        SETUP_DUMMY_PARAMETERS
        b       capture_line_c64lc6847_double_8bpp

        .ltorg

        .align 6
mc6847_artifact_table:
        .space (256*4), 0
mc6847_artifact_enable:
        .word 0
// set to 1 by the preload above (once per frame when this routine
// captures), cleared by cocovga_frame_hook after each frame
mc6847_table_active:
        .word 0

        .ltorg
