#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_inband_4bpp
.global capture_line_inband_8bpp
.global inBandData
.global sentinel
.global paletteFlags
.global inBandPointer
.global paletteHighNibble

// The capture line function is provided the following:
//   r0 = pointer to current line in frame buffer
//   r1 = number of complete psync cycles to capture (=param_chars_per_line)
//   r2 = frame buffer line pitch in bytes (=param_fb_pitch)
//   r3 = flags register
//   r4 = GPLEV0 constant
//   r5 = line number count down to 0
//   r6 = scan line count modulo 10
//   r7 = number of psyncs to skip
//   r8 = frame buffer height (=param_fb_height)
//
// All registers are available as scratch registers (i.e. nothing needs to be preserved)


inBandData:
        .space 32768, 0

paletteHighNibble:
        .space 1024, 0

        b       preload_capture_line_inband_4bpp
capture_line_inband_4bpp:
        push    {lr}
        ldr     r11, inBandPointer
        ldr     r8, paletteFlags
        mov     r6, #0
        tst     r8, #BIT_SET_MODE2_16COLOUR
        orrne   r3, r3, #BITDUP_MODE2_16COLOUR
        biceq   r3, r3, #BITDUP_MODE2_16COLOUR
        ldr     r12, sentinel            // 32 bit sentinel
        SKIP_PSYNC
        push    {r14}
        mov     r7, #0
loop:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_TRANSLATE       // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_TRANSLATE      // input in r8, result in r10, corrupts r9
        WRITE_WORD
        cmp     r6, r12
        beq     foundmode0inband
        cmp     r7, r12
        beq     foundmode0to6inband
        subs    r1, r1, #1
        bne     loop
        pop     {r0, pc}
 
preload_foundmode0inband:
        push    {lr}
        mov     r0, #0
        push    {r0}
foundmode0inband:                        // found 640 bits in band format works only in mode 0
        subs    r1, r1, #1               // too short for valid data
        popeq   {r0, pc}
        mov     r12, r0                  // save current screen pointer in r12 (points to end of sentinel)
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_TRANSLATE       // use 4 bit macro as it's faster (4 bit screen value discarded)
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_TRANSLATE      // use 4 bit macro as it's faster (4 bit screen value discarded)
        mov     r10, #0
        WRITE_WORD
        subs    r1, r1, #1
        popeq   {r0, pc}                 // too short for valid data
        and     r5, r6, #0xff             // first byte read in r6 is count of command bytes
        strb    r5, [r11], #1
inBandLoop0:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_TRANSLATE       // use 4 bit macro as it's faster (4 bit screen value discarded)
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_TRANSLATE      // use 4 bit macro as it's faster (4 bit screen value discarded)
        mov     r10, #0
        WRITE_WORD
        cmp     r5, #0                   // only write command bytes until last one read
        strneb  r6, [r11], #1
        subne   r5, r5, #1
        subs    r1, r1, #1
        bne     inBandLoop0
        str     r11, inBandPointer
        sub     r0, r12, #16
        mov     r11, #4
blank0loop:
        mov    r10, #0
        WRITE_WORD
        subs   r11, r11, #1
        bne    blank0loop
        ldr     r8, paletteFlags
        orr     r8, r8, #BIT_IN_BAND_DETECTED
        str     r8, paletteFlags
        pop    {r0, pc}

preload_foundmode0to6inband:
        push    {lr}
        mov     r0, #0
        push    {r0}
foundmode0to6inband:                // found 160 bits in band format works in all modes
        subs    r1, r1, #1
        popeq   {r0, pc}
        mov     r12, r0                  // save current screen pointer in r12 (points to end of sentinel)
        mov     r5, #4                    // need to read 4 word in 160 bit mode to get 1 byte of data
