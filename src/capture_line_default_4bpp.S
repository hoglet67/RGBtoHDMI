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
       
        adrl    r11, inBandData
        ldr     r12, =0x48444d49           // "HDMI" string
        
        mov     r5, #2      
capblank:        
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        CONVERT_MODE0_MODE2
        WRITE_WORD
        sub     r1, r1, #1
        subs    r5, r5, #1
        bne     capblank
        
        mov     r6, #0                   // in band data 8 bits per byte (mode 0 only)
        mov     r7, #0                   // in band data 2 bits per byte (modes 0-6)
        mov     r5, #4   
capheadermode0:                          // test for 640 bits in band format works only in mode 0
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        CONVERT_MODE0_MODE2
        WRITE_WORD
        sub     r1, r1, #1
        
        mov     r6, r6, lsl #8
        orr     r6, r6, r9
        mov     r7, r7, lsl #2
        tst     r9, #0x10
        orrne   r7, r7, #2
        tst     r9, #0x01
        orrne   r7, r7, #1
       
        subs    r5, r5, #1
        bne     capheadermode0
        
        cmp     r6, r12
        beq     foundmode0inband
        
        mov     r6, r7
        mov     r5, #13
capheadermodes0to6:                      // test for 160 bits in band format works in all modes
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        CONVERT_MODE0_MODE2
        WRITE_WORD     
        sub     r1, r1, #1
        
        mov     r6, r6, lsl #2
        tst     r9, #0x10
        orrne   r6, r6, #2
        tst     r9, #0x01
        orrne   r6, r6, #1   
        cmp     r6, r12
        beq     foundmode0to6inband
        
        subs    r5, r5, #1
        bne     capheadermodes0to6
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
        
         
foundmode0to6inband:                     // found 160 bits in band format works in all modes
        mov     r12, r0                  // save current screen pointer in r12 (points to end of "HDMI")
        orr     r3, r3, #BIT_IN_BAND_DETECTED 
inBandLoop0to6:
        mov     r6, #0
        mov     r5, #4
inBandLoop0to6sub:       
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        mov    r10, #0
        WRITE_WORD       
        mov    r6, r6, lsl #2
        tst    r9, #0x10
        orrne  r6, r6, #0x02
        tst    r9, #0x01
        orrne  r6, r6, #0x01
        subs   r1, r1, #1
        beq    exitinBandLoop0to6    
        subs   r5, r5, #1         
        bne    inBandLoop0to6sub        
        strb   r6, [r11], #1           
        b      inBandLoop0to6
        
exitinBandLoop0to6:
        strb   r6, [r11], #1 
        sub    r0, r12, #64   
        mov    r5, #16
blank0to6loop:        
        mov    r10, #0
        WRITE_WORD
        subs   r5, r5, #1
        bne    blank0to6loop              
        pop    {pc}
            
foundmode0inband:                        // found 640 bits in band format works only in mode 0
        mov     r12, r0                  // save current screen pointer in r12 (points to end of "HDMI")
        orr     r3, r3, #BIT_IN_BAND_DETECTED 
inBandLoop0:
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9/r14
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9/r14
        CONVERT_WORD_MODE0
        mov    r10, #0
        WRITE_WORD 
        strb   r9, [r11], #1        
        subs   r1, r1, #1        
        bne    inBandLoop0
        
        sub    r0, r12, #16
        mov    r5, #4
blank0loop:          
        mov    r10, #0
        WRITE_WORD
        subs   r5, r5, #1
        bne    blank0loop
             
        pop    {pc}
  