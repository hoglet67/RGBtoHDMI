#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_default_4bpp
.global inBandData

// The capture line function is provided the following:
//   r0 = pointer to current line in frame buffer
//   r1 = number of 8-pixel blocks to capture (=param_chars_per_line)
//   r2 = frame buffer line pitch in bytes (=param_fb_pitch)
//   r3 = flags register
//   r4 = GPLEV0 constant
//   r5 = frame buffer height (=param_fb_height)
//   r6 = scan line count modulo 10
//
// All registers are available as scratch registers (i.e. nothing needs to be preserved)

inBandData:
        .space 1024, 0
        
capture_line_default_4bpp:

        push    {lr}
        
        mov     r6, #0
        mov     r7, #0
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r7, =0x11111111
        adrl    r11, inBandData
        mov     r12, #0                  // pointer to end of "HDMI" string

        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        CONVERT_MODE0_MODE2
        WRITE_WORD
        sub     r1, r1, #1
        
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        CONVERT_MODE0_MODE2
        WRITE_WORD
        sub     r1, r1, #1
        
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        CONVERT_MODE0_MODE2
        WRITE_WORD
        sub     r1, r1, #1
        cmp     r9, #'H'        
        bne     loop
        
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        CONVERT_MODE0_MODE2
        WRITE_WORD
        sub     r1, r1, #1
        cmp     r9, #'D'        
        bne     loop
                
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        CONVERT_MODE0_MODE2
        WRITE_WORD
        sub     r1, r1, #1    
        cmp     r9, #'M'        
        bne     loop
              
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        CONVERT_MODE0_MODE2
        WRITE_WORD
        sub     r1, r1, #1
        cmp     r9, #'I'        
        bne     loop
        
        mov     r12, r0                  // save current screen pointer in r12 (points to end of "HDMI")
        orr     r3, r3, #BIT_IN_BAND_DETECTED 
inBandLoop:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        mov    r10, #0
        WRITE_WORD 
        strb   r9, [r11], #1        
        subs   r1, r1, #1        
        bne    inBandLoop
        
        sub    r0, r12, #16
        mov    r10, #0
        WRITE_WORD
        mov    r10, #0
        WRITE_WORD
        mov    r10, #0
        WRITE_WORD
        mov    r10, #0
        WRITE_WORD                 
        pop    {pc}
  
loop:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        CONVERT_MODE0_MODE2
        WRITE_WORD
        
        subs   r1, r1, #1
        bne    loop

        pop    {pc}
        


        
        

        






