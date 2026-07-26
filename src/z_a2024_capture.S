#include "rpi-base.h"
#include "defs.h"
#include "macros.S"

.text

.global a2024_capture_frame
.global a2024_finish_frame
.global a2024_probe_frame

.extern a2024_control_line
.extern a2024_decode_control_fast
.extern a2024_decode_control_nominal
.extern a2024_capture_result
.extern a2024_resume_active
.extern a2024_skip_all_lines_resume
.extern capture_line_a2024_8bpp
.extern capture_line_default_twelvebits_16bpp
.extern param_autoswitch
.extern param_fb_pitch
.extern param_fb_height
.extern param_nlines
.extern param_h_offset
.extern param_sync_type
.extern param_detected_sync_type
.extern param_timingset
.extern param_framebuffer0
.extern capture_address
.extern dummyscreen
.extern clear_vsync
.extern rgb_to_fb_exit
.extern show_vsync
.extern hsync_scroll
.extern hsync_measured
.extern total_lines

#define A2024_MATCH_LIMIT 2
#define A2024_MISS_LIMIT  4
#define A2024_PROBE_H_OFFSET 280

.global a2024_probe_samples
.global a2024_probe_invalid
.global a2024_probe_valid
.global a2024_probe_last_result
.global a2024_entry_events
.global a2024_release_events

/*
 * A2024 fields use an in-band command line followed by one panel of
 * framebuffer data.  Keep this path outside the normal capture engine so
 * ordinary PAL retains its established instruction and cache layout.
 */
        .align 6

/*
 * Probe the fixed A2024 command raster while an ordinary Amiga profile is
 * active.  The command is saved here but decoded only after the normal
 * active raster, avoiding variable work in the vertical blanking interval.
 */
a2024_probe_frame:
        ldr     r8, =param_autoswitch
        ldr     r8, [r8]
        cmp     r8, #AUTOSWITCH_A2024
        bne     clear_vsync

        ldr     r6, =param_nlines
        ldr     r6, [r6]
        cmp     r6, #240
        /*
         * Start with the manual A2024 offsets.  A normal separate-sync
         * interlaced waiter consumes one additional HSync before reaching
         * this dispatcher, so its command is one wait closer.
         */
        movlt   r6, #11
        movge   r6, #23
        cmp     r9, #VIDEO_INTERLACED
        bne     a2024_probe_offset_ready
        ldr     r8, =param_sync_type
        ldr     r8, [r8]
        ldr     r10, =param_detected_sync_type
        ldr     r10, [r10]
        tst     r10, #SYNC_BIT_MIXED_SYNC
        tsteq   r8, #SYNC_BIT_COMPOSITE_SYNC
        subeq   r6, r6, #1
a2024_probe_offset_ready:

        /*
         * Do not move the normal active raster if a custom profile starts
         * before the command line.  The supplied Amiga profiles leave more
         * than enough blank lines for both PAL and NTSC probes.
         */
        cmp     r5, r6
        ble     clear_vsync

        ldr     r8, =param_nlines
        ldr     r8, [r8]
        add     r8, r8, r5
        ldr     r10, =total_lines
        str     r8, [r10]
        mov     r5, r8

        bl      clear_vsync
        mov     r9, #0
a2024_probe_control_skip:
        ldr     r10, =hsync_measured
        str     r9, [r10]
        bl      show_vsync
        WAIT_FOR_CSYNC_0_LONG
        READ_CYCLE_COUNTER r12
        WAIT_FOR_CSYNC_1_LONG
        READ_CYCLE_COUNTER r10
        subs    r9, r10, r12
        rsbmi   r9, r9, #0
        subs    r5, r5, #1
        subs    r6, r6, #1
        bne     a2024_probe_control_skip

        /*
         * Preserve the normal capture state while saving the command.  The
         * twelve-word save retains the caller's eight-byte stack alignment.
         */
        bl      show_vsync
        push    {r1-r12}
        ldr     r0, =a2024_control_line
        mov     r1, #448
        ldr     r2, =param_fb_pitch
        ldr     r2, [r2]
        /*
         * Normal profiles move their capture offset when cropping for the
         * HDMI mode.  Start at the fixed wire position instead so the
         * hardware-confirmed first command centre remains at sample 39.
         */
        mov     r7, #A2024_PROBE_H_OFFSET
        ldr     r9, =hsync_scroll
        ldr     r9, [r9]
        orr     r3, r3, #BIT_NO_LINE_DOUBLE
        orr     r3, r3, #BIT_NO_SKIP_HSYNC
        bic     r3, r3, #(BIT_OSD | BITDUP_ENABLE_GREY_DETECT)
        bic     r3, r3, #BITDUP_ENABLE_FFOSD
        bic     r3, r3, #(BITDUP_RGB_INVERT | BITDUP_Y_INVERT)
        bic     r3, r3, #(BIT_VSYNC_MARKER | BIT_DEBUG)
        bl      capture_line_default_twelvebits_16bpp
        pop     {r1-r12}
        sub     r5, r5, #1

        ldr     r8, =a2024_probe_pending
        mov     r10, #1
        str     r10, [r8]

        /*
         * The command capture consumes one line.  Continue the original
         * total-lines countdown until it reaches the active line count, then
         * enter the unchanged active-line engine at the same source raster.
         */
        ldr     r6, =param_nlines
        ldr     r6, [r6]
