#include "rpi-base.h"
#include "defs.h"
#include "macros.S"

.text

.global a2024_capture_frame

.extern a2024_control_line
.extern a2024_decode_control_fast
.extern a2024_capture_result
.extern a2024_resume_active
.extern capture_line_a2024_8bpp
.extern capture_line_default_twelvebits_16bpp
.extern param_fb_pitch
.extern param_fb_height
.extern param_nlines
.extern param_h_offset
.extern param_framebuffer0
.extern capture_address
.extern dummyscreen
.extern clear_vsync
.extern show_vsync
.extern hsync_scroll
.extern total_lines

/*
 * A2024 fields use an in-band command line followed by one panel of
 * framebuffer data.  Keep this path outside the normal capture engine so
 * ordinary PAL retains its established instruction and cache layout.
 */
        .align 6
a2024_capture_frame:
        /*
         * Every panel updates framebuffer zero.  The fields must accumulate
         * in one buffer instead of following the normal buffer rotation.
         * Reload the base because the common path may already have applied
         * interlace or double-height adjustment.
         */
        bic     r3, r3, #(MASK_LAST_BUFFER | MASK_CURR_BUFFER)
        bic     r3, r3, #MASK_NBUFFERS
        ldr     r11, =param_framebuffer0
        ldr     r11, [r11]

        /*
         * The clean PAL separate-sync probe found the driver's constant
         * $f00 pre-command background after 22 lines.  Test the next raster,
         * offset 23, for the command.  Keep the original NTSC offset until
         * NTSC has an equivalent hardware capture.  The supplied profiles
         * use 200 and 256 active transfer lines respectively.
         */
        ldr     r6, =param_nlines
        ldr     r6, [r6]
        cmp     r6, #200
        moveq   r5, #34
        movne   r5, #38
        add     r8, r5, r6
        ldr     r10, =total_lines
        str     r8, [r10]
        mov     r5, r8
        cmp     r6, #200
        moveq   r6, #11
        movne   r6, #23

        bl      clear_vsync
a2024_control_skip:
        bl      show_vsync
        WAIT_FOR_CSYNC_0_LONG
        WAIT_FOR_CSYNC_1_LONG
        subs    r5, r5, #1
        subs    r6, r6, #1
        bne     a2024_control_skip

        /*
         * Preserve the live frame state around the raw 12-bit command
         * capture.  The stack is eight-byte aligned here, so the C decoder
         * is called with its required AAPCS alignment.
         */
        push    {r1-r12}
        ldr     r0, =a2024_control_line
        mov     r1, #448
        ldr     r2, =param_fb_pitch
        ldr     r2, [r2]
        ldr     r7, =param_h_offset
        ldr     r7, [r7]
        add     r7, r7, #160
        ldr     r9, =hsync_scroll
        ldr     r9, [r9]
        orr     r3, r3, #BIT_NO_LINE_DOUBLE
        orr     r3, r3, #BIT_NO_SKIP_HSYNC
        bic     r3, r3, #(BIT_OSD | BITDUP_ENABLE_GREY_DETECT)
        bic     r3, r3, #BITDUP_ENABLE_FFOSD
        bic     r3, r3, #(BITDUP_RGB_INVERT | BITDUP_Y_INVERT)
        bl      capture_line_default_twelvebits_16bpp
        bl      a2024_decode_control_fast
        ldr     r1, =a2024_capture_result
        str     r0, [r1]
        pop     {r1-r12}
        sub     r5, r5, #1

        /*
         * Data begins 23 NTSC or 15 PAL lines after the command.  Capturing
         * the command consumes one transition, leaving 22 or 14 to skip.
         */
        ldr     r6, =param_nlines
        ldr     r6, [r6]
        cmp     r6, #200
        moveq   r6, #22
        movne   r6, #14
a2024_data_skip:
        bl      show_vsync
        WAIT_FOR_CSYNC_0_LONG
        WAIT_FOR_CSYNC_1_LONG
        subs    r5, r5, #1
        subs    r6, r6, #1
        bne     a2024_data_skip

        ldr     r8, =capture_address
        ldr     r9, =capture_line_a2024_8bpp
        str     r9, [r8]

        ldr     r0, =a2024_capture_result
        ldr     r0, [r0]
        cmp     r0, #0
        bmi     a2024_discard_panel

        /*
         * Word one selects the panel.  Four-field mode uses two
         * 512-pixel columns; six-field mode uses three 336-pixel columns.
         */
        and     r8, r0, #3
        tst     r0, #16
        moveq   r9, #512
        movne   r9, #336
        mul     r8, r9, r8
        add     r11, r11, r8

        tst     r0, #4
        beq     a2024_resume_active
        ldr     r8, =param_fb_height
        ldr     r8, [r8]
        mov     r8, r8, lsr #1
        ldr     r9, =param_fb_pitch
        ldr     r9, [r9]
        mul     r8, r9, r8
        add     r11, r11, r8
        b       a2024_resume_active

a2024_discard_panel:
        /*
         * A bad command must not touch the displayed framebuffer.  Still
         * consume the data raster through the normal A2024 line routine so
         * field timing remains identical to a valid transfer.
         */
        ldr     r11, =dummyscreen
        b       a2024_resume_active

        .ltorg
