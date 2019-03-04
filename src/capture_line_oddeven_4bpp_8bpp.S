#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.text

.global capture_line_oddeven_4bpp
.global capture_line_oddeven_8bpp

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

   
        b       preload_capture_line_oddeven
capture_line_oddeven_4bpp:
        push    {lr}   

        cmp     r5, #DUPLICATE_HEIGHT
        orrle   r3, r3, #BITDUP_NO_LINE_DOUBLE
        bicgt   r3, r3, #BITDUP_NO_LINE_DOUBLE         
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r7, =0x11111111
        moveq   r7, #0
        tst     r3, #BIT_DEBUG
        eorne   r7, r7, #0x50         //magenta in leftmost
        eorne   r7, r7, #0x02000000   //green in rightmost
        
        tst    r3, #BIT_EVEN_SAMPLES
        ldrne  r6, =0x70707070 
        tst    r3, #BIT_ODD_SAMPLES
        ldrne  r6, =0x07070707 

loop:                               
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS                 // input in r8, result in r10, corrupts r9
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS                // input in r8, result in r10, corrupts r9

        tst    r3, #BIT_EVEN_SAMPLES
        bicne  r10, r10, r6
        orrne  r10, r10, r10, lsl #4
                     
        tst    r3, #BIT_ODD_SAMPLES
        bicne  r10, r10, r6
        orrne  r10, r10, r10, lsr #4
        
        WRITE_WORD_FAST                              
        subs    r1, r1, #1
        bne     loop
        pop     {pc} 
      
preload_capture_line_oddeven:  
        SETUP_DUMMY_PARAMETERS
        b       capture_line_oddeven_4bpp

        .ltorg    

        // *** 8 bit ***
        
        
        b       preload_capture_line_oddeven_8bpp
capture_line_oddeven_8bpp:
        push    {lr}
         
        cmp     r5, #DUPLICATE_HEIGHT
        orrle   r3, r3, #BITDUP_NO_LINE_DOUBLE
        bicgt   r3, r3, #BITDUP_NO_LINE_DOUBLE 
        tst     r3, #BIT_VSYNC_MARKER
        ldrne   r7, =0x01010101
        moveq   r7, #0
        tst     r3, #BIT_DEBUG
        eorne   r7, r7, #0x05           //magenta in leftmost
        eorne   r7, r7, #0x02000000     //green in rightmost
        mov     r1, r1, lsl #1
        
        tst    r3, #BIT_EVEN_SAMPLES
        ldrne  r6, =0x00070007 
        tst    r3, #BIT_ODD_SAMPLES
        ldrne  r6, =0x07000700 
        
loop_8bpp:                          
        WAIT_FOR_PSYNC_EDGE              // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_8BPP                // input in r8, result in r10, corrupts r9
        
        tst    r3, #BIT_EVEN_SAMPLES
        bicne  r10, r10, r6
        orrne  r10, r10, r10, lsr #8
                     
        tst    r3, #BIT_ODD_SAMPLES
        bicne  r10, r10, r6
        orrne  r10, r10, r10, lsl #8 
             
        WRITE_WORD_FAST         
        subs    r1, r1, #1
        bne     loop_8bpp
        pop     {pc}
        
preload_capture_line_oddeven_8bpp:  
        SETUP_DUMMY_PARAMETERS
        b       capture_line_oddeven_8bpp   
        
        