inBandLoop0to6_size:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_TRANSLATE       // use 4 bit macro to avoid corrupting r5 (4 bit screen value discarded)
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_TRANSLATE      // use 4 bit macro to avoid corrupting r5 (4 bit screen value discarded)
        mov    r10, #0
        WRITE_WORD
        subs   r1, r1, #1
        popeq   {r0, pc}                     // too short for valid data
        subs    r5, r5, #1
        bne     inBandLoop0to6_size
        and     r5, r7, #0xff            // first byte read in r7 is count of command bytes
        strb    r5, [r11], #1
        mov     r5, r5, lsl #2           // must read 4 words to get 1 byte in 160 bit mode
inBandLoop0to6:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_TRANSLATE       // use 4 bit macro to avoid corrupting r5 (4 bit screen value discarded)
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_TRANSLATE      // use 4 bit macro to avoid corrupting r5 (4 bit screen value discarded)
        mov    r10, #0
        WRITE_WORD
        and    r8, r5, #3
        cmp    r8, #1
        streqb r7, [r11], #1
        cmp    r5, #0
        subne  r5, r5, #1
        subs   r1, r1, #1
        bne    inBandLoop0to6
        str    r11, inBandPointer
        sub    r0, r12, #64
        mov    r11, #16
blank0to6loop:
        mov    r10, #0
        WRITE_WORD
        subs   r11, r11, #1
        bne    blank0to6loop
        ldr     r8, paletteFlags
        orr     r8, r8, #BIT_IN_BAND_DETECTED
        str     r8, paletteFlags
        pop    {r0, pc}

preload_capture_line_inband_4bpp:
        push    {lr}
        ldr     r0, paletteFlags
        ldr     r1, inBandPointer
        push    {r0, r1}
        mov     r0, #0
        str     r0, paletteFlags             //disable flags
        SETUP_DUMMY_PARAMETERS
        bl      capture_line_inband_4bpp
        mov     r1, #3
        bl      preload_foundmode0inband
        mov     r1, #6
        bl      preload_foundmode0to6inband
        pop     {r0, r1}
        str     r0, paletteFlags
        str     r1, inBandPointer
        pop     {pc}


        .ltorg

sentinel:
        .word 0

paletteFlags:
        .word 0

inBandPointer:
        .word 0


        // *** 8 bit ***


        b       preload_capture_line_inband_8bpp
capture_line_inband_8bpp:
        push    {lr}
        adrl    r9, paletteHighNibble
        subs    r5, r5, #VERTICAL_OFFSET      //r5 = line number count down to 0
        movmi   r5, #0
        cmp     r5, #0x100
        movge   r5, #0xff
        rsb     r5, r5, #0xff
        ldrb    r5, [r9, r5]
        ldr     r8, paletteFlags
        tst     r8, #BIT_MULTI_PALETTE
        bic     r3, #MASKDUP_PALETTE_HIGH_NIBBLE
        orrne   r3, r3, r5, lsl #OFFSETDUP_PALETTE_HIGH_NIBBLE
        tst     r8, #BIT_SET_MODE2_16COLOUR
        orrne   r3, r3, #BITDUP_MODE2_16COLOUR
        biceq   r3, r3, #BITDUP_MODE2_16COLOUR
        mov     r6, #0
        ldr     r11, inBandPointer
        ldr     r12, sentinel            // 32 bit sentinel
        SKIP_PSYNC
        push    {r14}
        mov     r7, #0
loop_8bpp:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_TRANSLATE_8BPP  // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_TRANSLATE_8BPP // input in r8, result in r9/r10
        WRITE_WORDS_8BPP
        cmp     r6, r12
        beq     foundmode0inband_8bpp
        cmp     r7, r12
        beq     foundmode0to6inband_8bpp
        subs    r1, r1, #1
        bne     loop_8bpp
        pop     {r0, pc}

preload_foundmode0inband_8bpp:
        push    {lr}
        mov     r0, #0
        push    {r0}
foundmode0inband_8bpp:                   // found 640 bits in band format works only in mode 0
        subs    r1, r1, #1               // too short for valid data
        popeq   {r0, pc}
        mov     r12, r0                  // save current screen pointer in r12 (points to end of sentinel)
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_TRANSLATE       // use 4 bit macro as it's faster (4 bit screen value discarded)
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_TRANSLATE      // use 4 bit macro as it's faster (4 bit screen value discarded)
        mov     r9, #0
        mov     r10, #0
        WRITE_WORDS_8BPP
        subs    r1, r1, #1
        popeq   {r0, pc}                     // too short for valid data
        and     r5, r6, #0xff             // first byte read in r6 is count of command bytes
        strb    r5, [r11], #1
