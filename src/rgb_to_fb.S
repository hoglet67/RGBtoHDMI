#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text
.global rgb_to_fb
.global poll_keys_only
.global key_press_reset
.global measure_vsync
.global analyse_sync
.global clear_full_screen
.global clear_menu_bits
.global clear_screen
.global measure_n_lines
.global sw1counter
.global sw2counter
.global sw3counter
.global vsync_line
.global total_lines
.global customPalette
.global dummyscreen
.global elk_mode
.global vsync_period
.global vsync_comparison_lo
.global vsync_comparison_hi
.global hsync_period
.global total_hsync_period
.global hsync_comparison_lo
.global hsync_comparison_hi
.global linecountmod10
.global sync_detected
.global last_sync_detected
.global delay_in_arm_cycles
.global benchmarkRAM
.global jitter_offset
.global debug_value
.global param_ntscphase
.global ntsc_status
.global param_delay

.global field_type_threshold
.global elk_lo_field_sync_threshold
.global elk_hi_field_sync_threshold
.global odd_threshold
.global even_threshold
.global hsync_threshold
.global equalising_threshold
.global frame_minimum
.global line_minimum
.global frame_timeout
.global hsync_scroll
.global line_timeout
.global vsync_retry_count

#ifdef USE_MULTICORE
.global run_core
.global read_pointer
.global line_buffer
#endif

.global capture_line_normal_3bpp_table
.global capture_line_normal_6bpp_table
.global capture_line_normal_9bpplo_table
.global capture_line_normal_9bpphi_table
.global capture_line_normal_12bpp_table


.global capture_line_odd_3bpp_table
.global capture_line_even_3bpp_table
.global capture_line_double_3bpp_table
.global capture_line_half_odd_3bpp_table
.global capture_line_half_even_3bpp_table


.global capture_line_simple_12bpp_trailing_pos_table
.global capture_line_simple_12bpp_leading_pos_table
.global capture_line_simple_12bpp_trailing_neg_table
.global capture_line_simple_12bpp_leading_neg_table
.global capture_line_simple_12bpp_trailing_both_table
.global capture_line_simple_12bpp_leading_both_table

