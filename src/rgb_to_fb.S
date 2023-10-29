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
.global vsync_width
.global vsync_comparison_lo
.global vsync_comparison_hi
.global hsync_period
.global total_hsync_period
.global hsync_comparison_lo
.global hsync_comparison_hi
.global hsync_width
.global sync_detected
.global last_sync_detected
.global last_but_one_sync_detected
.global delay_in_arm_cycles
.global get_cycle_counter
.global benchmarkRAM
.global jitter_offset
.global debug_value
.global param_ntscphase
.global ntsc_status
.global param_delay
.global param_intensity
.global param_sync_edge
.global sw1_power_up
.global osd_timer
.global palette_data_16
.global core_1_available
.global start_core_1_code
.global wait_for_source_fieldsync
.global poll_soft_reset

.global field_type_threshold
.global elk_lo_field_sync_threshold
.global elk_hi_field_sync_threshold
.global odd_threshold
.global even_threshold
.global hsync_threshold
.global normal_hsync_threshold
.global equalising_threshold
.global frame_minimum
.global line_minimum
.global frame_timeout
.global hsync_scroll
.global line_timeout
.global vsync_retry_count
.global wait_for_pi_fieldsync
.global scan_for_single_pixels_4bpp
.global scan_for_single_pixels_12bpp
.global scan_for_diffs_12bpp

#ifdef USE_MULTICORE
.global run_core
#endif

.global GPU_workspace

.global capture_line_normal_1bpp_table
.global capture_line_normal_3bpp_table
.global capture_line_normal_6bpp_table
.global capture_line_normal_odd_even_6bpp_table
.global capture_line_normal_9bpplo_table
.global capture_line_normal_9bpphi_table
.global capture_line_normal_12bpp_table


.global capture_line_odd_3bpp_table
.global capture_line_even_3bpp_table
.global capture_line_double_3bpp_table
.global capture_line_half_odd_3bpp_table
.global capture_line_half_even_3bpp_table

.global capture_line_simple_6bpp_table
.global capture_line_simple_9bpplo_table
.global capture_line_simple_9bpplo_blank_table
.global capture_line_simple_9bpphi_table
.global capture_line_simple_12bpp_table