inBandLoop0_8bpp:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_TRANSLATE       // use 4 bit macro as it's faster (4 bit screen value discarded)
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_TRANSLATE      // use 4 bit macro as it's faster (4 bit screen value discarded)
        mov     r9, #0
        mov     r10, #0
        WRITE_WORDS_8BPP
        cmp     r5, #0                   // only write command bytes until last one read
        strneb  r6, [r11], #1
        subne   r5, r5, #1
        subs    r1, r1, #1
        bne     inBandLoop0_8bpp
        str     r11, inBandPointer
        sub     r0, r12, #32
        mov     r11, #8
blank0loop_8bpp:
        mov    r9, #0
        mov    r10, #0
        WRITE_WORDS_8BPP
        subs   r11, r11, #1
        bne    blank0loop_8bpp
        ldr    r8, paletteFlags
        orr    r8, r8, #BIT_IN_BAND_DETECTED
        str    r8, paletteFlags
        pop    {r0, pc}

preload_foundmode0to6inband_8bpp:
        push    {lr}
        mov     r0, #0
        push    {r0}
foundmode0to6inband_8bpp:                // found 160 bits in band format works in all modes
        subs    r1, r1, #1
        popeq   {r0, pc}
        mov     r12, r0                  // save current screen pointer in r12 (points to end of sentinel)

        mov    r5, #4                    // need to read 4 word in 160 bit mode to get 1 byte of data
inBandLoop0to6_size_8bpp:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_TRANSLATE       // use 4 bit macro to avoid corrupting r5 (4 bit screen value discarded)
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_TRANSLATE      // use 4 bit macro to avoid corrupting r5 (4 bit screen value discarded)
        mov    r9, #0
        mov    r10, #0
        WRITE_WORDS_8BPP
        subs   r1, r1, #1
        popeq   {r0, pc}                     // too short for valid data
        subs    r5, r5, #1
        bne     inBandLoop0to6_size_8bpp
        and     r5, r7, #0xff            // first byte read in r7 is count of command bytes
        strb    r5, [r11], #1
        mov     r5, r5, lsl #2           // must read 4 words to get 1 byte in 160 bit mode
inBandLoop0to6_8bpp:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_TRANSLATE       // use 4 bit macro to avoid corrupting r5 (4 bit screen value discarded)
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_TRANSLATE      // use 4 bit macro to avoid corrupting r5 (4 bit screen value discarded)
        mov    r9, #0
        mov    r10, #0
        WRITE_WORDS_8BPP
        and    r8, r5, #3
        cmp    r8, #1
        streqb r7, [r11], #1
        cmp    r5, #0
        subne  r5, r5, #1
        subs   r1, r1, #1
        bne    inBandLoop0to6_8bpp
        str    r11, inBandPointer
        sub    r0, r12, #128
        mov    r11, #32
blank0to6loop_8bpp:
        mov    r9, #0
        mov    r10, #0
        WRITE_WORDS_8BPP
        subs   r11, r11, #1
        bne    blank0to6loop_8bpp
        ldr     r8, paletteFlags
        orr     r8, r8, #BIT_IN_BAND_DETECTED
        str     r8, paletteFlags
        pop    {r0, pc}


preload_capture_line_inband_8bpp:
        push    {lr}
        ldr     r0, paletteFlags
        ldr     r1, inBandPointer
        push    {r0, r1}
        mov     r0, #0
        str     r0, paletteFlags             //disable flags
        SETUP_DUMMY_PARAMETERS
        bl      capture_line_inband_8bpp
        mov     r1, #3
        bl      preload_foundmode0inband_8bpp
        mov     r1, #6
        bl      preload_foundmode0to6inband_8bpp
        pop     {r0, r1}
        str     r0, paletteFlags
        str     r1, inBandPointer
        pop     {pc}