rgb_to_fb:

        push   {r4-r12, lr}

        // Save the capture_info_t parameters to absolute addresses
        ldr    r2, [r0, #O_FB_PITCH]
        str    r2, param_fb_pitch
        ldr    r2, [r0, #O_FB_WIDTH]
        str    r2, param_fb_width
        ldr    r2, [r0, #O_FB_HEIGHT]
        str    r2, param_fb_height
        ldr    r2, [r0, #O_FB_SIZEX2]
        str    r2, param_fb_sizex2
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
        ldr    r2, [r0, #O_PALETTE_CONTROL]
        str    r2, param_palette_control
        ldr    r2, [r0, #O_SAMPLE_WIDTH]
        str    r2, param_sample_width
        ldr    r2, [r0, #O_H_ADJUST]
        str    r2, param_h_adjust
        ldr    r2, [r0, #O_V_ADJUST]
        str    r2, param_v_adjust
        ldr    r2, [r0, #O_SYNCTYPE]
        str    r2, param_sync_type
        ldr    r3, [r0, #O_DETSYNCTYPE]
        str    r3, param_detected_sync_type
        ldr    r2, [r0, #O_VSYNCTYPE]
        str    r2, param_vsync_type
        ldr    r2, [r0, #O_VIDEOTYPE]
        str    r2, param_video_type
        bic    r1, r1, #BIT_INTERLACED_VIDEO
        cmp    r2, #0                        //VIDEO_PROGRESSIVE;
        tstne  r3, #SYNC_BIT_INTERLACED
        orrne  r1, r1, #BIT_INTERLACED_VIDEO
        ldr    r2, [r0, #O_NTSCPHASE]
        str    r2, param_ntscphase
        ldr    r2, [r0, #O_BORDER]
        str    r2, param_border
        ldr    r2, [r0, #O_DELAY]
        str    r2, param_delay
        ldr    r2, [r0, #O_FB_BASE]
        str    r2, param_framebuffer0

        ldr    r9, ntsc_status
        and    r9, #0x08                      // last detected burst state
        ldr    r8, param_ntscphase            // 2 bits phase + 1 bit artifact on/off
        orr    r9, r9, r8
        str    r9, ntsc_status

        // Sanity check chars_per_line <= fb_pitch
        ldr    r3, param_fb_pitch
        ldr    r2, param_fb_bpp
        cmp    r2, #4
        moveq  r3, r3, lsl #1
        mov    r3, r3, lsr #3
        ldr    r2, param_chars_per_line
        cmp    r2, r3
        strgt  r3, param_chars_per_line

        // Sanity check nlines <= fb_height
        ldr    r3, param_fb_height
        ldr    r10, param_fb_sizex2
        ands   r10, r10, #1
        movne  r3, r3, lsr #1
        ldr    r2, param_nlines
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
        tst    r1, #(BIT_INTERLACED_VIDEO | BIT_PROBE) // options currently in r1!
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

        // Clear the following state bits:
        bic    r3, r3, #(BIT_FIELD_TYPE)
        bic    r3, r3, #(BIT_FIELD_TYPE1_VALID)

        // In Mode 7 (or on probe) write to buffer 0, display buffer 0
        bic    r3, r3, #(MASK_LAST_BUFFER | MASK_CURR_BUFFER)
#ifdef MULTI_BUFFER
        mov    r8, #NBUFFERS
        str    r8, buffer_total

        tst    r3, #(BIT_INTERLACED_VIDEO | BIT_PROBE)
        // In modes 0..6, restore the previous buffer state
        ldreq  r10, buffer_state
        orreq  r3, r3, r10
#endif

        ldr    r8, param_h_offset
        ldr    r7, param_palette_control
        cmp    r7, #6                       // is it PALETTECONTROL_ATARI_GTIA ?
        moveq  r8, r8, lsl #1               // if so double the offset as sample rate is 2x pixel rate
        ldr    r7, param_sample_width
        cmp    r7, #0x02
        moveq  r7, #1                       // == SAMPLE_WIDTH_6x2
        movgt  r7, #2                       // == SAMPLE_WIDTH_9LO, SAMPLE_WIDTH_9HI, SAMPLE_WIDTH_12
        mov    r8, r8, lsl r7
        add    r8, r8, #1                   // first psync test is wait for a zero after csync
        str    r8, param_h_offset

        ldr    r9, param_fb_sizex2
        tst    r9, #1
        bicne  r3, r3, #BIT_NO_LINE_DOUBLE
        orreq  r3, r3, #BIT_NO_LINE_DOUBLE

        ldr    r8, param_video_type
        cmp    r8, #1 //VIDEO_INTERLACED
        bne    skip_double_check
        tst    r3, #BIT_INTERLACED_VIDEO
        beq    skip_double_check
        ands   r8, r3, #MASK_INTERLACE
        orreq  r3, r3, #BIT_NO_LINE_DOUBLE
        biceq  r9, #1
skip_double_check:

        ldr    r8, param_palette_control
        cmp    r7, #2                     // is sample width SAMPLE_WIDTH_9LO, SAMPLE_WIDTH_9HI, SAMPLE_WIDTH_12 (from above test)
        moveq  r8, #0                     // if so then force palette control to 0
        ldr    r7, param_fb_bpp
        cmp    r7, #8
        moveq  r7, #1
        movne  r7, #0

        // r7 0= 4 bpp or 16bpp, 1=8 bpp
        // r8 0=normal, 1= in band, 2=CGA ntsc, 3=mono ntsc, 4=auto mono ntsc, 5 = pal artifact, 6 = atari gtia
        // r9 0=normal, 1=Hx2, 2=Wx2, 3=H&Wx2

        orr    r10, r7, r8, lsl #1    // slow index in r10 now 0-9
        tst    r9, #2                 // double width?
        addne  r10, r10, #14          // slow index in r10 now 10-19

        add    r7, r7, #28            // main index initially points to fast 4bpp or fast 8bpp (20-21)

        cmp    r8, #0                 // palette control?
        cmpeq  r9, #0                 // double size?
        tsteq  r3, #BIT_OLD_FIRMWARE_SUPPORT  // if version < 3 have to do the second PSYNC read
        movne  r7, r10                // if any are enabled make index point to non-fast versions

        ldr    r9, param_capture_line
        ldr    r8, [r9, r7, lsl #2]

        ldr    r9, param_video_type
        cmp    r9, #2 //VIDEO_TELETEXT
        ldreq  r8, =capture_line_mode7_4bpp

        ldr    r10, param_border
        tst    r10, #0x80
        ldrne  r8, =capture_line_null

        str    r8, capture_address

        ldr    r8, =sentinel
        ldr    r9, =0x48444d49              // "HDMI" sentinel
        str    r9, [r8]

        mov    r8, #4                     // number of frames before h and v sync timing is analysed
        str    r8, frame_countdown

        bl     restore_menu_bits

frame:
        ldr    r8, =inBandPointer
        ldr    r9, =inBandData
        str    r9, [r8]

        ldr    r8, =paletteFlags
        ldr    r9, [r8]
        bic    r9, r9, #BIT_IN_BAND_DETECTED     //in band data detected
        str    r9, [r8]

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
        tst    r3, #(BIT_INTERLACED_VIDEO | BIT_PROBE)
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
        and    r0, r0, #3
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
        subs   r6, r6, r7
        rsbmi  r6, r6, #0
        subs   r5, r7, r5               // work out length of field sync pulse (r5 is start, r7 is end)
        rsbmi  r5, r5, #0
        bic    r3, r3, #BIT_ELK         // clear elk mode
        ldr    r8, param_vsync_type
        cmp    r8, #0                   // auto
        beq    auto_detect_vsync
        cmp    r8, #2                   // interlaced half line
        orreq  r3, r3, #BIT_ELK
        ble    get_field_type           // branch for both types of interlace
        tst    r3, #BIT_MODE_DETECT
        bne    auto_detect_vsync        // "if mode 7 detect enabled then don't use non-interlaced mode as it messes up mode 7 switching"
        orr    r3, r3, #BIT_ELK
        bic    r3, r3, #BIT_FIELD_TYPE  // Odd, clear bit
        bge    got_field_type           // if non interlaced then always elk and odd field
auto_detect_vsync:
        ldr    r7, elk_lo_field_sync_threshold
        cmp    r5, r7  // test for electron field sync which is 2.5 lines (160uS) instead of a whole number (normally 2 lines (128uS) with a 6845)
        blt    get_field_type
        ldr    r7, elk_hi_field_sync_threshold
        cmp    r5, r7
        orrlt  r3, r3, #BIT_ELK
get_field_type:
        // Save the current field type
        ldr    r7, field_type_threshold
        cmp    r6, r7
        biclt  r3, r3, #BIT_FIELD_TYPE  // Odd, clear bit
        orrge  r3, r3, #BIT_FIELD_TYPE  // Even, set bit
got_field_type:
        tst    r3, #BIT_ELK
        moveq  r0, #0
        movne  r0, #1
        str    r0, elk_mode

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
        ldreq  r5, odd_threshold     // Use 22.5us threshold in odd field
        ldrne  r5, even_threshold    // Use 54.5us threshold in even field
        cmp    r6, r5
        movlt  r0, #0         // Modes 0-6
        movge  r0, #1         // Mode 7

        ldr    r9, sync_detected
        cmp    r9, #0
        bne    normal_mode_test
        // preserve existing mode 7 state if no sync
        tst    r3, #BIT_MODE7
        moveq  r0, #0         // Modes 0-6
        movne  r0, #1         // Mode 7

normal_mode_test:
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
        ldr    r9, sync_detected
        cmp    r9, #0
        beq    skip_interlace_test

        ldr    r9, frame_countdown
        cmp    r9, #0
        bne    skip_interlace_test

        tst    r3, #BIT_MODE_DETECT // Have we been told to exit on mode change
        beq    do_interlace_test

        tst    r3, #BIT_INHIBIT_MODE_DETECT
        bne    force_interlace_test

        tst    r3, #BIT_MODE7
        moveq  r5, #0         // Modes 0-6
        movne  r5, #1         // Mode 7
        cmp    r5, r0         // Check if we have changed mode
        bne    exit           // If so, then bail, as the frame buffer needs to be resized
        b      force_interlace_test

do_interlace_test:
        ldr    r9, param_video_type
        cmp    r9, #1  //VIDEO_INTERLACE
        bne    skip_interlace_test
force_interlace_test:
         // only test for interlace if video type set to interlaced or if BBC auto detect enabled
        tst    r3, #BIT_FIELD_TYPE1_VALID
        beq    detect_interlace // "we haven't yet seen two fields, so skip the test"

        // XOR BIT_FIELD_TYPE and BIT_FIELD_TYPE1 to determine if the current frame is interlacd
        // FT1 FT
        // 0   0 -> 0
        // 0   1 -> 1
        // 1   0 -> 1
        // 1   1 -> 0
        // then XOR BIT_INTERLACED and if the result is 1 then the interlace mode has changed
        tst    r3, #BIT_FIELD_TYPE
        eorne  r3, #BIT_FIELD_TYPE1
        ldr    r8, param_detected_sync_type
        tst    r8, #SYNC_BIT_INTERLACED
        eorne  r3, #BIT_FIELD_TYPE1
        tst    r3, #BIT_FIELD_TYPE1
        orrne  r0, #RET_INTERLACE_CHANGED
        bne    exit

detect_interlace:
        // copy BIT_FIELD_TYPE to BIT_FIELD_TYPE1
        tst    r3, #BIT_FIELD_TYPE
        biceq  r3, #BIT_FIELD_TYPE1
        orrne  r3, #BIT_FIELD_TYPE1
        orr    r3, #BIT_FIELD_TYPE1_VALID   // set the valid bit

skip_interlace_test:
        ldr    r8, param_fb_pitch
        ldr    r9, param_v_adjust
        mul    r9, r9, r8
        ldr    r8, param_h_adjust
        add    r9, r9, r8
        add    r11, r11, r9

        ldr    r8, last_sync_detected
        ldr    r9, sync_detected
        str    r9, last_sync_detected
        ldr    r10, param_border
        cmp    r8, #0
        cmpne  r9, #1
        cmpne  r10, #0
        beq    no_sync_loss
        bl     clear_screen              // clear non-zero border on loss of sync
        b      skip_all_lines
no_sync_loss:

        cmp    r8, #0
        cmpeq  r9, #1
        andeq  r0, r3, #BIT_MODE7
        orreq  r0, #RET_SYNC_TIMING_CHANGED
        beq    exit                       // if sync just returned, bail to allow recalculation of sampling clock etc

        // Save a copy of the frame buffer base
        push   {r11}

#if defined(USE_CACHED_COMPARISON_BUFFER)
        ldr r5, =dummyscreen
        sub r5, r5, r11                   // put comparison buffer in cached memory
#else
        ldr    r5, param_framebuffer0
        ldr    r6, param_framebuffer1
        sub    r5, r6, r5                 // put comparison buffer in uncached screen memory
#endif
        str    r5, video_offset

        ldr    r5, param_v_offset

        ldr    r9, param_vsync_type
        cmp    r9, #4    //flywheel vsync
        bne    skip_fix_vsync_jitter


        SHOW_VSYNC
        WAIT_FOR_CSYNC_0_LONG
        WAIT_FOR_CSYNC_1_LONG

        subs   r5, r5, #1
        movmi  r5, #0

        READ_CYCLE_COUNTER r9

        tst    r3, #BIT_OSD | BIT_CALIBRATE | BIT_PROBE
        bne    skip_fix_vsync_jitter_saving_timestamp

        ldr    r10, sync_detected
        ldr    r8, last_sync_detected
        ands   r8, r8, r10
        beq    skip_fix_vsync_jitter_saving_timestamp

        ldr    r10, first_hsync_timestamp
        subs   r6, r9, r10
        rsbmi  r6, r6, #0
        ldr    r0, required_vsync_period
        add    r10, r0, r0, lsr #1
fixvloop:
        cmp    r6, r10
        subge  r6, r6, r0
        bge    fixvloop

        subs   r8, r6, r0                        // flag used below

        add    r6, r6, r0
        mov    r6, r6, lsr #1                    // average latest frame time with previous

        ldr    r0, jitter_offset
        mov    r7, r0
        ldr    r10, hsync_period
        bmi    negative
        add    r8, r8, r10, lsr #1                // half line rounding
subsloop:
        subs   r8, r8, r10
        subpl  r0, r0, #1
        bpl    subsloop
        b      continue
negative:
        sub    r8, r8, r10, lsr #1                // half line rounding
addsloop:
        adds   r8, r8, r10
        addmi  r0, r0, #1
        bmi    addsloop
continue:
        cmp    r0, r7
        streq  r6, required_vsync_period          // update vertical period timing if no jitter detected.

        cmp    r0, #MAX_JITTER_LINES
        movgt  r0, #0
        cmp    r0, #-MAX_JITTER_LINES
        movlt  r0, #0
        adds   r5, r5, r0
        movmi  r5, #0
        str    r0, jitter_offset

skip_fix_vsync_jitter_saving_timestamp:
        str    r9, first_hsync_timestamp

skip_fix_vsync_jitter:

        // Correct the relative positions of the odd and even frames
        // In Mode 0..6, reduce the number of active lines by one for the even frame
        // In Mode 7, increment the frame buffer pointer by one line for the even field

        tst    r3, #BIT_NO_LINE_DOUBLE
        tsteq  r3, #BIT_INTERLACED_VIDEO
        addeq  r11, r11, r2
        tst    r3, #BIT_ELK
        bne    is_elk
        tst    r3, #BIT_INTERLACED_VIDEO
        beq    fixupmodes
        tst    r3, #BIT_FIELD_TYPE
        addeq  r11, r11, r2
fixupmodes:
        tst    r3, #BIT_FIELD_TYPE
        subne  r5, r5, #1     // skip one less line in even fields
is_elk:
        CLEAR_VSYNC
        ldr    r6, param_nlines
        add    r5, r5, r6
        str    r5, total_lines
        str    r5, vsync_line          // default for vsync line if undetectable in blanking area
skip_line_loop:
        cmp    r5, r6
        ble    skip_line_loop_exit
        SHOW_VSYNC
        WAIT_FOR_CSYNC_0_LONG
        WAIT_FOR_CSYNC_1_LONG
        subs   r5, r5, #1
        b      skip_line_loop
skip_line_loop_exit:

        push   {r1-r5, r11}

        ldr    r12, capture_address
        sub    r12, r12, #4
        // Call preload capture line function (runs all paths of capture code to preload it into cache)
        // waits for csync so loses one line
        blx    r12
        pop    {r1-r5, r11}

        // Compute the current scanline mod 10
        ldr    r5, param_v_offset
        add    r5, r5, #1
mod10:
        subs   r5, r5, #10
        bpl    mod10
        add    r5, r5, #10
        str    r5, linecountmod10

        mov    r5, #0
        str    r5, total_hsync_period

        // Process active lines
        ldr    r5, param_nlines
process_line_loop:

        SHOW_VSYNC

        // Preserve the state used by the outer code
        push   {r1-r5, r11}

        // The capture line function is provided the following:
        //   r0 = pointer to current line in frame buffer
        //   r1 = number of complete psync cycles to capture (=param_chars_per_line)
        //   r2 = frame buffer line pitch in bytes (=param_fb_pitch)
        //   r3 = flags register
        //   r4 = GPLEV0 constant
        //   r5 = line number count down to 0 (initial value =param_nlines)
        //   r6 = scan line count modulo 10
        //   r7 = number of psyncs to skip
        //   r8 = frame buffer height (=param_fb_height)
        //   r9 = hsync scroll limits
        // All registers are available as scratch registers (i.e. nothing needs to be preserved)

        // Setup parameters
        // Load the address of the capture_line function into r12
        ldr    r12, capture_address
        mov    r0, r11
        orr    r3, r3, #BIT_NO_SKIP_HSYNC
        ldr    r6, linecountmod10
        ldr    r7, param_h_offset
        ldr    r8, video_offset
        ldr    r9, hsync_scroll

        // Call capture line function
        blx    r12 // exits with h sync timestamp in r0

        // Restore the state used by the outer code

        pop    {r1-r5, r11}

        ldr    r7, last_hsync_time
        str    r0, last_hsync_time
        subs   r7, r0, r7
        rsbmi  r7, r7, #0
        str    r7, hsync_period
        ldr    r0, param_nlines
        cmp    r0, r5                 //ignore 1st line as time undefined
        ldrne  r0, total_hsync_period
        addne  r0, r0, r7
        strne  r0, total_hsync_period

        ldr    r7, param_fb_sizex2
        ands   r7, r7, #1
        // Skip a whole line to maintain aspect ratio
        ldr    r0, linecountmod10
        addne  r11, r11, r2, lsl #1
        addeq  r11, r11, r2
        add    r0, r0, #1
        cmp    r0, #10
        moveq  r0, #0
        str    r0, linecountmod10

        subs   r5, r5, #1
        bne    process_line_loop
        tst    r3, #BIT_INHIBIT_MODE_DETECT
        bicne  r3, #BIT_MODE7
        pop    {r11}

skip_all_lines:
        ldr    r8, frame_countdown
        subs   r8, r8, #1
        strpl  r8, frame_countdown
        bpl    skip_sync_time_test

        ldr    r8, sync_detected
        cmp    r8, #0
        beq    skip_sync_time_test           // if no sync then timing comparison is meaningless

        tst    r3, #BIT_OSD | BIT_CALIBRATE | BIT_PROBE
        bne    skip_sync_time_test

        ldr    r7, hsync_comparison_lo
        ldr    r8, hsync_comparison_hi
        cmp    r7,r8
        beq    skip_sync_time_test            //no sensible window

        and    r0, r3, #BIT_MODE7
        orr    r0, #RET_SYNC_TIMING_CHANGED

        ldr    r6, hsync_period
        ldr    r7, hsync_comparison_lo
        ldr    r8, hsync_comparison_hi
        cmp    r6, r7
        blt    exit
        cmp    r6, r8
        bgt    exit

        ldr    r6, vsync_period
        ldr    r7, vsync_comparison_lo
        ldr    r8, vsync_comparison_hi
        cmp    r6, r7
        blt    exit
        cmp    r6, r8
        ble    no_test_half

        mov    r6, r6, lsr #1                // workaround to check for half the time in case frame dropped due to genlock or palette update taking too long
        cmp    r6, r7
        blt    exit
        cmp    r6, r8
        bgt    exit
no_test_half:

        tst    r3, #BIT_NO_AUTOSWITCH
        bne    skip_sync_time_test

        orr    r0, #RET_VSYNC_POLARITY_CHANGED
        SWITCH_PSYNC_TO_VSYNC
        ldr    r8, [r4]  // dummy read for delay
        ldr    r6, [r4]
        ldr    r9, [r4]  // dummy read for delay
        ldr    r7, [r4]
        SWITCH_VSYNC_TO_PSYNC
        ldr    r8, param_sync_type
        tst    r8, #SYNC_BIT_VSYNC_INVERTED
        eorne  r6, r6, #PSYNC_MASK
        eorne  r7, r7, #PSYNC_MASK
        tst    r6, #PSYNC_MASK       // do test twice in case of glitches
        bne    skip_sync_time_test
        tst    r7, #PSYNC_MASK       // do test twice in case of glitches
        beq    exit

skip_sync_time_test:

        push   {r1-r5, r11}
        ldr    r9, =paletteFlags
        ldr    r8, [r9]
        bic    r8, r8, #BIT_SET_MODE2_16COLOUR   // mode 2 emulation flag
        bic    r8, r8, #BIT_MODE2_PALETTE
        mov    r9, #0           // palette changed flag

        tst    r8, #BIT_IN_BAND_DETECTED
        beq    noInBandData

        adrl    r10, customPalette
        ldr    r12, =inBandData
        ldrb   r11, [r12], #1   //read 1 byte of command data
        cmp    r11, #0
        beq    noInBandData
        cmp    r11, #76        //sanity check on size
        bgt    noInBandData
        mov    r11, r11, lsr #1
        ORR    r8, #BIT_SET_MODE2_16COLOUR          // mode 2 emulation enabled
        ORR    r8, #BIT_MODE2_PALETTE
commandloop:
        ldrb   r1, [r12], #1   //read 1 byte of command data
        and    r0, r1, #0x0f
        ldrb   r1, [r12], #1   //read 1 byte of command data
        and    r3, r1, #0xf0
        orr    r0, r0, r3, lsl #4
        and    r3, r1, #0x0f
        orr    r0, r0, r3, lsl #16
        orr    r0, r0, r0, lsl #4

        ldr    r2, [r10]
        str    r0, [r10], #4

        cmp    r0, r2
        movne  r9, #1

        subs   r11, r11, #1
        bne    commandloop

noInBandData:

        ldr    r10, =paletteFlags
        ldr    r7, [r10]
        str    r8, [r10]
        cmp    r9, #0
        cmpeq  r7, r8
        blne   osd_update_palette

        pop    {r1-r5,r11}

        // Update the OSD in Mode 0..6

        tst    r3, #BIT_CLEAR
        bne    force_osd_update
        ldr    r0, param_video_type
        cmp    r0, #2  //VIDEO_TELETEXT
        beq    skip_osd_update
        tst    r3, #BIT_OSD
        beq    skip_osd_update

        push   {r1-r5, r11}
        mov    r0, #0 //do not force genlock
        bl     recalculate_hdmi_clock_line_locked_update
        bl     wait_for_vsync               //wait for field sync as sometimes the update will be on the ragged edge of finishing during field sync causing glitches
        pop    {r1-r5, r11}
        subs   r6, r6, r7
        rsbmi  r6, r6, #0
        ldr    r7, field_type_threshold
        cmp    r6, r7
        biclt  r3, r3, #BIT_FIELD_TYPE1  // Odd, clear bit
        orrge  r3, r3, #BIT_FIELD_TYPE1  // Even, set bit
force_osd_update:
        push   {r1-r5, r11}
        mov    r0, r11
        mov    r1, r2                       // bytes per line
        bl     osd_update_fast
        pop    {r1-r5, r11}
skip_osd_update:
        bic    r3, r3, #BIT_CLEAR

#ifdef MULTI_BUFFER
        // Update the last drawn buffer
        mov    r0, r3, lsr #OFFSET_CURR_BUFFER
        and    r0, #3
        bic    r3, r3, #MASK_LAST_BUFFER
        orr    r3, r3, r0, lsl #OFFSET_LAST_BUFFER
        // Flip to it on next V SYNC
        FLIP_BUFFER
#endif
        push   {r1-r5, r11}

        mov    r0, #0 //do not force genlock
        bl     recalculate_hdmi_clock_line_locked_update

        // Returns:
        //   r0=0 genlock disabled           - LED off
        //   r0=1 genlock enabled (unlocked) - LED flash
        //   r0=2 genlock enabled (locked)   - LED on

        READ_CYCLE_COUNTER r1

        mov    r2, #LED1_MASK
        tst    r0, #1         // should LED flash?
        tstne  r1, #(1 << 26) // flash rate ~ 8Hz
        tsteq  r0, #2         // should LED be on?
        ldrne  r1, =GPSET0    // LED on
        ldreq  r1, =GPCLR0    // LED off
        str    r2, [r1]

        pop    {r1-r5, r11}

        // Loop back if required number of fields has not been reached
        // or if negative (capture forever)
        ldr    r5, param_ncapture
        cmp    r5, #0
        blt    frame
        subs   r5, #1
        str    r5, param_ncapture
        bne    frame

        // Setup the response code
        and    r0, r3, #BIT_MODE7
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
        push   {r0}
        ldr    r9, ntsc_status
        tst    r9, #4
        moveq  r0,#0
        movne  r0,#1
        bl     set_ntsccolour
        pop    {r0,r4-r12, lr}
        mov    pc, lr

key_press_reset:
        push   {r4-r12, lr}
        ldr    r4, =GPLEV0
        ldr    r8, [r4]
        mov    r0, #0
        tst    r8, #SW1_MASK
        orreq  r0, r0, #1
        tst    r8, #SW2_MASK
        orreq  r0, r0, #2
        tst    r8, #SW3_MASK
        orreq  r0, r0, #4
        pop    {r4-r12, lr}
        mov     pc, lr

// ======================================================================
// Local Variables
// ======================================================================
        .ltorg

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

param_fb_sizex2:
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

param_palette_control:
        .word 0

param_sample_width:
        .word 0

param_h_adjust:
        .word 0

param_v_adjust:
        .word 0

param_sync_type:
        .word 0

param_detected_sync_type:
        .word 0

param_vsync_type:
        .word 0

param_video_type:
        .word 0

param_ntscphase:
        .word 0

param_border:
        .word 0

param_delay:
        .word 0

buffer_total:
        .word 1

capture_address:
        .word 0

linecountmod10:
        .word 0

vsync_line:
        .word 0

total_lines:
        .word 0

elk_mode:
        .word 0

last_vsync_time:
        .word 0

required_vsync_period:
        .word 0

vsync_period:
        .word 0

vsync_comparison_lo:
        .word 0

vsync_comparison_hi:
        .word 0

last_hsync_time:
        .word 0

first_hsync_timestamp:
        .word 0

hsync_period:
        .word 0

total_hsync_period:
        .word 0

hsync_comparison_lo:
        .word 0

hsync_comparison_hi:
        .word 0

frame_countdown:
        .word 0

sync_detected:
        .word 1

last_sync_detected:
        .word 1

last_scanlines_state:
        .word 0

video_offset:
        .word 0

jitter_offset:
        .word 0

debug_value:
        .word 0

ntsc_status:
        .word 0

field_type_threshold:
        .word 32000
elk_lo_field_sync_threshold:
        .word 150000
elk_hi_field_sync_threshold:
        .word 170000
odd_threshold:
        .word 22500
even_threshold:
        .word 54500
hsync_threshold:
        .word 6144
equalising_threshold:
        .word 3250
frame_minimum:
        .word 10000000
line_minimum:
        .word 20000
frame_timeout:
        .word 24000000
hsync_scroll:
        .word (4000 - 224) | ((4000 + 224) << 16)
line_timeout:
        .word 100000000
vsync_retry_count:
        .word 0



// ======================================================================
// WAIT_FOR_VSYNC
// ======================================================================

wait_for_vsync_with_line_delay:
        push {lr}
        bl   wait_for_vsync
        mov  r9, #25 //wait for 25 lines so clear of any spurious pulses
wait_100_lines:
        WAIT_FOR_CSYNC_1_LONG
        WAIT_FOR_CSYNC_0_LONG
        subs   r9, r9, #1
        bne    wait_100_lines
        pop  {pc}

wait_for_vsync:
        // Wait for end of vsync
        //
        // Returns:
        // r11 = duration of last csync low pulse
        // r6 = time of last rising edge of csync
        // r7 = time of last-but-one rising edge of csync
        // r5 = time of start of field sync pulse

        // Working registers in the first half
        //
        // r4 = GPLEV0
        // r5 = time of falling edge
        // r6 = time of rising edge
        // r7 = time of previous rising edge
        // r8 = value read from GPLEV0
        // r9 = state variable (1 = seen a long pulse

        push   {lr}
        ldr    r10, param_sync_type
        ldr    r9, param_detected_sync_type
        tst    r9, #SYNC_BIT_MIXED_SYNC           //clear if V and H not eored in CPLD
        tsteq  r10, #SYNC_BIT_COMPOSITE_SYNC       //clear if separate H and V syncs
        beq    separate_syncs

        // Initialize "seen long pulse" to false (0)
        mov    r9, #0
        // Wait for csync to be high
        READ_CYCLE_COUNTER r10
        WAIT_FOR_CSYNC_1_LONG
        READ_CYCLE_COUNTER r6
vsync_loop:
        READ_CYCLE_COUNTER r8
        subs    r8, r8, r10
        rsbmi   r8, r8, #0
        ldr     r14, frame_timeout
        cmp     r8, r14
        movgt   r10, #0
        bgt     abort_vsync
        // Wait for the falling edge of csync
        WAIT_FOR_CSYNC_0_LONG
        // Wait for the rising edge of hsync
        WAIT_FOR_CSYNC_1_LONG       //puts entry time in r14 recording time of the falling edge above
        // Save time of previous rising edge
        mov    r7, r6
        // Record time of the rising edge
        READ_CYCLE_COUNTER r6
        // Calculate length of low hsync pulse (in ARM cycles = ns)
        subs   r11, r6, r14
        rsbmi  r11, r11, #0
        // Compare with 6us for bbc or 9us for others to descriminate short from long
        // - normal hsync pulses are 4us
        // - during vsync everything is either inverted, or clamped to zero
        // - this results in hsync pulses between 9us and 128us
        ldr    r8, equalising_threshold
        cmp    r11, r8
        blt    not_hsync
        ldr    r8, hsync_threshold
        cmp    r11, r8
        blt    seen_short
not_hsync:
        cmp    r9, #1
        movne  r5, r14     // save falling edge of first broad pulse to calculate field sync length
        // Set the state variable to indicate we are in the vsync
        mov    r9, #1
        // Loop back to wait for the next pulse
        b      vsync_loop
seen_short:
        // "Test to see if we've seen any long pulses"
        cmp    r9, #1
        // No, so look back for the next pulse
        bne    vsync_loop

        ldr    r11, hsync_threshold             // normally 10us
        mov    r11, r11, lsl #1                 // 20us
wait_hi_20:                                     // make sure sync high for > 20us to avoid malformed vsync pulses (e.g. Apple II)
        ldr    r8, [r4]
        tst    r8, #CSYNC_MASK
        beq    vsync_loop
        READ_CYCLE_COUNTER r8
        subs   r8, r8, r6
        rsbmi  r8, r8, #0
        cmp    r8, r11
        blt    wait_hi_20

        mov    r10, #1
abort_vsync:
        str    r10, sync_detected

        cmp    r10, #0
        moveq  r7, r6                           // make all fields look the same
        moveq  r5, r7                           // set length of vsync pulse to 0 so electron test fails

        ldr    r8, last_vsync_time
        str    r6, last_vsync_time
        subs   r8, r6, r8
        rsbmi  r8, r8, #0
        str    r8, vsync_period
        pop    {pc}






separate_syncs:
        SWITCH_PSYNC_TO_VSYNC
        ldr    r8, [r4]                         //delay
        ldr    r9, [r4]                         //delay
        tst    r10, #SYNC_BIT_VSYNC_INVERTED    // if set then +ve going vsync
        biceq  r3, r3, #PSYNC_MASK
        orrne  r3, r3, #PSYNC_MASK
        READ_CYCLE_COUNTER r14

resetPSA:
        READ_CYCLE_COUNTER r9
waitPSA:
        READ_CYCLE_COUNTER r8
        subs   r8, r8, r14
        rsbmi  r8, r8, #0
        ldr    r7, frame_timeout
        cmp    r8, r7
        movgt  r10, #0
        bgt    abortvs
        // Read the GPLEV0

deglitch1:
        ldr    r8, [r4]
        ldr    r7, [r4]     //delay
        ldr    r7, [r4]
        eor    r7, r7, r8
        tst    r7, #PSYNC_MASK
        bne    deglitch1

        eor    r8, r3
        tst    r8, #PSYNC_MASK
        beq    noresetPSA
deglitch2:
        ldr    r8, [r4]
        ldr    r7, [r4]     //delay
        ldr    r7, [r4]
        eor    r7, r7, r8
        tst    r7, #PSYNC_MASK
        bne    deglitch2

        eor    r8, r3
        tst    r8, #PSYNC_MASK
        bne    resetPSA
noresetPSA:
        READ_CYCLE_COUNTER r8
        subs   r8, r8, r9
        rsbmi  r8, r8, #0
        ldr    r7, hsync_threshold
        cmp    r8, r7                     // has it been low for ~10us?
        blt    waitPSA

resetPSB:
        READ_CYCLE_COUNTER r9
waitPSB:
        READ_CYCLE_COUNTER r8
        subs   r8, r8, r14
        rsbmi  r8, r8, #0
        ldr    r7, frame_timeout
        cmp    r8, r7
        movgt  r10, #0
        bgt    abortvs
        // Read the GPLEV0
deglitch3:
        ldr    r8, [r4]
        ldr    r7, [r4]     //delay
        ldr    r7, [r4]
        eor    r7, r7, r8
        tst    r7, #PSYNC_MASK
        bne    deglitch3
        eor    r8, r3
        tst    r8, #PSYNC_MASK
        bne    noresetPSB
deglitch4:
        ldr    r8, [r4]
        ldr    r7, [r4]     //delay
        ldr    r7, [r4]
        eor    r7, r7, r8
        tst    r7, #PSYNC_MASK
        bne    deglitch4
        eor    r8, r3
        tst    r8, #PSYNC_MASK
        beq    resetPSB
noresetPSB:
        READ_CYCLE_COUNTER r8
        subs   r8, r8, r9
        rsbmi  r8, r8, #0
        ldr    r7, hsync_threshold
        cmp    r8, r7                        // has it been high for ~10us?
        blt    waitPSB
        mov    r10, #1
abortvs:
        str    r10, sync_detected

        READ_CYCLE_COUNTER r6
        mov    r7, r6                           // make all fields look the same
        mov    r5, r7                           // set length of vsync pulse to 0 so electron test fails
        SWITCH_VSYNC_TO_PSYNC

        ldr    r8, last_vsync_time
        str    r6, last_vsync_time
        subs   r8, r6, r8
        rsbmi  r8, r8, #0
        str    r8, vsync_period

        WAIT_FOR_CSYNC_1_LONG                    // resync with hsync
        pop    {pc}

// ======================================================================
// ANALYSE SYNC POLARITY
// ======================================================================
analyse_sync:
        push    {r4-r12, lr}
        SWITCH_PSYNC_TO_VSYNC
        ldr    r4, =GPLEV0

        READ_CYCLE_COUNTER r10
delaysloop:
        ldr    r5, [r4]                            // dummy read for delay
        READ_CYCLE_COUNTER r11
        subs   r12, r10, r11
        rsbmi  r12, r12, #0
        ldr    r5, frame_timeout
        cmp    r12, r5
        blt    delaysloop

        mov    r6, #0 //csync low
        mov    r7, #0 //csync high
        mov    r8, #0 //vsync low
        mov    r9, #0 //vsync high
        READ_CYCLE_COUNTER r10
analyse_vloop:
        ldr    r5, [r4]
        ldr    r11, [r4]           //delay
        ldr    r11, [r4]
        eor    r11, r11, r5
        tst    r11, #CSYNC_MASK | PSYNC_MASK      //deglitch sync & vsync
        bne    analyse_vloop

        tst    r5, #CSYNC_MASK
        addeq  r6, r6, #1
        addne  r7, r7, #1
        tst    r5, #PSYNC_MASK                     // actually vsync when version = 0
        addeq  r8, r8, #1
        addne  r9, r9, #1
        READ_CYCLE_COUNTER r11
        subs   r12, r10, r11
        rsbmi  r12, r12, #0
        ldr    r5, frame_timeout
        cmp    r12, r5, lsl#1
        blt    analyse_vloop

       // cmp    r8, #4                             // if a very small count in r8 or r9, it might be noise / spikes on vsync so set to 0
       // movlt  r8, #0
       // cmp    r9, #4
       // movlt  r8, #0

        mov    r0, #0
        cmp    r6, r7                              // is low time > high time
        orrgt  r0, #SYNC_BIT_HSYNC_INVERTED        // inverted means positive going
        cmp    r8, r9                              // is low time > high time
        orrgt  r0, r0, #SYNC_BIT_VSYNC_INVERTED    // inverted means positive going
        SWITCH_VSYNC_TO_PSYNC

        pop    {r4-r12, pc}

// ======================================================================
// MEASURE_VSYNC
// ======================================================================

measure_vsync:
        push    {r4-r12, lr}

        mov    r3, #0
        // Setup R4 as a constant
        ldr    r4, =GPLEV0
        mov    r12, #VSYNC_RETRY_MAX          //retry count
        ldr    r9, sync_detected
        cmp    r9, #0
        moveq  r12, #1                        //no retry if no sync
test_again:
        // wait for vsync, r6 contains the time of the subsequent hsync rising edge
        bl     wait_for_vsync_with_line_delay
        mov    r0, r6

        ldr    r8, last_sync_detected
        ldr    r9, sync_detected
        and    r9, r9, r8
        str    r9, last_sync_detected            //make a sync fail persist over the measurement

        // Wait for a first field of frame
        bl     wait_for_vsync_with_line_delay
        subs   r8, r6, r0
        str    r8, vsync_period

        ldr    r8, last_sync_detected
        ldr    r9, sync_detected
        and    r9, r9, r8
        str    r9, last_sync_detected            //make a sync fail persist over the measurement

        // Wait for a second field of frame
        bl     wait_for_vsync_with_line_delay

        // Return the time for a complete frame (should be 40ms)
        subs   r0, r6, r0
        rsbmi  r0, r0, #0

        ldr    r8, vsync_period
        sub    r8, r0, r8, lsl #1
        ldr    r9, hsync_period
        cmp    r8, r9, lsr #1
        subgts r12, r12, #1                     //retry count to avoid hangup
        cmpgt  r12, #0
        bgt    test_again                       //jitter or error detected if difference > half a line

        rsb    r12, r12, #VSYNC_RETRY_MAX
        str    r12, vsync_retry_count

        mov    r8, r0, lsr #1
        str    r8, required_vsync_period
        str    r8, vsync_period

        ldr    r8, last_sync_detected
        ldr    r9, sync_detected
        and    r9, r9, r8
        str    r9, last_sync_detected            //make a sync fail persist over the measurement

        WAIT_FOR_CSYNC_1_LONG
        WAIT_FOR_CSYNC_0_LONG

        READ_CYCLE_COUNTER r9
        str    r9, first_hsync_timestamp

        mov    r8, #0
        str    r8, jitter_offset

        pop    {r4-r12, pc}

// ======================================================================
// MEASURE_N_LINES
// ======================================================================

// Note: this is coded as a single loop with conditional mrc instructions
// to mitigate the effect of I-Cache misses.

measure_n_lines:
        push   {r4-r12, lr}
        mov    r3, #0
        // Setup R4 as a constant
        ldr    r4, =GPLEV0

        // wait for vsync
        bl     wait_for_vsync

        // skip 20 lines so we are well away from any double vsync pulses
        add    r1, r0, #20
        add    r0, r0, #1

        // r1 is the loop counter
measure_n_loop:
        WAIT_FOR_CSYNC_1_LONG
        WAIT_FOR_CSYNC_0_LONG
        cmp    r1, r0
#if defined(RPI2) || defined(RPI3) || defined(RPI4)
        mrceq  p15, 0, r7, c9, c13, 0
#else
        mrceq  p15, 0, r7, c15, c12, 1
#endif
        subs   r1, r1, #1
#if defined(RPI2) || defined(RPI3) || defined(RPI4)
        mrceq  p15, 0, r6, c9, c13, 0
#else
        mrceq  p15, 0, r6, c15, c12, 1
#endif
        bne    measure_n_loop

        WAIT_FOR_CSYNC_1_LONG
        WAIT_FOR_CSYNC_0_LONG

        READ_CYCLE_COUNTER r10
        subs   r10, r10, r6
        rsbmi  r10, r10, #0
        str    r10, hsync_period                //initial value for hsync_period

        subs   r0, r6, r7
        rsbmi  r0, r0, #0

        ldr    r8, last_sync_detected
        ldr    r9, sync_detected
        and    r9, r9, r8
        str    r9, last_sync_detected            //make a sync fail persist over the measurement
        pop    {r4-r12, pc}



// ======================================================================
// CLEAR_SCREEN
// ======================================================================

clear_screen:
        push   {r4-r12, lr}
        ldr    r5, param_fb_height
        ldr    r6, param_fb_pitch
        ldr    r11, param_framebuffer0

        mul    r6, r5, r6
#ifdef MULTI_BUFFER
        ldr    r5, buffer_total
        //mov    r5, #NBUFFERS
        mul    r6, r5, r6
#endif
        ldr    r7, param_fb_bpp
        ldr    r9, param_border
        ldr    r10, sync_detected
        tst    r9, #0x80
        cmpeq  r10, #0
        moveq  r9, #0
        andne  r9, r9, #0x7f
        cmp    r7, #4
        andeq  r9, #7
        orreq  r9, r9, lsl #4
        orr    r9, r9, lsl #8
        orr    r9, r9, lsl #16
clearloop:
        subs   r6, r6, #4
        str    r9, [r11], #4
        bne    clearloop
        pop    {r4-r12, pc}

// ======================================================================
// CLEAR_FULL_SCREEN
// ======================================================================

clear_full_screen:
        push   {r4-r12, lr}
        ldr    r5, param_fb_height
        ldr    r6, param_fb_pitch
        ldr    r11, param_framebuffer0
        mul    r6, r5, r6
#ifdef MULTI_BUFFER
        ldr    r5, buffer_total
        //mov    r5, #NBUFFERS
        mul    r6, r5, r6
#endif
        mov    r7, #0
clearfull:
        subs   r6, r6, #4
        str    r7, [r11], #4
        bne    clearfull
        pop    {r4-r12, pc}

// ======================================================================
// CLEAR_MENU_BITS
// ======================================================================

clear_menu_bits:
        ldr    r0, param_fb_bpp
        cmp    r0, #2 //16 BPP
        movge  pc, lr
        ldr    r3, last_scanlines_state
        tst    r3, #BIT_NO_SCANLINES | BIT_PROBE | BIT_INTERLACED_VIDEO
        movne  pc, lr
        push   {r4-r12, lr}
        ldr    r5, param_fb_height
        ldr    r6, param_fb_pitch
        ldr    r7, param_fb_bpp
        ldr    r11, param_framebuffer0
        ldr    r8, =0x88888888
        ldr    r9, =0x80808080
        cmp    r7, #4
        movne  r8, r9
        mul    r6, r5, r6
#ifdef MULTI_BUFFER
        ldr    r5, buffer_total
        //mov    r5, #NBUFFERS
        mul    r6, r5, r6
#endif
clear_menu:
        ldr    r7, [r11]
        subs   r6, r6, #4
        bic    r7, r8
        str    r7, [r11], #4
        bne    clear_menu
        pop    {r4-r12, pc}

restore_menu_bits:
        ldr    r0, param_fb_bpp
        cmp    r0, #2 //16 BPP
        movge  pc, lr
        str    r3, last_scanlines_state
        tst    r3, #BIT_NO_SCANLINES | BIT_PROBE | BIT_INTERLACED_VIDEO
        movne  pc, lr
        ldr    r0, param_ncapture
        cmp    r0, #0
        movpl  pc, lr
        push   {r4-r12, lr}
        bl     wait_for_vsync
        ldr    r7, param_fb_bpp
        ldr    r8, =0x88888888
        ldr    r9, =0x80808080
        cmp    r7, #4
        movne  r8, r9
        ldr    r11, param_framebuffer0
#ifdef MULTI_BUFFER
        ldr    r7, buffer_total
        //mov    r7, #NBUFFERS
#else
        mov    r7, #1
#endif
        ldr    r12, param_fb_pitch
restfull3:
        ldr    r6, param_fb_height
        mov    r6, r6, lsr #1
restfull2:
        mov    r5, r12
restfull:
        subs   r5, r5, #4
        ldr    r10, [r11]
        orr    r10, r10, r8
        str    r10, [r11], #4
        bne    restfull
        add    r11, r11, r12
        subs   r6, r6, #1
        bne    restfull2
        subs   r7, r7, #1
        bne    restfull3
        pop    {r4-r12, pc}

        mov    pc, lr   //entry point for capture_line_null cache pre-load
capture_line_null:
        push    {lr}
        ldr    r8, param_fb_bpp
        ldr    r12, param_border

        cmp    r8, #4
        movne  r1, r1, lsl #1
        andne  r12, r12, #0x7f
        andeq  r12, r12, #7
        orreq  r12, r12, lsl #4
        orr    r12, r12, lsl #8
        orr    r12, r12, lsl #16

        SKIP_PSYNC
loop_null:
        mov    r7, r12
        mov    r10, r12
        WRITE_R7_R10
        subs    r1, r1, #1
        bne     loop_null
        pop     {r0, pc}


// ======================================================================
// delay_in_arm_cycles()
// ======================================================================

delay_in_arm_cycles:
        READ_CYCLE_COUNTER r1
delay_loop:
        READ_CYCLE_COUNTER r2
        subs   r2, r1
        rsbmi  r2, r2, #0
        cmp    r2, r0
        blt    delay_loop
        mov    pc, lr

benchmarkRAM:
        push   {r1-r12, lr}
        // RAM address in r0 returns with time in r0
        READ_CYCLE_COUNTER r4
        mov    r3, #1024
benchloop:
        mov    r1, r0
        add    r2, r1, #1024
benchloop2:
        ldr    r6, [r1], #4
        cmp    r1, r2
        blt    benchloop2
        subs   r3, r3, #1
        bne    benchloop
        READ_CYCLE_COUNTER r5
        subs   r0, r5, r4
        rsbmi  r0, r0, #1
        pop   {r1-r12, pc}
        .ltorg

        .align 6
        .ltorg

#ifdef USE_MULTICORE

read_pointer:
        .word 0
write_pointer:
        .word 0
r3copy:
        .word 0
r4copy:
        .word 0
r9copy:
        .word 0
capture_count:
        .word 0
skip_count:
        .word 0
run_core:
        bl     enable_MMU_and_IDCaches
        bl    _enable_unaligned_access
        bl    _init_cycle_counter
run_core_loop:
        wfe                       // put core to sleep until an event
        ldr    r11, capture_count
        cmp    r11, #0
        beq    run_core_loop      // go back to sleep if capture count not valid
        ldr    r7, skip_count
        cmp    r7, #0
        beq    run_core_loop      // go back to sleep if skip count not valid
        ldr    r3, r3copy
        ldr    r4, r4copy
        ldr    r9, r9copy
        SKIP_PSYNC
        mov    r7, #0
        str    r7, skip_count
        ldr    r12, write_pointer
core_capture:
        WAIT_FOR_PSYNC_EDGE
        CAPTURE_LOW_BITS
        WAIT_FOR_PSYNC_EDGE
        CAPTURE_HIGH_BITS
        mov    r6, r10
        WAIT_FOR_PSYNC_EDGE
        CAPTURE_LOW_BITS
        WAIT_FOR_PSYNC_EDGE
        CAPTURE_HIGH_BITS
        mov    r7, r10
        WAIT_FOR_PSYNC_EDGE
        CAPTURE_LOW_BITS
        WAIT_FOR_PSYNC_EDGE
        CAPTURE_HIGH_BITS
        stmia  r12!, {r6,r7,r10}
        str    r12, write_pointer
        sub    r11, r11, #3
        cmp    r11, #0
        bgt    core_capture
        str    r12, write_pointer
        mov    r11, #0
        str    r11, capture_count
        b      run_core_loop
#endif



customPalette:
        .space 2048, 0

         // order of table entries

         // default for 4 bits per pixel - used if double height enabled and palette control off. Also used if source is BBC micro when double height disabled
         // default for 8 bits per pixel - used if double height enabled and palette control off. Also used if source is BBC micro when double height disabled
         // in band mode for 4 bits per pixel - used if palette control = in band
         // in band mode for 8 bits per pixel - used if palette control = in band (not yet implemented)
         // cga ntsc artifacting mode for 4 bits per pixel - used if palette control = ntsc artifacting
         // cga ntsc artifacting mode for 8 bits per pixel - used if palette control = ntsc artifacting
         // mono ntsc artifacting mode for 4 bits per pixel - used if palette control = ntsc artifacting
         // mono ntsc artifacting mode for 8 bits per pixel - used if palette control = ntsc artifacting
         // auto mono ntsc artifacting mode for 4 bits per pixel - used if palette control = ntsc artifacting
         // auto mono ntsc artifacting mode for 8 bits per pixel - used if palette control = ntsc artifacting

         // double default for 4 bits per pixel - used if double height enabled and palette control off. Also used if source is BBC micro when double height disabled
         // double default for 8 bits per pixel - used if double height enabled and palette control off. Also used if source is BBC micro when double height disabled
         // double in band mode for 4 bits per pixel - used if palette control = in band (not yet implemented)
         // double in band mode for 8 bits per pixel - used if palette control = in band (not yet implemented)
         // double ntsc artifacting mode for 4 bits per pixel - used if palette control = ntsc artifacting (not yet implemented)
         // double ntsc artifacting mode for 8 bits per pixel - used if palette control = ntsc artifacting (not yet implemented)
         // double ntsc artifacting mode for 4 bits per pixel - used if palette control = ntsc artifacting (not yet implemented)
         // double ntsc artifacting mode for 8 bits per pixel - used if palette control = ntsc artifacting (not yet implemented)
         // double ntsc artifacting mode for 4 bits per pixel - used if palette control = ntsc artifacting (not yet implemented)
         // double ntsc artifacting mode for 8 bits per pixel - used if palette control = ntsc artifacting (not yet implemented)

         // fast mode for 4 bits per pixel - used if double size disabled and palette control off (excluding BBC micro source as fine H scroll doesn't work)
         // fast mode for 8 bits per pixel - used if double size disabled and palette control off (excluding BBC micro source as fine H scroll doesn't work)

capture_line_normal_3bpp_table:
        .word capture_line_default_4bpp
        .word capture_line_default_8bpp
        .word capture_line_inband_4bpp
        .word capture_line_inband_8bpp
        .word capture_line_default_4bpp
        .word capture_line_ntsc_8bpp_cga
        .word capture_line_default_4bpp
        .word capture_line_ntsc_8bpp_mono
        .word capture_line_default_4bpp
        .word capture_line_ntsc_8bpp_mono                //"3bpp mono_auto won't work"
        .word capture_line_default_4bpp
        .word capture_line_default_8bpp
        .word capture_line_default_4bpp
        .word capture_line_atari_8bpp

        .word capture_line_default_double_4bpp
        .word capture_line_default_double_8bpp
        .word capture_line_default_double_4bpp           // placeholder inband
        .word capture_line_default_double_8bpp           // placeholder inband
        .word capture_line_default_double_4bpp           // placeholder ntsc
        .word capture_line_default_double_8bpp           // placeholder ntsc
        .word capture_line_default_double_4bpp           // placeholder ntsc
        .word capture_line_default_double_8bpp           // placeholder ntsc
        .word capture_line_default_double_4bpp           // placeholder ntsc
        .word capture_line_default_double_8bpp           // placeholder ntsc
        .word capture_line_default_double_4bpp
        .word capture_line_default_double_8bpp
        .word capture_line_default_double_4bpp
        .word capture_line_atari_double_8bpp

        .word capture_line_fast_4bpp
        .word capture_line_fast_8bpp

capture_line_normal_6bpp_table:
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp          // placeholder inband
        .word capture_line_default_sixbits_8bpp           // placeholder inband
        .word capture_line_default_sixbits_16bpp
        .word capture_line_ntsc_sixbits_8bpp_cga
        .word capture_line_default_sixbits_16bpp
        .word capture_line_ntsc_sixbits_8bpp_mono
        .word capture_line_default_sixbits_16bpp
        .word capture_line_ntsc_sixbits_8bpp_mono_auto
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_atari_sixbits_8bpp

        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp   // placeholder inband
        .word capture_line_default_sixbits_double_8bpp    // placeholder inband
        .word capture_line_default_sixbits_double_16bpp   // placeholder ntsc
        .word capture_line_default_sixbits_double_8bpp    // placeholder ntsc
        .word capture_line_default_sixbits_double_16bpp   // placeholder ntsc
        .word capture_line_default_sixbits_double_8bpp    // placeholder ntsc
        .word capture_line_default_sixbits_double_16bpp   // placeholder ntsc
        .word capture_line_default_sixbits_double_8bpp    // placeholder ntsc
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_atari_sixbits_double_8bpp

        .word capture_line_fast_sixbits_16bpp
        .word capture_line_fast_sixbits_8bpp

capture_line_normal_9bpplo_table:
        .word capture_line_default_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp

        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_ninebitslo_16bpp
        .word capture_line_fast_eightbits_8bpp


capture_line_normal_9bpphi_table:
        .word capture_line_default_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp

        .word capture_line_default_ninebitshi_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitshi_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitshi_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitshi_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitshi_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitshi_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitshi_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_ninebitshi_16bpp
        .word capture_line_fast_eightbits_8bpp

capture_line_normal_12bpp_table:
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp

        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_twelvebits_16bpp
        .word capture_line_fast_eightbits_8bpp

capture_line_simple_12bpp_trailing_pos_table:
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp


        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_simple_trailing_pos_16bpp
        .word capture_line_fast_eightbits_8bpp

capture_line_simple_12bpp_leading_pos_table:
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp

        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_simple_leading_pos_16bpp
        .word capture_line_fast_eightbits_8bpp

capture_line_simple_12bpp_trailing_neg_table:
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp

        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_simple_trailing_neg_16bpp
        .word capture_line_fast_eightbits_8bpp

capture_line_simple_12bpp_leading_neg_table:
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp

        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_simple_leading_neg_16bpp
        .word capture_line_fast_eightbits_8bpp

capture_line_simple_12bpp_trailing_both_table:
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp

        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_simple_trailing_both_16bpp
        .word capture_line_fast_eightbits_8bpp

capture_line_simple_12bpp_leading_both_table:
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_twelvebits_16bpp
        .word capture_line_default_eightbits_8bpp

        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_simple_leading_both_16bpp
        .word capture_line_fast_eightbits_8bpp

// tables below are deprecated and will be removed in future

capture_line_odd_3bpp_table:
capture_line_odd_6bpp_table:  //no six bit versions
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp

        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp
        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp

        .word capture_line_odd_4bpp
        .word capture_line_odd_8bpp


capture_line_even_3bpp_table:
capture_line_even_6bpp_table: //no six bit versions
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp

        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp
        .word capture_line_even_4bpp
        .word capture_line_even_8bpp

        .word capture_line_even_4bpp
        .word capture_line_even_8bpp

capture_line_half_odd_3bpp_table:
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp

        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp
        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp

        .word capture_line_half_odd_4bpp
        .word capture_line_half_odd_8bpp

capture_line_half_even_3bpp_table:
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp

        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp
        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp

        .word capture_line_half_even_4bpp
        .word capture_line_half_even_8bpp

// ======================================================================
// Poll only keys (for when CPLD is unprogrammed)
// ======================================================================

poll_keys_only:
        push   {r4-r12, lr}
        ldr    r12, [r0, #O_NCAPTURE]
poll_keys_loop:
        // Wait ~20ms (for debouncing)
        ldr    r0, =20*1024*1024
        bl     delay_in_arm_cycles
        ldr    r4, =GPLEV0
        ldr    r8, [r4]
        mov    r0, #0
        KEY_PRESS_DETECT SW1_MASK, RET_SW1, sw1counter2
        KEY_PRESS_DETECT SW2_MASK, RET_SW2, sw2counter2
        KEY_PRESS_DETECT SW3_MASK, RET_SW3, sw3counter2
        tst    r0, #(RET_SW1 | RET_SW2 | RET_SW3)
        bne    poll_keys_exit
        // Loop back if required number of fields has not been reached
        // or if negative (capture forever)
        cmp    r12, #0
        blt    poll_keys_loop
        subs   r12, #1
        bne    poll_keys_loop
        orr    r0, #RET_EXPIRED
poll_keys_exit:
        pop    {r4-r12, lr}
        mov    pc, lr

sw1counter2:
        .word 0

sw2counter2:
        .word 0

sw3counter2:
        .word 0

        .ltorg
        .align 6
line_buffer:
        .space 1920, 0
        .align 6
dummyscreen:               // used by capture preload
        .space 1920*1080, 0