#define FRAME_COUNT_MAX 7

        .align 6               // so cache loads align
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
        cmp    r2, #VSYNC_BLANKING
        moveq  r2, #VSYNC_AUTO
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
        ldr    r2, [r0, #O_INTENSITY]
        and    r2, r2, #0x0f
        eor    r2, r2, #0x0f
        mov    r2, r2, lsl #12
        orr    r2, r2, lsl #16
        str    r2, param_intensity
        ldr    r2, [r0, #O_AUTOSWITCH]
        str    r2, param_autoswitch
        ldr    r2, [r0, #O_TIMINGSET]
        str    r2, param_timingset
        ldr    r2, [r0, #O_SYNCEDGE]
        str    r2, param_sync_edge
        ldr    r2, [r0, #O_FB_BASE]
        str    r2, param_framebuffer0

        ldr    r9, ntsc_status
        and    r9, #NTSC_LAST_ARTIFACT | NTSC_LAST_IIGS        // last detected burst state & last IIGS state
        ldr    r8, param_ntscphase            // 3 bits phase + 1 bit artifact on/off + 1 bit Y invert
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
        ands   r10, r10, #SIZEX2_DOUBLE_HEIGHT
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
        bl     _get_GPLEV0_r4

        // Setup r3 with the flags/options parameter (as per before)
        mov    r3, r1
        bl     set_hardware_id_r3

        // Setup r2 with the framebuffer pitch (as per before)
        ldr    r2, param_fb_pitch

        // Setup r1 with the number of active characters per line (as per before) and adjust to number of psyncs required
        ldr    r1, param_chars_per_line
    //    ldr    r9, param_palette_control
    //    bic    r9, r9, #INHIBIT_PALETTE_DIMMING_16_BIT
    //    cmp    r9, #PALETTECONTROL_ATARI_GTIA
    //    movge  r1, r1, lsl #4
        mov    r1, r1, lsl #3
        ldr    r9, param_fb_sizex2
        ands   r9, r9, #SIZEX2_DOUBLE_WIDTH
        movne  r1, r1, lsr #1
        ldr    r9, param_sample_width
        cmp    r9, #SAMPLE_WIDTH_1
        moveq  r1, r1, lsr #3
        cmp    r9, #SAMPLE_WIDTH_3
        moveq  r1, r1, lsr #2
        cmp    r9, #SAMPLE_WIDTH_6
        moveq  r1, r1, lsr #1

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
        ldr    r9, param_fb_sizex2
        tst    r9, #SIZEX2_DOUBLE_HEIGHT
        bicne  r3, r3, #BIT_NO_LINE_DOUBLE
        orreq  r3, r3, #BIT_NO_LINE_DOUBLE
        tst    r9, #SIZEX2_BASIC_SCANLINES
        orrne  r3, r3, #BIT_NO_LINE_DOUBLE
        bicne  r9, #SIZEX2_DOUBLE_HEIGHT

        ldr    r8, param_video_type
        cmp    r8, #VIDEO_INTERLACED
        bne    skip_double_check
        tst    r3, #BIT_INTERLACED_VIDEO
        beq    skip_double_check
        ands   r8, r3, #MASK_INTERLACE
        orreq  r3, r3, #BIT_NO_LINE_DOUBLE
        biceq  r9, #1
skip_double_check:

        ldr    r7, param_sample_width
        ldr    r8, param_palette_control
        cmp    r7, #SAMPLE_WIDTH_9LO        // is sample width SAMPLE_WIDTH_9LO, SAMPLE_WIDTH_9HI, SAMPLE_WIDTH_12 (from above test)
        andge  r8, r8, #INHIBIT_PALETTE_DIMMING_16_BIT //leaving PALETTECONTROL_OFF  if so then force palette control to 0 but preserve inhibit palette dimming
        str    r8, param_palette_control
        bic    r8, r8, #INHIBIT_PALETTE_DIMMING_16_BIT

        ldr    r10, param_h_offset
        tst    r3, #BIT_NO_H_SCROLL
        subeqs r10, r10, #1
        moveq  r10, #1
        movmi  r10, #1

    //    cmp    r8, #PALETTECONTROL_ATARI_GTIA
    //    movge  r10, r10, lsl #1                // if so double the offset as sample rate is 2x pixel rate
        add    r10, r10, #1                    // first psync test is wait for a zero after csync
        str    r10, param_h_offset

        ldr    r7, param_fb_bpp
        cmp    r7, #8
        moveq  r7, #1
        movne  r7, #0

        // r7 0= 4 bpp or 16bpp, 1=8 bpp
        // r8 0=normal, 1= in band, 2=CGA ntsc, 3=mono ntsc, 4=auto mono ntsc, 5 = pal artifact, 6 = atari gtia
        // r9 0=normal, 1=Hx2, 2=Wx2, 3=H&Wx2

        orr    r10, r7, r8, lsl #1    // slow index in r10 now 0-9
        tst    r9, #2                 // double width?
        addne  r10, r10, #(NUM_CONTROLS << 1)          // slow index in r10 now 10-19

        add    r7, r7, #(NUM_CONTROLS << 2)            // main index initially points to fast 4bpp or fast 8bpp (20-21)

        cmp    r8, #0                 // palette control?
        tsteq  r9, #3                 // double size?
        tsteq  r3, #BIT_OLD_FIRMWARE_SUPPORT  // if version < 3 have to do the second PSYNC read
        movne  r7, r10                // if any are enabled make index point to non-fast versions

        ldr    r9, param_capture_line
        ldr    r8, [r9, r7, lsl #2]

        ldr    r9, param_video_type
        cmp    r9, #VIDEO_TELETEXT
        ldreq  r8, =capture_line_mode7_4bpp

        ldr    r10, param_border
        tst    r10, #0x80
        ldrne  r8, =capture_line_null

        str    r8, capture_address

        ldr    r8, =sentinel
        ldr    r9, =0x48444d49              // "HDMI" sentinel
        str    r9, [r8]

        mov    r8, #FRAME_COUNT_MAX                // number of frames before h and v sync timing is analysed
        str    r8, frame_countdown

        ldr    r8, dpms_state
        cmp    r8, #0
        movne  r8, #DPMS_FRAME_COUNT
        str    r8, dpmsframecount

        bl     restore_menu_bits
        str    r3, flag_state
        b      frame

        .align 6               // so cache loads align
frame:

        ldr    r8, sync_detected
        cmp    r8, #0
        bne    no_refresh
        push   {r0-r12}
        bl     refresh_cpld
        pop    {r0-r12}
no_refresh:

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
        cmp    r8, #VSYNC_AUTO
        cmpne  r8, #VSYNC_POLARITY
        beq    auto_detect_vsync
        cmp    r8, #VSYNC_INTERLACED_160  // interlaced half line
        orreq  r3, r3, #BIT_ELK
        ble    get_field_type           // branch for both types of interlace

        ldr    r7, param_autoswitch
        cmp    r7, #AUTOSWITCH_MODE7
        beq    auto_detect_vsync        // "if mode 7 detect enabled then don't use non-interlaced mode as it messes up mode 7 switching"
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

        ldr    r0, param_timingset

        ldr    r9, param_autoswitch
        cmp    r9, #AUTOSWITCH_MODE7
        bicne  r3, #BIT_INHIBIT_MODE_DETECT   //inhibit mode detect only works in mode7
        bne    no_mode7_test
        ldr    r8, sync_detected
        cmp    r8, #0
        beq    no_mode_test

        tst    r3, #BIT_FIELD_TYPE
        ldreq  r5, odd_threshold     // Use 22.5us threshold in odd field
        ldrne  r5, even_threshold    // Use 54.5us threshold in even field
        cmp    r6, r5
        movlt  r0, #0         // Modes 0-6
        movge  r0, #1         // Mode 7

no_mode7_test:
        cmp    r9, #AUTOSWITCH_VSYNC
        bne    no_vsync_test
glitch_detected:
        mov    r8, #VERSION_MASK
        str    r8, [r4, #-(GPLEV0_OFFSET - GPCLR0_OFFSET)]               //briefly switch to vsync on psync by clearing version bit
        ldr    r9, [r4]  // dummy read for delay
        ldr    r9, [r4]  // dummy read for delay
        ldr    r6, [r4]
        ldr    r9, [r4]  // dummy read for delay
        ldr    r7, [r4]
        str    r8, [r4, #-(GPLEV0_OFFSET - GPSET0_OFFSET)]               //restore version bit
        eor    r6, r6, r7            // eor together in case of glitches
        tst    r6, #PSYNC_MASK
        bne    glitch_detected
        tst    r7, #PSYNC_MASK
        moveq  r0, #0             //set 1
        movne  r0, #1             //set 2
        b      no_mode_test

no_vsync_test:
        cmp    r9, #AUTOSWITCH_IIGS
        cmpne  r9, #AUTOSWITCH_IIGS_MANUAL
        bne    no_mode_test
        ldr    r10, frame_countdown
        cmp    r10, #0
        bne    no_mode_test
        ldr    r7, vsync_detected
        ldr    r8, ntsc_status
        mov    r10, r8
        cmp    r7, #IIGS_DETECTED_LINE_COUNT
        biclt  r8, #NTSC_LAST_IIGS   // set1
        orrge  r8, #NTSC_LAST_IIGS   // set2
        str    r8, ntsc_status
        eor    r10, r10, r8
        mov    r8, r8, lsr #NTSC_LAST_IIGS_SHIFT
        and    r8, r8, #1
        cmp    r9, #AUTOSWITCH_IIGS
        moveq  r0, r8
        moveq  r10, #0  //so branch test fails
        tst    r10, #NTSC_LAST_IIGS
        beq    no_mode_test
        mov    r0, r8
        push   {r0 - r12}
        bl     set_timingset
        pop    {r0 - r12}
no_mode_test:
        tst    r3, #BIT_PROBE
        bne    exit
        tst    r3, #BIT_CALIBRATE
        bne    skip_switch_test

        // Test for keys being pressed, with variable rate auto repeat
        // Note: macro uses r5&r6 as a scratch register
        ldr    r8, [r4]
        ldr    r9, sw1_power_up

        bl     do_key_press_detect

        tst    r0, #RET_SW1
        moveq  r8, #0
        streq  r8, sw1_power_up
        cmpne  r9, #0
        bicne  r0, #RET_SW1

        tst    r0, #(RET_SW1 | RET_SW2 | RET_SW3)
        bne    exit

skip_switch_test:
        ldr    r9, sync_detected
        cmp    r9, #0
        beq    skip_interlace_test

        ldr    r9, frame_countdown
        cmp    r9, #FRAME_COUNT_MAX
        bne    not_first_frame
        subs   r9, r9, #1
        str    r9, frame_countdown
        ldr    r9, param_detected_sync_type
        tst    r9, #SYNC_BIT_INTERLACED
        tstne  r3, #BIT_OSD
        tstne  r3, #BIT_FIELD_TYPE    // test for interlaced sync with OSD on and wrong field
        bne    frame                  // wait for next fieldsync so always start on the same field to prevent interlace 'bounce'
not_first_frame:
        cmp    r9, #0
        bne    skip_interlace_test

        tst    r3, #BIT_INHIBIT_MODE_DETECT
        bne    skip_mode_detect

        ldr    r5, param_timingset
        cmp    r5, r0         // Check if we have changed mode
        bne    exit           // If so, then bail, as the frame buffer needs to be resized
skip_mode_detect:

        ldr    r9, param_autoswitch
        cmp    r9, #AUTOSWITCH_MODE7
        beq    force_interlace_test

        ldr    r9, param_video_type
        cmp    r9, #VIDEO_INTERLACED
        beq    force_interlace_test

        ldr    r9, param_vsync_type
        cmp    r9, #VSYNC_INTERLACED
        beq    force_interlace_test
        cmp    r9, #VSYNC_INTERLACED_160
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

        ldr    r7, last_but_one_sync_detected
        ldr    r8, last_sync_detected
        ldr    r9, sync_detected
        str    r8, last_but_one_sync_detected
        str    r9, last_sync_detected
        ldr    r10, param_border
        cmp    r7, #0
        cmpne  r8, #1
        cmpne  r9, #1
        cmpne  r10, #0
        beq    no_sync_loss
        bl     clear_screen              // clear non-zero border on loss of sync
        b      skip_all_lines
no_sync_loss:
        cmp    r7, #0
        cmpeq  r8, #0
        cmpeq  r9, #1
        ldreq  r0, param_timingset
        orreq  r0, #RET_SYNC_STATE_CHANGED
        beq    exit                       // if sync just returned, bail to allow recalculation of sampling clock etc

        cmp    r8, #0                         // if sync lost for 2 fields then increase hsync threshold
        cmpeq  r9, #0
        ldreq  r5, normal_hsync_threshold
        streq  r5, hsync_threshold

        // Save a copy of the frame buffer base
        push   {r11}

        ldr    r5, param_framebuffer0
        ldr    r6, param_framebuffer1
        sub    r5, r6, r5
        str    r5, video_offset

        ldr    r5, param_v_offset

        ldr    r9, param_vsync_type
        cmp    r9, #VSYNC_NONINTERLACED_DEJITTER   //flywheel vsync
        bne    skip_fix_vsync_jitter


        bl     show_vsync
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

        ldr    r9, param_video_type
        cmp    r9, #VIDEO_INTERLACED // (primarily amiga)
        bne    nointadj
        tst    r3, #BIT_INTERLACED_VIDEO //is video interlaced?
        beq    nointadj
        tst    r3, #BIT_FIELD_TYPE
        addne  r5, r5, #1     // add one line in even fields
        addne  r11, r11, r2
        subeq  r11, r11, r2
nointadj:
        ldr    r6, param_fb_sizex2
        eor    r6, r6, #SIZEX2_DOUBLE_HEIGHT
        tst    r6, #SIZEX2_DOUBLE_HEIGHT                     //now 0 if double height
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
        bl     clear_vsync
        ldr    r6, param_nlines
        add    r5, r5, r6
        str    r5, total_lines
        //str    r5, vsync_line          // default for vsync line if undetectable in blanking area
        mov    r9, #0
skip_line_loop:
        str    r9, hsync_measured
        cmp    r5, r6
        ble    skip_line_loop_exit
        bl     show_vsync
        WAIT_FOR_CSYNC_0_LONG
        READ_CYCLE_COUNTER r12
        WAIT_FOR_CSYNC_1_LONG
        READ_CYCLE_COUNTER r10
        subs   r9, r10, r12
        rsbmi  r9, r9, #0
        subs   r5, r5, #1
        b      skip_line_loop
skip_line_loop_exit:

        push   {r1-r5, r11}
        ldr    r12, capture_address
        sub    r12, r12, #4
        orr    r3, r3, #BIT_OSD
        // Call preload capture line function (runs all paths of capture code to preload it into cache)
        blx    r12
        pop    {r1-r5, r11}
        mov    r6, #0
        str    r6, total_hsync_period

        // Compute the current scanline mod 10
        ldr    r6, param_v_offset
        add    r6, r6, #1
mod10:
        subs   r6, r6, #10
        bpl    mod10
        add    r6, r6, #10

        // Process active lines
        ldr    r5, param_nlines
        ldr    r7, param_h_offset
        ldr    r8, video_offset
        ldr    r9, hsync_scroll

        ldr    r12, capture_address

        //pre cache the stack
        push   {r0-r11}
        push   {r0-r11}
        pop    {r0-r11}
        pop    {r0-r11}


        mov    r10, #0
        str    r10, detectedlinecount
        str    r10, vsync_detected
        //preload some variables into cache
        ldr    r10, last_hsync_time
        ldr    r10, hsync_period
        ldr    r10, param_nlines
        ldr    r10, total_hsync_period
        ldr    r10, param_fb_sizex2
        ldr    r10, vsync_line

        push   {r3}

        bic    r3, #BITDUP_ENABLE_GREY_DETECT | BITDUP_ENABLE_FFOSD
        bic    r3, #BITDUP_RGB_INVERT | BITDUP_Y_INVERT
        ldr    r0, ntsc_status
        tst    r0, #NTSC_HDMI_BLANK_ENABLE
        orrne  r3, r3, #BITDUP_ENABLE_GREY_DETECT
        tst    r0, #NTSC_FFOSD_ENABLE
        orrne  r3, r3, #BITDUP_ENABLE_FFOSD
        tst    r0, #NTSC_RGB_INVERT
        orrne  r3, r3, #BITDUP_RGB_INVERT
        tst    r0, #NTSC_Y_INVERT
        orrne  r3, r3, #BITDUP_Y_INVERT

        bic    r3, #BITDUP_IIGS_DETECT
        ldr    r0, param_autoswitch
        cmp    r0, #AUTOSWITCH_IIGS
        cmpne  r0, #AUTOSWITCH_IIGS_MANUAL
        orreq  r3, #BITDUP_IIGS_DETECT

        bic    r3, #BITDUP_FFOSD_DETECTED

        tst    r3, #BIT_OSD
        orrne  r3, r3, #BIT_NO_SCANLINES   //disable scanlines if OSD on

        ldr    r0, param_palette_control
        tst    r0, #INHIBIT_PALETTE_DIMMING_16_BIT
        bicne  r3, r3, #BIT_OSD

        bic    r3, r3, #BIT_NO_SKIP_HSYNC
        b      process_line_loop
        .align 6

GPU_workspace:
        .word 0
        .word 0
        .word 0
        .word 0

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

param_intensity:
        .word 0

param_autoswitch:
        .word 0

param_timingset:
        .word 0

param_sync_edge:
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

buffer_total:
        .word 1

ntsc_status:
        .word 0
.ltorg

        .align  6

process_line_loop:

        bl     show_vsync
        // Preserve the state used by the outer code
        push   {r1, r2, r4-r9, r11, r12}

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

        mov    r0, r11

        bic    r3, #BITDUP_LINE_CONDITION_DETECTED

        tst    r3, #BIT_NO_SKIP_HSYNC
        ldreq  r1, [r0]
        streq  r1, [r0]

        moveq  r1, #8   //number of psyncs to capture
        moveq  r7, #4  //number of psyncs to skip
        ldreq   r0, =(dummyscreen + 1024)
        // Call capture line function
        blx    r12 // exits with h sync timestamp in r0

        // Restore the state used by the outer code
        pop    {r1, r2, r4-r9, r11, r12}
        tst    r3, #BIT_NO_SKIP_HSYNC
        orreq  r3, r3, #BIT_NO_SKIP_HSYNC
        biceq  r3, #BITDUP_FFOSD_DETECTED
        beq    process_line_loop

        mov    r14, #0
        tst    r3, #BITDUP_IIGS_DETECT
        movne  r10, #VERSION_MASK
        strne  r10, [r4, #-(GPLEV0_OFFSET - GPCLR0_OFFSET)]               //briefly switch to vsync on psync by clearing version bit
        ldrne  r14, [r4]  // dummy read for delay
        ldrne  r14, [r4]  // dummy read for delay
        ldrne  r14, [r4]
        strne  r10, [r4, #-(GPLEV0_OFFSET - GPSET0_OFFSET)]               //restore version bit
        ldrne  r10, vsync_detected
        tst    r14, #CSYNC_MASK
        addne  r10, r10, #1
        strne  r10, vsync_detected

        tst    r3, #BITDUP_LINE_CONDITION_DETECTED
        ldrne  r10, detectedlinecount
        addne  r10, r10, #1
        strne  r10, detectedlinecount

        ldr    r10, last_hsync_time
        str    r0, last_hsync_time
        subs   r10, r0, r10
        rsbmi  r10, r10, #0
        str    r10, hsync_period
        ldr    r0, param_nlines
        cmp    r0, r5                 //ignore 1st line as time undefined
        ldrne  r0, total_hsync_period
        addne  r0, r0, r10
        strne  r0, total_hsync_period

        ldr    r10, param_fb_sizex2
        tst    r10, #SIZEX2_DOUBLE_HEIGHT
        // Skip a whole line to maintain aspect ratio
        addne  r11, r11, r2, lsl #1
        addeq  r11, r11, r2
        add    r6, r6, #1
        cmp    r6, #10
        moveq  r6, #0

        subs   r5, r5, #1
        bne    process_line_loop

        ldr    r5, flag_state
        and    r5, r5, #BIT_OSD
        and    r7, r3, #BIT_INHIBIT_MODE_DETECT
        tst    r3, #BITDUP_FFOSD_DETECTED
        pop    {r3}
        orrne  r3, r3, #BIT_OSD       //enable OSD if FFOSD detected
        biceq  r3, r3, #BIT_OSD
        orreq  r3, r5                 //otherwise restore original OSD state
        bic    r3, r3, #BIT_INHIBIT_MODE_DETECT
        orr    r3, r3, r7    //persist inhibit after R3 restored

        tst    r3, #BIT_INHIBIT_MODE_DETECT
        movne  r7, #0
        strne  r7, param_timingset
        pop    {r11}

skip_all_lines:
        push   {r1-r12}
        ldr    r7, detectedlinecount

        ldr    r9, param_palette_control
        bic    r9, r9, #INHIBIT_PALETTE_DIMMING_16_BIT
        cmp    r9, #PALETTECONTROL_NTSCARTIFACT_BW_AUTO
        bne    check_dpms

        ldr    r6, param_fb_sizex2
        tst    r6, #SIZEX2_DOUBLE_WIDTH
        moveq  r8, #ARTIFACT_DETECTED_LINE_COUNT
        movne  r8, #1

        ldr    r9, ntsc_status
        mov    r11, r9
        cmp    r7, r8
        bic    r9, r9, #NTSC_LAST_ARTIFACT
        orrge  r9, r9, #NTSC_LAST_ARTIFACT

        tst    r6, #SIZEX2_DOUBLE_WIDTH
        eorne  r9, r9, #NTSC_LAST_ARTIFACT

        eor    r11, r11, r9
        tst    r11, #NTSC_LAST_ARTIFACT
        mov    r11, r9, lsr #(NTSC_LAST_ARTIFACT_SHIFT - NTSC_ARTIFACT_SHIFT)
        and    r11, r11, #NTSC_ARTIFACT

        bicne  r9, r9, #NTSC_ARTIFACT
        orrne  r9, r9, r11
        str    r9, ntsc_status

        b      done_ntsc_auto

check_dpms:
        ldr    r8, dpmsframecount
        tst    r3, #BIT_OSD                  //if osd on then disable grey screen
        movne  r8, #0
        ldr    r9, dpms_state
        cmp    r7, #GREY_DETECTED_LINE_COUNT
        movlt  r8, #0
        addge  r8, r8, #1
        cmp    r8, #DPMS_FRAME_COUNT
        strle  r8, dpmsframecount
        movlt  r0, #0
        movge  r0, #1
        str    r0, dpms_state
        cmp    r0, r9
        blne   DPMS

done_ntsc_auto:
        ands   r0, r3, #BIT_OSD
        movne  r0, #1
        ldr    r9, ntsc_status
        tst    r9, #NTSC_FFOSD_ENABLE
        blne   osd_write_palette

        pop    {r1-r12}

        ldr    r8, frame_countdown
        subs   r8, r8, #1
        strpl  r8, frame_countdown
        bpl    skip_sync_time_test

        tst    r3, #BIT_OSD | BIT_CALIBRATE | BIT_PROBE
        bne    skip_sync_time_test

        ldr    r0, param_timingset
        orr    r0, #RET_SYNC_TIMING_CHANGED | RET_SYNC_POLARITY_CHANGED

        ldr    r9, param_autoswitch
        cmp    r9, #AUTOSWITCH_OFF
        beq    skip_hsync_time_test

        ldr    r8, param_vsync_type
        cmp    r8, #VSYNC_POLARITY
        bne    skip_hsync_time_test

        ldr    r8, hsync_measured                //check measured hsync width
        ldr    r9, hsync_threshold               //if > threshold then hsync likely inverted and active line has been measured
        rsb    r7, r9, #LINE_TIMEOUT   //r7 is now timeout length minus sync threshold (~100us - 9uS)
        cmp    r7, r8    //is timeout length -9uS > measured hsync  (i.e. genuine reading, not a timeout)
        cmpgt  r8, r9   //if yes is measured hsync > 9uS
        bgt    exit

skip_hsync_time_test:
        ldr    r8, sync_detected
        cmp    r8, #0
        beq    skip_sync_time_test           // if no sync then timing comparison is meaningless

        ldr    r7, hsync_comparison_lo
        ldr    r8, hsync_comparison_hi
        cmp    r7, r8
        beq    skip_sync_time_test            //no sensible window

        ldr    r0, param_timingset
        orr    r0, #RET_SYNC_TIMING_CHANGED

        ldr    r6, total_hsync_period
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
        bgt    exit

//        ble    no_test_half
//        mov    r6, r6, lsr #1                // workaround to check for half the time in case frame dropped due to genlock or palette update taking too long
//        cmp    r6, r7
//        blt    exit
//        cmp    r6, r8
//        bgt    exit
//no_test_half:

        ldr    r9, param_autoswitch
        cmp    r9, #AUTOSWITCH_OFF
        beq    skip_sync_time_test

        ldr    r8, param_vsync_type
        cmp    r8, #VSYNC_POLARITY
        bne    skip_sync_time_test


        ldr    r8, param_sync_type
        tst    r8, #SYNC_BIT_COMPOSITE_SYNC       //set if composite so don't check vsync polarity
        bne    skip_sync_time_test

        orr    r0, #RET_SYNC_POLARITY_CHANGED

        mov    r8, #VERSION_MASK
        str    r8, [r4, #-(GPLEV0_OFFSET - GPCLR0_OFFSET)]             //briefly switch to vsync on psync by clearing version bit
        ldr    r9, [r4]  // dummy read for delay
        ldr    r9, [r4]  // dummy read for delay
        ldr    r6, [r4]
        ldr    r9, [r4]  // dummy read for delay
        ldr    r7, [r4]
        str    r8, [r4, #-(GPLEV0_OFFSET - GPSET0_OFFSET)]                //restore version bit

        ldr    r8, param_sync_type
        tst    r8, #SYNC_BIT_VSYNC_INVERTED
        eorne  r6, r6, #PSYNC_MASK
        eorne  r7, r7, #PSYNC_MASK
        orr    r6, r6, r7            // or together in case of glitches
        tst    r6, #PSYNC_MASK
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
        cmp    r0, #VIDEO_TELETEXT
        beq    skip_osd_update
        tst    r3, #BIT_OSD | BIT_SKIP_ALT_FRAME
        beq    skip_osd_update

        push   {r1-r5, r11}
        push   {r3}
        mov    r0, #0 //do not force genlock
        bl     recalculate_hdmi_clock_line_locked_update
        pop    {r3}
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
        push   {r3, r4}
        mov    r0, #0 //do not force genlock
        bl     recalculate_hdmi_clock_line_locked_update
        pop    {r3, r4}
        // Returns:
        //   r0=0 genlock disabled           - LED off
        //   r0=1 genlock enabled (unlocked) - LED flash
        //   r0=2 genlock enabled (locked)   - LED on

        ldr    r2, sync_detected
        ldr    r11, last_sync_detected
        orr    r2, r2, r11
        ldr    r11, last_but_one_sync_detected
        orr    r2, r2, r11           //sync gone for 3 frames?
        cmp    r2, #0
        moveq  r0, #0               //if no sync switch off genlock led
        moveq  r5, #MODE7_MASK
        subeq  r11, r4, #(GPLEV0_OFFSET - GPSET0_OFFSET)
        streq  r5, [r11]             //switch on mode7 led if no sync so at least one led is always lit (will be set correctly when sync reacquired)
        READ_CYCLE_COUNTER r1
        mov    r2, #LED1_MASK
        tst    r0, #1         // should LED flash?
        tstne  r1, #(1 << 26) // flash rate ~ 8Hz
        tsteq  r0, #2         // should LED be on?
        subne  r1, r4, #(GPLEV0_OFFSET - GPSET0_OFFSET)
        subeq  r1, r4, #(GPLEV0_OFFSET - GPCLR0_OFFSET)
        str    r2, [r1]

        pop    {r1-r5, r11}

        ldr    r5, osd_timer
        subs   r5, r5, #1
        strpl  r5, osd_timer

        // Loop back if required number of fields has not been reached
        // or if negative (capture forever)
        ldr    r5, param_ncapture
        cmp    r5, #0
        blt    frame
        subs   r5, #1
        str    r5, param_ncapture
        bne    frame

        // Setup the response code
        ldr    r0, param_timingset
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
        tst    r9, #NTSC_ARTIFACT
        moveq  r0,#0
        movne  r0,#1
        bl     set_ntsccolour
        pop    {r0}
        pop    {r4 - r12, pc}


// ======================================================================
// Local Variables
// ======================================================================
        .align 6
capture_address:
        .word 0
dpmsframecount:
        .word 0
dpms_state:
        .word 0
flag_state:
        .word 0

frame_countdown:
        .word 0

sw1counter:
        .word 0

sw2counter:
        .word 0

sw3counter:
        .word 0

hsync_period:
        .word 0

total_hsync_period:
        .word 0

last_hsync_time:
        .word 0

vsync_line:
        .word 0

vsync_detected:
        .word 0



detectedlinecount:
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

vsync_period_1:
        .word 0

vsync_width:
        .word 0

vsync_comparison_lo:
        .word 0

vsync_comparison_hi:
        .word 0

hsync_comparison_lo:
        .word 0

hsync_comparison_hi:
        .word 0

hsync_width:
        .word 0


sync_detected:
        .word 1

last_sync_detected:
        .word 1

last_but_one_sync_detected:
        .word 1

last_scanlines_state:
        .word 0

video_offset:
        .word 0

jitter_offset:
        .word 0

debug_value:
        .word 0

first_hsync_timestamp:
        .word 0

sw1_power_up:
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
normal_hsync_threshold:
        .word 9000
equalising_threshold:
        .word 3250
frame_minimum:
        .word 10000000
line_minimum:
        .word 20000

hsync_scroll:
        .word (4000 - 224) | ((4000 + 224) << 16)
line_timeout:
        .word 100000000
vsync_retry_count:
        .word 0
osd_timer:
        .word 0
hsync_measured:
        .word 0

        .ltorg

frame_timeout:
        .word 24000000

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
        bl     set_hardware_id_r3
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

        ldr    r11, hsync_threshold             // normally 9us
        add    r11, r11, r11, lsr #1            // 13.5us
wait_hi_20:                                     // make sure sync high for > 13.5us to avoid malformed vsync pulses (e.g. Apple II)
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
        b      exit_vsync


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

        READ_CYCLE_COUNTER r5             //save start of vsync
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

        mov    r8, #VERSION_MASK
        str    r8, [r4, #-(GPLEV0_OFFSET - GPSET0_OFFSET)]                //restore version bit
        ldr    r7, [r4]  // dummy read for delay
        ldr    r7, [r4]  // dummy read for delay

        mov    r7, r6

        WAIT_FOR_CSYNC_1_LONG                   // resync with hsync

        ldr    r9, param_video_type
        cmp    r9, #VIDEO_INTERLACED
        bne    skip_interlace_detect

        // Wait for the falling edge of csync
        WAIT_FOR_CSYNC_0_LONG
        // Wait for the rising edge of hsync
        WAIT_FOR_CSYNC_1_LONG       //puts entry time in r14 recording time of the falling edge above
        // Record time of the rising edge
        READ_CYCLE_COUNTER r6

skip_interlace_detect:
        cmp    r10, #0
        moveq  r7, r6                           // make all fields look the same
        moveq  r5, r7                           // set length of vsync pulse to 0 so electron test fails

        SWITCH_VSYNC_TO_PSYNC

        ldr    r8, last_vsync_time
        str    r7, last_vsync_time
        subs   r8, r7, r8
        rsbmi  r8, r8, #0

exit_vsync:
        ldr    r9, vsync_period_1
        str    r8, vsync_period_1
        add    r8, r8, r9
        mov    r8, r8, lsr #1
        str    r8, vsync_period
        pop    {pc}

// ======================================================================
// MEASURE_N_LINES
// ======================================================================

// Note: this is coded as a single loop with conditional mrc instructions
// to mitigate the effect of I-Cache misses.

measure_n_lines:
        push   {r4-r12, lr}
        mov    r3, #0
        bl     set_hardware_id_r3
        // Setup R4 as a constant
        bl     _get_GPLEV0_r4

        // wait for vsync
        bl     wait_for_vsync

        bl     do_measure_n_lines

        subs   r11, r12, r11
        rsbmi  r11, r11, #0               //r11 now hsync pulse width
        str    r11, hsync_width

        subs   r10, r10, r6
        rsbmi  r10, r10, #0
        str    r10, hsync_period          //initial value for hsync_period

        subs   r0, r6, r7
        rsbmi  r0, r0, #0

        ldr    r8, last_sync_detected
        ldr    r9, sync_detected
        and    r9, r9, r8
        str    r9, last_sync_detected            //make a sync fail persist over the measurement
        pop    {r4-r12, pc}

// ======================================================================
// MEASURE_VSYNC
// ======================================================================

measure_vsync:
        push    {r4-r12, lr}

        mov    r3, #0
        bl     set_hardware_id_r3
        // Setup R4 as a constant
        bl     _get_GPLEV0_r4
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

        subs   r8, r6, r5        // work out length of vsync pulse
        rsbmi  r8, r0, #0
        str    r8, vsync_width

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
// ANALYSE SYNC POLARITY
// ======================================================================
analyse_sync:
        push    {r4-r12, lr}
        bl     set_hardware_id_r3
        bl     _get_GPLEV0_r4
        mov    r6, #0 //csync low
        mov    r7, #0 //csync high
        READ_CYCLE_COUNTER r10
analyse_hloop:
        ldr    r5, [r4]                            // dummy read for delay
        tst    r5, #CSYNC_MASK
        addeq  r6, r6, #1
        addne  r7, r7, #1
        READ_CYCLE_COUNTER r11
        subs   r12, r10, r11
        rsbmi  r12, r12, #0
        ldr    r5, frame_timeout
        cmp    r12, r5
        blt    analyse_hloop

        SWITCH_PSYNC_TO_VSYNC

        mov    r8, #0 //vsync low
        mov    r9, #0 //vsync high
        READ_CYCLE_COUNTER r10
analyse_vloop:
        ldr    r5, [r4]
        ldr    r11, [r4]           //delay
        ldr    r11, [r4]
        eor    r11, r11, r5
        tst    r11, #PSYNC_MASK      //deglitch vsync
        bne    analyse_vloop
        tst    r5, #PSYNC_MASK                     // actually vsync when version = 0
        ldr    r5, frame_timeout
        addeq  r8, r8, #1
        addne  r9, r9, #1
        READ_CYCLE_COUNTER r11
        subs   r12, r10, r11
        rsbmi  r12, r12, #0
        cmp    r12, r5
        blt    analyse_vloop

//str r6, hsync_comparison_lo
//str r7, hsync_comparison_hi
//str r8, vsync_comparison_lo
//str r9, vsync_comparison_hi

        mov    r0, #0
        cmp    r6, r7                              // is low time > high time
        orrgt  r0, #SYNC_BIT_HSYNC_INVERTED        // inverted means positive going
        cmp    r8, r9                              // is low time > high time
        orrgt  r0, r0, #SYNC_BIT_VSYNC_INVERTED    // inverted means positive going

        SWITCH_VSYNC_TO_PSYNC

        pop    {r4-r12, pc}
        .ltorg


// ======================================================================
// CLEAR_SCREEN
// ======================================================================

clear_screen:
        push   {r4-r12, lr}
        ldr    r5, =param_fb_height
        ldr    r5, [r5]
        ldr    r6, =param_fb_pitch
        ldr    r6, [r6]
        ldr    r11, =param_framebuffer0
        ldr    r11, [r11]

        mul    r6, r5, r6
#ifdef MULTI_BUFFER
        ldr    r5, =buffer_total
        ldr    r5, [r5]
        //mov    r5, #NBUFFERS
        mul    r6, r5, r6
#endif
        ldr    r7, =param_fb_bpp
        ldr    r7, [r7]
        ldr    r9, =param_border
        ldr    r9, [r9]
        ldr    r10, =sync_detected
        ldr    r10, [r10]
        tst    r9, #0x80
        cmpeq  r10, #0
        moveq  r9, #0
        andne  r9, r9, #0x3f
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
        ldr    r5, =param_fb_height
        ldr    r5, [r5]
        ldr    r6, =param_fb_pitch
        ldr    r6, [r6]
        ldr    r11, =param_framebuffer0
        ldr    r11, [r11]
        mul    r6, r5, r6
#ifdef MULTI_BUFFER
        ldr    r5, =buffer_total
        ldr    r5, [r5]
        //mov    r5, #NBUFFERS
        mul    r6, r5, r6
#endif
        mov    r7, #0
clearfull:
        subs   r6, r6, #4
        str    r7, [r11], #4
        bne    clearfull
        pop    {r4-r12, pc}

        .ltorg
// ======================================================================
// CLEAR_MENU_BITS
// ======================================================================

clear_menu_bits:
        ldr    r0, =param_fb_bpp
        ldr    r0, [r0]
        cmp    r0, #16 //16 BPP
        movge  pc, lr
        ldr    r3, =last_scanlines_state
        ldr    r3, [r3]
        tst    r3, #BIT_NO_SCANLINES | BIT_PROBE | BIT_INTERLACED_VIDEO
        movne  pc, lr
        push   {r4-r12, lr}
        ldr    r5, =param_fb_height
        ldr    r5, [r5]
        ldr    r6, =param_fb_pitch
        ldr    r6, [r6]
        ldr    r11, =param_framebuffer0
        ldr    r11, [r11]
        ldr    r7, =param_fb_bpp
        ldr    r7, [r7]

        ldr    r8, =0x88888888
        ldr    r9, =0x80808080
        cmp    r7, #4
        movne  r8, r9
        mul    r6, r5, r6
#ifdef MULTI_BUFFER
        ldr    r5, =buffer_total
        ldr    r5, [r5]
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
        ldr    r0, =param_fb_bpp
        ldr    r0, [r0]
        cmp    r0, #16 //16 BPP
        movge  pc, lr
        ldr    r0, =last_scanlines_state
        str    r3, [r0]
        tst    r3, #BIT_NO_SCANLINES | BIT_PROBE | BIT_INTERLACED_VIDEO
        movne  pc, lr
        ldr    r0, =param_ncapture
        ldr    r0, [r0]
        cmp    r0, #0
        movpl  pc, lr
        push   {r4-r12, lr}
        bl     wait_for_vsync
        ldr    r7, =param_fb_bpp
        ldr    r7, [r7]
        ldr    r8, =0x88888888
        ldr    r9, =0x80808080
        cmp    r7, #4
        movne  r8, r9
        ldr    r11, =param_framebuffer0
        ldr    r11, [r11]
#ifdef MULTI_BUFFER
        ldr    r7, =buffer_total
        ldr    r7, [r7]
        //mov    r7, #NBUFFERS
#else
        mov    r7, #1
#endif
        ldr    r12, =param_fb_pitch
        ldr    r12, [r12]
restfull3:
        ldr    r6, =param_fb_height
        ldr    r6, [r6]
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
        ldr    r12, =param_border
        ldr    r12, [r12]
        cmp    r12, #248
        bge    do_colour_bars
        ldr    r8, =param_fb_bpp
        ldr    r8, [r8]
        cmp    r8, #4
        moveq  r1, r1, lsr #3
        movne  r1, r1, lsr #2
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
do_colour_bars:
        SKIP_PSYNC
        ldr    r8, =param_fb_bpp
        ldr    r8, [r8]
        cmp    r8, #4
        moveq  r6, r1, lsr #3+3
        movne  r6, r1, lsr #2+3
        cmp    r6, #0
        moveq  r6, #1

        mov    r12, #7
        bl     draw_bar
        mov    r12, #3
        bl     draw_bar
        mov    r12, #6
        bl     draw_bar
        mov    r12, #2
        bl     draw_bar
        mov    r12, #5
        bl     draw_bar
        mov    r12, #1
        bl     draw_bar
        mov    r12, #4
        bl     draw_bar
        mov    r12, #0
        bl     draw_bar
        pop     {r0, pc}
draw_bar:
        push {r14}
        cmp    r8, #4
        orreq  r12, r12, lsl #4
        orr    r12, r12, lsl #8
        orr    r12, r12, lsl #16
        mov    r1, r6
loop_null_bar:
        mov    r7, r12
        mov    r10, r12
        WRITE_R7_R10
        subs    r1, r1, #1
        bne     loop_null_bar
        pop     {pc}

        .ltorg

set_hardware_id_r3:
        push   {r0, lr}
        bl     _get_hardware_id
        cmp    r0, #_RPI2
        biclt  r3, r3, #BIT_RPI234
        orrge  r3, r3, #BIT_RPI234
        pop    {r0, pc}

clear_vsync:
        push   {lr}
        // Clear the VSYNC interrupt
        bl     _get_peripheral_base
        add    r0, r0, #SMICTRL_OFFSET
        bic    r3, r3, #BIT_VSYNC_MARKER
        mov    r10, #0
        str    r10, [r0]
        // Don't proceed until this write is complete
        _DSB
        pop    {pc}


show_vsync:
        push   {lr}
        bic    r3, r3, #BIT_VSYNC_MARKER
        // Poll for the VSYNC interrupt
        bl     _get_peripheral_base
        ldr    r14, =INTPEND2_OFFSET
        add    r0, r0, r14
        ldr    r0, [r0]
        tst    r0, #(1<<VSYNCINT)
        beq    novsync
        // Clear the VSYNC interrupt
        bl     clear_vsync
        // If the vsync indicator is enabled, mark the next line in red
        tst    r3, #(BIT_VSYNC)
        orrne  r3, r3, #BIT_VSYNC_MARKER
        // Remember the line where vsync occurred
        ldr    r14, =vsync_line
        str    r5, [r14]
novsync:
        pop    {pc}


do_measure_n_lines:
        push   {lr}
        // skip 20 lines so we are well away from any double vsync pulses
        add    r1, r0, #20
        add    r0, r0, #1

        // r1 is the loop counter
        tst    r3, #BIT_RPI234
        beq    measure_n_loop_rpi0_1
measure_n_loop_rpi2_4:
        mrc    p15, 0, r11, c9, c13, 0
        WAIT_FOR_CSYNC_1_LONG
        mrc    p15, 0, r12, c9, c13, 0
        WAIT_FOR_CSYNC_0_LONG
        cmp    r1, r0
        mrceq  p15, 0, r7, c9, c13, 0
        subs   r1, r1, #1
        mrceq  p15, 0, r6, c9, c13, 0
        bne    measure_n_loop_rpi2_4
        b      done_measure_n_loop_rpi2_4
measure_n_loop_rpi0_1:
        mrc    p15, 0, r11, c15, c12, 1
        WAIT_FOR_CSYNC_1_LONG
        mrc    p15, 0, r12, c15, c12, 1
        WAIT_FOR_CSYNC_0_LONG
        cmp    r1, r0
        mrceq  p15, 0, r7, c15, c12, 1
        subs   r1, r1, #1
        mrceq  p15, 0, r6, c15, c12, 1
        bne    measure_n_loop_rpi0_1
done_measure_n_loop_rpi2_4:

        WAIT_FOR_CSYNC_1_LONG
        WAIT_FOR_CSYNC_0_LONG
        READ_CYCLE_COUNTER r10
        pop    {pc}

do_key_press_detect:
        tst    r8, #SW1_MASK
        tsteq  r8, #SW3_MASK
        beq    softreset
        KEY_PRESS_DETECT SW1_MASK, RET_SW1, sw1counter
        KEY_PRESS_DETECT SW2_MASK, RET_SW2, sw2counter
        KEY_PRESS_DETECT SW3_MASK, RET_SW3, sw3counter
        mov     pc, lr

poll_soft_reset:
        push   {r0-r12,lr}
        bl     _get_GPLEV0_r4
        ldr    r8, [r4]
        tst    r8, #SW1_MASK
        tsteq  r8, #SW3_MASK
        beq    softreset
        pop    {r0-r12, pc}

softreset:
        ldr    r8, [r4]
        //tst    r8, #SW1_MASK
        //tst    r8, #SW2_MASK
        tst    r8, #SW3_MASK
        beq    softreset
        bl     reboot
        b      softreset
// ======================================================================
// delay_in_arm_cycles()
// ======================================================================

delay_in_arm_cycles:
        push   {lr}
        bl     set_hardware_id_r3
        READ_CYCLE_COUNTER r1
delay_loop:
        READ_CYCLE_COUNTER r2
        subs   r2, r1
        rsbmi  r2, r2, #0
        cmp    r2, r0
        blt    delay_loop
        pop    {pc}

get_cycle_counter:
        push   {r3, lr}
        bl     set_hardware_id_r3
        READ_CYCLE_COUNTER r0
        pop    {r3, pc}


benchmarkRAM:
        push   {r1-r12, lr}
        mov    r3, #0
        bl     set_hardware_id_r3
        cmp    r0, #1
        beq    gpu_bench
        cmp    r0, #2
        beq    gpu_bench
        cmp    r0, #3
        beq    gpio_bench
        cmp    r0, #4
        beq    mbox_bench_1
        cmp    r0, #5
        beq    mbox_bench_3
        // RAM address in r0 returns with time in r0
        mov    r1, r0
        add    r2, r1, #4000
preload_benchloop2:
        ldr    r6, [r1], #4
        cmp    r1, r2
        blt    preload_benchloop2
        READ_CYCLE_COUNTER r4
        mov    r7, #100
benchloop:
        mov    r1, r0
        add    r2, r1, #4000
benchloop2:
        ldr    r6, [r1], #4
        cmp    r1, r2
        blt    benchloop2
        subs   r7, r7, #1
        bne    benchloop
        READ_CYCLE_COUNTER r5
        subs   r0, r5, r4
        rsbmi  r0, r0, #1
        pop   {r1-r12, pc}

gpio_bench:
        bl     _get_GPLEV0_r4
        ldr    r1, =100000
        READ_CYCLE_COUNTER r6
gpio_bench_loop:
        ldr    r8, [r4]
        subs   r1, r1, #1
        bne    gpio_bench_loop
        READ_CYCLE_COUNTER r7
        subs   r0, r7, r6
        rsbmi  r0, r0, #1
        pop   {r1-r12, pc}

mbox_bench_1:
        bl     _get_gpu_data_base_r4
        ldr    r1, =100000
        READ_CYCLE_COUNTER r6
mbox_bench_1_loop:
        ldr    r8, [r4]
        subs   r1, r1, #1
        bne    mbox_bench_1_loop
        READ_CYCLE_COUNTER r7
        subs   r0, r7, r6
        rsbmi  r0, r0, #1
        pop   {r1-r12, pc}

mbox_bench_3:
        bl     _get_gpu_data_base_r4
        ldr    r1, =100000
        READ_CYCLE_COUNTER r6
mbox_bench_3_loop:
        ldr    r11, [r4, #8]
        ldmia  r4, {r8, r9}   //test ldm on an unaligned register pair
        subs   r1, r1, #1
        bne    mbox_bench_3_loop
        READ_CYCLE_COUNTER r7
        subs   r0, r7, r6
        rsbmi  r0, r0, #1
        pop   {r1-r12, pc}

gpu_bench:
        READ_CYCLE_COUNTER r6
        push   {r3, r6}
        bl     start_vc_bench
        pop    {r3, r6}
        READ_CYCLE_COUNTER r7
        subs   r0, r7, r6
        rsbmi  r0, r0, #1
        pop   {r1-r12, pc}

wait_for_source_fieldsync:
        push {r0-r12, lr}
        bl     _get_GPLEV0_r4
        // wait for vsync
        bl     wait_for_vsync
        pop  {r0-r12, pc}
        .align 6

key_press_reset:
        push   {r4-r12, lr}
        bl     _get_GPLEV0_r4
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

        .ltorg

#ifdef USE_MULTICORE
        .align 6
run_core:
        mov r0, #1
        str r0, core_1_available
        mov r0, #0
        mov r1, #0
        bl     enable_MMU_and_IDCaches
    //    bl    _enable_unaligned_access  //do not use for an armv6 to armv8 compatible binary
        bl    _init_cycle_counter
run_core_loop:
        wfe          // put core to sleep until an event
        ldr    r0, start_core_1_code
        cmp    r0, #0
        beq    run_core_loop
        mov    r0, #0
        str    r0, start_core_1_code
        bl     cga_process_artifact
        b    run_core_loop
core_1_available:
        .word 0
start_core_1_code:
        .word 0
#endif


        .ltorg
        .align 6
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

capture_line_normal_1bpp_table:
        .word capture_line_default_onebit_4bpp
        .word capture_line_default_onebit_8bpp
        .word capture_line_default_onebit_4bpp          // placeholder inband
        .word capture_line_default_onebit_8bpp           // placeholder inband
        .word capture_line_default_onebit_4bpp
        .word capture_line_default_onebit_8bpp
        .word capture_line_default_onebit_4bpp
        .word capture_line_default_onebit_8bpp
        .word capture_line_default_onebit_4bpp
        .word capture_line_default_onebit_8bpp
        .word capture_line_default_onebit_4bpp
        .word capture_line_default_onebit_8bpp
        .word capture_line_default_onebit_4bpp
        .word capture_line_default_onebit_8bpp
        .word capture_line_default_onebit_4bpp
        .word capture_line_default_onebit_8bpp
        .word capture_line_default_onebit_4bpp
        .word capture_line_default_onebit_8bpp

        .word capture_line_default_onebit_double_4bpp
        .word capture_line_default_onebit_double_8bpp
        .word capture_line_default_onebit_double_4bpp   // placeholder inband
        .word capture_line_default_onebit_double_8bpp    // placeholder inband
        .word capture_line_default_onebit_double_4bpp   // placeholder ntsc
        .word capture_line_default_onebit_double_8bpp    // placeholder ntsc
        .word capture_line_default_onebit_double_4bpp   // placeholder ntsc
        .word capture_line_default_onebit_double_8bpp    // placeholder ntsc
        .word capture_line_default_onebit_double_4bpp   // placeholder ntsc
        .word capture_line_default_onebit_double_8bpp    // placeholder ntsc
        .word capture_line_default_onebit_double_4bpp
        .word capture_line_default_onebit_double_8bpp
        .word capture_line_default_onebit_double_4bpp
        .word capture_line_default_onebit_double_8bpp
        .word capture_line_default_onebit_double_4bpp
        .word capture_line_default_onebit_double_8bpp
        .word capture_line_default_onebit_double_4bpp
        .word capture_line_default_onebit_double_8bpp

        .word capture_line_default_onebit_4bpp
        .word capture_line_default_onebit_8bpp
        //.word capture_line_fast_onebit_4bpp
        //.word capture_line_fast_onebit_8bpp

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
        .word capture_line_default_4bpp
        .word capture_line_default_8bpp
        .word capture_line_default_4bpp
        .word capture_line_default_8bpp
        .word capture_line_default_4bpp
        .word capture_line_default_8bpp

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
        .word capture_line_default_double_4bpp
        .word capture_line_default_double_8bpp
        .word capture_line_default_double_4bpp
        .word capture_line_default_double_8bpp
        .word capture_line_default_double_4bpp
        .word capture_line_default_double_8bpp

        .word capture_line_fast_4bpp
        .word capture_line_fast_8bpp

capture_line_normal_6bpp_table:
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp          // placeholder inband
        .word capture_line_default_sixbits_8bpp           // placeholder inband
        .word capture_line_ntsc_sixbits_16bpp_cga
        .word capture_line_ntsc_sixbits_8bpp_cga
        .word capture_line_ntsc_sixbits_16bpp_mono
        .word capture_line_ntsc_sixbits_8bpp_mono
        .word capture_line_ntsc_sixbits_16bpp_mono_auto
        .word capture_line_ntsc_sixbits_8bpp_mono_auto
        .word capture_line_default_sixbits_16bpp
        .word capture_line_c64yuv_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_atari_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_c64lc_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_atarilc_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_atarilc2600_sixbits_8bpp

        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp          // placeholder inband
        .word capture_line_default_sixbits_double_8bpp           // placeholder inband
        .word capture_line_default_sixbits_double_16bpp          // placeholder ntsc
        .word capture_line_ntsc_sixbits_double_8bpp_mono         // placeholder ntsc
        .word capture_line_default_sixbits_double_16bpp          // placeholder ntsc
        .word capture_line_ntsc_sixbits_double_8bpp_mono         // for tandy coco 1 & 2
        .word capture_line_default_sixbits_double_16bpp          // placeholder ntsc
        .word capture_line_ntsc_sixbits_double_8bpp_mono_auto    // for tandy coco 1 & 2 auto switches if UV = no colour
        .word capture_line_c64yuv_sixbits_double_16bpp
        .word capture_line_c64yuv_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_atari_sixbits_double_8bpp
        .word capture_line_c64lc_sixbits_double_16bpp
        .word capture_line_c64lc_sixbits_double_8bpp
        .word capture_line_atarilc_sixbits_double_16bpp
        .word capture_line_atarilc_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_atarilc2600_sixbits_double_8bpp

        .word capture_line_fast_sixbits_16bpp
        .word capture_line_fast_sixbits_8bpp


capture_line_normal_odd_even_6bpp_table:
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_odd_even_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp          // placeholder inband
        .word capture_line_default_odd_even_sixbits_8bpp           // placeholder inband
        .word capture_line_ntsc_sixbits_16bpp_cga
        .word capture_line_ntsc_sixbits_8bpp_cga
        .word capture_line_ntsc_sixbits_16bpp_mono
        .word capture_line_ntsc_sixbits_8bpp_mono
        .word capture_line_ntsc_sixbits_16bpp_mono_auto
        .word capture_line_ntsc_sixbits_8bpp_mono_auto
        .word capture_line_default_sixbits_16bpp
        .word capture_line_c64yuv_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_atari_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_c64lc_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_atarilc_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_atarilc2600_sixbits_8bpp

        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_odd_even_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp          // placeholder inband
        .word capture_line_default_odd_even_sixbits_double_8bpp           // placeholder inband
        .word capture_line_default_sixbits_double_16bpp          // placeholder ntsc
        .word capture_line_ntsc_sixbits_double_8bpp_mono         // placeholder ntsc
        .word capture_line_default_sixbits_double_16bpp          // placeholder ntsc
        .word capture_line_ntsc_sixbits_double_8bpp_mono         // for tandy coco 1 & 2
        .word capture_line_default_sixbits_double_16bpp          // placeholder ntsc
        .word capture_line_ntsc_sixbits_double_8bpp_mono_auto    // for tandy coco 1 & 2 auto switches if UV = no colour
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_c64yuv_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_atari_sixbits_double_8bpp
        .word capture_line_c64lc_sixbits_double_16bpp
        .word capture_line_c64lc_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_atarilc_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_atarilc2600_sixbits_double_8bpp

        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_odd_even_sixbits_double_8bpp

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
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_twelvebits_16bpp
        .word capture_line_fast_eightbits_8bpp


capture_line_simple_6bpp_table:
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_simple_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp          // placeholder inband
        .word capture_line_default_simple_sixbits_8bpp         // placeholder inband
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_simple_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_simple_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_simple_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_simple_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_simple_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_simple_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_simple_sixbits_8bpp
        .word capture_line_default_sixbits_16bpp
        .word capture_line_default_simple_sixbits_8bpp

        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp          // placeholder inband
        .word capture_line_default_sixbits_double_8bpp           // placeholder inband
        .word capture_line_default_sixbits_double_16bpp          // placeholder ntsc
        .word capture_line_default_sixbits_double_8bpp         // placeholder ntsc
        .word capture_line_default_sixbits_double_16bpp          // placeholder ntsc
        .word capture_line_default_sixbits_double_8bpp         // for tandy coco 1 & 2
        .word capture_line_default_sixbits_double_16bpp          // placeholder ntsc
        .word capture_line_default_sixbits_double_8bpp    // for tandy coco 1 & 2 auto switches if UV = no colour
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_sixbits_double_8bpp
        .word capture_line_default_sixbits_double_16bpp
        .word capture_line_default_sixbits_double_8bpp

        .word capture_line_fast_sixbits_16bpp
        .word capture_line_fast_simple_sixbits_8bpp


capture_line_simple_9bpplo_table:
        .word capture_line_default_simple_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp
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
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_simple_ninebitslo_16bpp
        .word capture_line_fast_eightbits_8bpp

capture_line_simple_9bpplo_blank_table:
        .word capture_line_default_simple_ninebitslo_16bpp_blank
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp_blank
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp_blank
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp_blank
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp_blank
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp_blank
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp_blank
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp_blank
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp_blank
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitslo_16bpp_blank
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
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitslo_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_simple_ninebitslo_16bpp_blank
        .word capture_line_fast_eightbits_8bpp

capture_line_simple_9bpphi_table:
        .word capture_line_default_simple_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitshi_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_ninebitshi_16bpp
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
        .word capture_line_default_ninebitshi_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitshi_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_ninebitshi_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_simple_ninebitshi_16bpp
        .word capture_line_fast_eightbits_8bpp


capture_line_simple_12bpp_table:
        .word capture_line_default_simple_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_16bpp
        .word capture_line_default_eightbits_8bpp
        .word capture_line_default_simple_16bpp
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
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp
        .word capture_line_default_twelvebits_double_16bpp
        .word capture_line_default_eightbits_double_8bpp

        .word capture_line_fast_simple_16bpp
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


.macro COUNT_PIXELS_3BPP reg
        // enters with r4,r5,r6 already loaded
        mov    r7, \reg, lsr #4   //pixel 1
        and    r7, r7, #7

        cmp    r5, r6
        cmpeq  r4, #0
        cmpeq  r7, #0
        bne    nocomp1\@

        cmp    r5, #0
        cmpne  r5, r4
        cmpne  r5, r7
        addne  r2, r2, #1
nocomp1\@:

        and    r4, \reg, #7       //pixel 2

        cmp    r6, r7
        cmpeq  r4, #0
        cmpeq  r5, #0
        bne    nocomp2\@
        cmp    r6, #0
        cmpne  r6, r5
        cmpne  r6, r4
        addne  r2, r2, #1
nocomp2\@:

        mov    r5, \reg, lsr #12  //pixel 3
        and    r5, r5, #7

        cmp    r7, r4
        cmpeq  r5, #0
        cmpeq  r6, #0
        bne    nocomp3\@
        cmp    r7, #0
        cmpne  r7, r6
        cmpne  r7, r5
        addne  r2, r2, #1
nocomp3\@:

        mov    r6, \reg, lsr #8   //pixel 4
        and    r6, r6, #7

        cmp    r4, r5
        cmpeq  r6, #0
        cmpeq  r7, #0
        bne    nocomp4\@
        cmp    r4, #0
        cmpne  r4, r7
        cmpne  r4, r6
        addne  r2, r2, #1
nocomp4\@:

        mov    r7, \reg, lsr #20   //pixel 5
        and    r7, r7, #7

        cmp    r5, r6
        cmpeq  r4, #0
        cmpeq  r7, #0
        bne    nocomp5\@
        cmp    r5, #0
        cmpne  r5, r4
        cmpne  r5, r7
        addne  r2, r2, #1
nocomp5\@:

        mov    r4, \reg, lsr #16   //pixel 6
        and    r4, r4, #7

        cmp    r6, r7
        cmpeq  r4, #0
        cmpeq  r5, #0
        bne    nocomp6\@
        cmp    r6, #0
        cmpne  r6, r5
        cmpne  r6, r4
        addne  r2, r2, #1
nocomp6\@:

        mov    r5, \reg, lsr #28  //pixel 7
        and    r5, r5, #7

        cmp    r7, r4
        cmpeq  r5, #0
        cmpeq  r6, #0
        bne    nocomp7\@
        cmp    r7, #0
        cmpne  r7, r6
        cmpne  r7, r5
        addne  r2, r2, #1
nocomp7\@:

        mov    r6, \reg, lsr #24  //pixel 8
        and    r6, r6, #7

        cmp    r4, r5
        cmpeq  r6, #0
        cmpeq  r7, #0
        bne    nocomp8\@
        cmp    r4, #0
        cmpne  r4, r7
        cmpne  r4, r6
        addne  r2, r2, #1
nocomp8\@:
.endm

.macro COUNT_PIXELS_12BPP
        and    r7, r8, r3     //pixel 1

        cmp    r5, r6
        cmpeq  r4, r12
        cmpeq  r7, r12
        tsteq  r4, #0x8000
        tsteq  r5, #0x8000
        tsteq  r6, #0x8000
        tsteq  r7, #0x8000
        bne    nocomp1b\@
        cmp    r5, r12
        cmpne  r5, r4
        cmpne  r5, r7
        addne  r2, r2, #1
nocomp1b\@:

        mov    r4, r8, lsr #16  //pixel 2

        cmp    r6, r7
        cmpeq  r4, r12
        cmpeq  r5, r12
        tsteq  r4, #0x8000
        tsteq  r5, #0x8000
        tsteq  r6, #0x8000
        tsteq  r7, #0x8000
        bne    nocomp2b\@
        cmp    r6, r12
        cmpne  r6, r5
        cmpne  r6, r4
        addne  r2, r2, #1
nocomp2b\@:

        and    r5, r9, r3     //pixel 3

        cmp    r7, r4
        cmpeq  r5, r12
        cmpeq  r6, r12
        tsteq  r4, #0x8000
        tsteq  r5, #0x8000
        tsteq  r6, #0x8000
        tsteq  r7, #0x8000
        bne    nocomp3b\@
        cmp    r7, r12
        cmpne  r7, r6
        cmpne  r7, r5
        addne  r2, r2, #1
nocomp3b\@:

        mov    r6, r9, lsr #16   //pixel 4

        cmp    r4, r5
        cmpeq  r6, r12
        cmpeq  r7, r12
        tsteq  r4, #0x8000
        tsteq  r5, #0x8000
        tsteq  r6, #0x8000
        tsteq  r7, #0x8000
        bne    nocomp4b\@
        cmp    r4, r12
        cmpne  r4, r7
        cmpne  r4, r6
        addne  r2, r2, #1
nocomp4b\@:

        and    r7, r10, r3       //pixel 5

        cmp    r5, r6
        cmpeq  r4, r12
        cmpeq  r7, r12
        tsteq  r4, #0x8000
        tsteq  r5, #0x8000
        tsteq  r6, #0x8000
        tsteq  r7, #0x8000
        bne    nocomp5b\@
        cmp    r5, r12
        cmpne  r5, r4
        cmpne  r5, r7
        addne  r2, r2, #1
nocomp5b\@:

        mov    r4, r10, lsr #16   //pixel 6

        cmp    r6, r7
        cmpeq  r4, r12
        cmpeq  r5, r12
        tsteq  r4, #0x8000
        tsteq  r5, #0x8000
        tsteq  r6, #0x8000
        tsteq  r7, #0x8000
        bne    nocomp6b\@
        cmp    r6, r12
        cmpne  r6, r5
        cmpne  r6, r4
        addne  r2, r2, #1
nocomp6b\@:

        and    r5, r11, r3       //pixel 7

        cmp    r7, r4
        cmpeq  r5, r12
        cmpeq  r6, r12
        tsteq  r4, #0x8000
        tsteq  r5, #0x8000
        tsteq  r6, #0x8000
        tsteq  r7, #0x8000
        bne    nocomp7b\@
        cmp    r7, r12
        cmpne  r7, r6
        cmpne  r7, r5
        addne  r2, r2, #1
nocomp7b\@:

        mov    r6, r11, lsr #16  //pixel 8

        cmp    r4, r5
        cmpeq  r6, r12
        cmpeq  r7, r12
        tsteq  r4, #0x8000
        tsteq  r5, #0x8000
        tsteq  r6, #0x8000
        tsteq  r7, #0x8000
        bne    nocomp8b\@
        cmp    r4, r12
        cmpne  r4, r7
        cmpne  r4, r6
        addne  r2, r2, #1
nocomp8b\@:

.endm

scan_for_single_pixels_12bpp:
        push   {r2-r12, lr}
        // r0 = pointer to start of memory
        add    r1, r0, r1
        // r1 = pointer to end of memory
        mov    r2, #0
        mov    r3, #0xff
        orr    r3, r3, #0xff00
        //ldr    r12, [r0]
        //and    r12, r12, r3         //first pixel used as background colour reference
        mov    r12, #0x7000           //black with alpha = 7 (i.e. dimmed)
        mov    r4, #0  //pixel 6
        mov    r5, #0  //pixel 7
        mov    r6, #0  //pixel 8
scan_loop12:
        ldmia  r0!, {r8-r11}
        COUNT_PIXELS_12BPP
        cmp    r0, r1
        blt    scan_loop12
        mov    r0, r2
        //on exit, r0= number of single pixels detected
        pop    {r2-r12, pc}

scan_for_single_pixels_4bpp:
        push   {r2-r12, lr}
        // r0 = pointer to start of memory
        add    r1, r0, r1
        // r1 = pointer to end of memory
        mov    r2, #0
        mov    r4, #0  //pixel 6
        mov    r5, #0  //pixel 7
        mov    r6, #0  //pixel 8
scan_loop:
        ldmia  r0!, {r8-r11}
        COUNT_PIXELS_3BPP r8
        COUNT_PIXELS_3BPP r9
        COUNT_PIXELS_3BPP r10
        COUNT_PIXELS_3BPP r11
        cmp    r0, r1
        blt    scan_loop
        mov    r0, r2
        //on exit, r0= number of single pixels detected
        pop    {r2-r12, pc}

.macro COMPARE_12BPP reg1 reg2
        tst    \reg1, #0x00008000
        tsteq  \reg2, #0x00008000
        bne    osd_pixels1\@
        eor    r14, \reg1, \reg2
        movs   r14, r14, lsl #16
        ldrne  r14, [r3, r4]
        addne  r14, r14, #1
        strne  r14, [r3, r4]
osd_pixels1\@:
        add    r4, r4, #4
        cmp    r4, #6*4
        movge  r4, #0

        tst    \reg1, #0x80000000
        tsteq  \reg2, #0x80000000
        bne    osd_pixels2\@
        eor    r14, \reg1, \reg2
        movs   r14, r14, lsr #16
        ldrne  r14, [r3, r4]
        addne  r14, r14, #1
        strne  r14, [r3, r4]
osd_pixels2\@:
        add    r4, r4, #4
        cmp    r4, #6*4
        movge  r4, #0
.endm


scan_for_diffs_12bpp:
        //r0 = pointer to new
        //r1 = pointer to old
        //r2 = length
        //r3 = address of diff array
        push   {r4-r12, lr}
        add    r2, r0, r2
        mov    r4, #0 //index into diff (mod 6)

diff_loop:
        ldmia  r0!, {r5-r8}
        ldmia  r1!, {r9-r12}
        COMPARE_12BPP r5, r9
        COMPARE_12BPP r6, r10
        COMPARE_12BPP r7, r11
        COMPARE_12BPP r8, r12
        cmp    r0, r2
        blt  diff_loop
        pop    {r4-r12, pc}

wait_for_pi_fieldsync:
        push   {r4-r12, lr}
        bl     clear_vsync
        // Poll for the VSYNC interrupt
        bl     _get_peripheral_base
        ldr    r14, =INTPEND2_OFFSET
        add    r0, r0, r14
wait_for_pi_loop:
        ldr    r1, [r0]
        tst    r1, #(1<<VSYNCINT)
        beq    wait_for_pi_loop
        // Clear the VSYNC interrupt
        bl     clear_vsync
        pop    {r4-r12, pc}

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
        bl     _get_GPLEV0_r4
        ldr    r8, [r4]
        tst    r8, #SW1_MASK
        tsteq  r8, #SW3_MASK
        beq    softreset
        mov    r0, #0
        ldr    r10, =sw1_power_up
        ldr    r9, [r10]
        KEY_PRESS_DETECT SW1_MASK, RET_SW1, sw1counter
        KEY_PRESS_DETECT SW2_MASK, RET_SW2, sw2counter
        KEY_PRESS_DETECT SW3_MASK, RET_SW3, sw3counter

        tst    r0, #RET_SW1
        moveq  r8, #0
        streq  r8, [r10]
        cmpne  r9, #0
        bicne  r0, #RET_SW1

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

        .ltorg

        .align 6
palette_data_16:
        .space (256*4), 0

        .align 6
line_buffer:
        .space 4096, 0
        .align 6
dummyscreen:               // used by capture preload
        .space 1920*1080, 0