a2024_probe_remaining_skip:
        ldr     r10, =hsync_measured
        str     r9, [r10]
        cmp     r5, r6
        ble     a2024_probe_resume_active
        bl      show_vsync
        WAIT_FOR_CSYNC_0_LONG
        READ_CYCLE_COUNTER r12
        WAIT_FOR_CSYNC_1_LONG
        READ_CYCLE_COUNTER r10
        subs    r9, r10, r12
        rsbmi   r9, r9, #0
        subs    r5, r5, #1
        b       a2024_probe_remaining_skip
a2024_probe_resume_active:
        b       a2024_resume_active

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
        bic     r3, r3, #(BIT_VSYNC_MARKER | BIT_DEBUG)
        bl      capture_line_default_twelvebits_16bpp
        bl      a2024_decode_control_fast
        ldr     r1, =a2024_capture_result
        str     r0, [r1]
        pop     {r1-r12}
        sub     r5, r5, #1

        ldr     r0, =a2024_capture_result
        ldr     r0, [r0]
        cmn     r0, #1
        beq     a2024_control_missed

        /*
         * A deferred result has a complete A2024 signature but needs a new
         * DPL lookup table.  Drop that field without treating it as mode
         * loss; the existing end-of-field decoder prepares the next one.
         */
        ldr     r8, =a2024_miss_count
        mov     r9, #0
        str     r9, [r8]
        b       a2024_control_checked

a2024_control_missed:
        ldr     r8, =param_autoswitch
        ldr     r8, [r8]
        cmp     r8, #AUTOSWITCH_A2024
        bne     a2024_control_checked

        ldr     r8, =a2024_miss_count
        ldr     r9, [r8]
        add     r9, r9, #1
        str     r9, [r8]
        cmp     r9, #A2024_MISS_LIMIT
        blt     a2024_control_checked

        mov     r9, #0
        str     r9, [r8]
        ldr     r8, =a2024_match_count
        str     r9, [r8]
        ldr     r8, =a2024_release_events
        ldr     r9, [r8]
        add     r9, r9, #1
        str     r9, [r8]
        pop     {r11}
        ldr     r0, =param_timingset
        ldr     r0, [r0]
        orr     r0, r0, #RET_A2024_CHANGED
        b       rgb_to_fb_exit

a2024_control_checked:

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

/*
 * Complete a pending normal-mode probe after its active raster.  The
 * displaced register save is also the stack frame expected by the common
 * post-capture path, so non-probe frames return there without any layout
 * change in rgb_to_fb.S.
 */
        .align 6
a2024_finish_frame:
        push    {r1-r12}
        ldr     r8, =a2024_probe_pending
        ldr     r9, [r8]
        cmp     r9, #0
        beq     a2024_finish_without_probe

        mov     r9, #0
        str     r9, [r8]

        ldr     r8, =a2024_probe_samples
        ldr     r9, [r8]
        add     r9, r9, #1
        str     r9, [r8]

        /*
         * The common post-frame save enters four bytes off an AAPCS
         * boundary.  Saving seven words aligns the C decoder call.
         */
        push    {r0-r4, r12, lr}
        bl      a2024_decode_control_nominal
        mov     r8, r0
        pop     {r0-r4, r12, lr}

        ldr     r9, =a2024_probe_last_result
        str     r8, [r9]
        cmp     r8, #0
        bmi     a2024_probe_missed

        ldr     r8, =a2024_probe_valid
        ldr     r9, [r8]
        add     r9, r9, #1
        str     r9, [r8]

        ldr     r8, =a2024_match_count
        ldr     r9, [r8]
        add     r9, r9, #1
        str     r9, [r8]
        cmp     r9, #A2024_MATCH_LIMIT
        blt     a2024_finish_resume

        mov     r9, #0
        str     r9, [r8]
        ldr     r8, =a2024_miss_count
        str     r9, [r8]
        ldr     r8, =a2024_entry_events
        ldr     r9, [r8]
        add     r9, r9, #1
        str     r9, [r8]
        add     sp, sp, #48
        ldr     r0, =param_timingset
        ldr     r0, [r0]
        orr     r0, r0, #RET_A2024_CHANGED
        b       rgb_to_fb_exit

a2024_probe_missed:
        ldr     r8, =a2024_probe_invalid
        ldr     r9, [r8]
        add     r9, r9, #1
        str     r9, [r8]

        ldr     r8, =a2024_match_count
        mov     r9, #0
        str     r9, [r8]
        b       a2024_finish_resume

a2024_finish_without_probe:
        /*
         * A valid header before a skipped probe must not count toward a
         * later pair.  Entry requires two uninterrupted probe fields.
         */
        ldr     r8, =a2024_match_count
        mov     r9, #0
        str     r9, [r8]

a2024_finish_resume:
        b       a2024_skip_all_lines_resume

        .ltorg

.bss
        .align 2
a2024_probe_pending:
        .space 4
a2024_match_count:
        .space 4
a2024_miss_count:
        .space 4
a2024_probe_samples:
        .space 4
a2024_probe_invalid:
        .space 4
a2024_probe_valid:
        .space 4
a2024_probe_last_result:
        .space 4
a2024_entry_events:
        .space 4
a2024_release_events:
        .space 4
