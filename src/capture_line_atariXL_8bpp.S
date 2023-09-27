#include "rpi-base.h"
#include "defs.h"

#include "macros.S"

.global capture_line_atari_8bpp
.global capture_line_atari_double_8bpp
.global capture_line_atari_sixbits_8bpp
.global capture_line_atari_sixbits_double_8bpp

.global capture_line_c64lc_sixbits_8bpp
.global capture_line_c64lc_sixbits_double_8bpp
.global capture_line_c64lc_sixbits_double_16bpp
.global c64_artifact_palette_16

.global capture_line_atarilc_sixbits_8bpp
.global capture_line_atarilc_sixbits_double_8bpp

.global capture_line_atarilc2600_sixbits_8bpp
.global capture_line_atarilc2600_sixbits_double_8bpp

.macro CAPTURE_LOW_BITS_8BPP_ATARI reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)        //Y HI
        eorne  r10, r10, #0x0004                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)        //Y LO
        eorne  r10, r10, #0x0002                //bit 2 of luma
        tst    r8, #(0x20 << PIXEL_BASE)        //PB LO
        eorne  r10, r10, #0x0001                //bit 1 of luma
        tst    r8, #(0x08 << PIXEL_BASE)        //PR LO
        eorne  r10, r10, #0x0080                //bit 0 of luma

        tst    r8, #(0x02 << (PIXEL_BASE + 6))  //Y HI
        eorne  r10, r10, #0x0400                //bit 3 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))  //Y LO
        eorne  r10, r10, #0x0200                //bit 2 of luma
        tst    r8, #(0x20 << (PIXEL_BASE + 6))  //PB LO
        eorne  r10, r10, #0x0100                //bit 1 of luma
        tst    r8, #(0x08 << (PIXEL_BASE + 6))  //PR LO
        eorne  r10, r10, #0x8000                //bit 0 of luma

        tst    r8, #(0x04 << PIXEL_BASE)        //PB HI
        eorne  r10, r10, #0x0040                //bit 3 of chroma
        eorne  r10, r10, #0x4000                //bit 3 of chroma
        tst    r8, #(0x01 << PIXEL_BASE)        //PR HI
        eorne  r10, r10, #0x0020                //bit 2 of chroma
        eorne  r10, r10, #0x2000                //bit 2 of chroma
        tst    r8, #(0x04 << (PIXEL_BASE + 6))  //PB HI
        eorne  r10, r10, #0x0010                //bit 1 of chroma
        eorne  r10, r10, #0x1000                //bit 1 of chroma
        tst    r8, #(0x01 << (PIXEL_BASE + 6))  //PR HI
        eorne  r10, r10, #0x0008                //bit 0 of chroma
        eorne  r10, r10, #0x0800                //bit 0 of chroma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x0080
        bicne  r10, r10, #0x8000
.endm

.macro CAPTURE_HIGH_BITS_8BPP_ATARI reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24


        tst    r8, #(0x02 << PIXEL_BASE)        //Y HI
        eorne  r10, r10, #0x00040000            //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)        //Y LO
        eorne  r10, r10, #0x00020000            //bit 2 of luma
        tst    r8, #(0x20 << PIXEL_BASE)        //PB LO
        eorne  r10, r10, #0x00010000            //bit 1 of luma
        tst    r8, #(0x08 << PIXEL_BASE)        //PR LO
        eorne  r10, r10, #0x00800000            //bit 0 of luma

        tst    r8, #(0x02 << (PIXEL_BASE + 6))  //Y HI
        eorne  r10, r10, #0x04000000            //bit 3 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))  //Y LO
        eorne  r10, r10, #0x02000000            //bit 2 of luma
        tst    r8, #(0x20 << (PIXEL_BASE + 6))  //PB LO
        eorne  r10, r10, #0x01000000            //bit 1 of luma
        tst    r8, #(0x08 << (PIXEL_BASE + 6))  //PR LO
        eorne  r10, r10, #0x80000000            //bit 0 of luma

        tst    r8, #(0x04 << PIXEL_BASE)        //PB HI
        eorne  r10, r10, #0x00400000            //bit 3 of chroma
        eorne  r10, r10, #0x40000000            //bit 3 of chroma
        tst    r8, #(0x01 << PIXEL_BASE)        //PR HI
        eorne  r10, r10, #0x00200000            //bit 2 of chroma
        eorne  r10, r10, #0x20000000            //bit 2 of chroma
        tst    r8, #(0x04 << (PIXEL_BASE + 6))  //PB HI
        eorne  r10, r10, #0x00100000            //bit 1 of chroma
        eorne  r10, r10, #0x10000000            //bit 1 of chroma
        tst    r8, #(0x01 << (PIXEL_BASE + 6))  //PR HI
        eorne  r10, r10, #0x00080000            //bit 0 of chroma
        eorne  r10, r10, #0x08000000            //bit 0 of chroma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x00800000
        bicne  r10, r10, #0x80000000

        mov    \reg, r10
.endm

.macro CAPTURE_BITS_DOUBLE_8BPP_ATARI reg reg2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)        //Y HI
        eorne  r10, r10, #0x000004                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)        //Y LO
        eorne  r10, r10, #0x000002                //bit 2 of luma
        tst    r8, #(0x20 << PIXEL_BASE)        //PB LO
        eorne  r10, r10, #0x000001                //bit 1 of luma
        tst    r8, #(0x08 << PIXEL_BASE)        //PR LO
        eorne  r10, r10, #0x000080                //bit 0 of luma

        tst    r8, #(0x02 << (PIXEL_BASE + 6))  //Y HI
        eorne  r10, r10, #0x040000                //bit 3 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))  //Y LO
        eorne  r10, r10, #0x020000                //bit 2 of luma
        tst    r8, #(0x20 << (PIXEL_BASE + 6))  //PB LO
        eorne  r10, r10, #0x010000                //bit 1 of luma
        tst    r8, #(0x08 << (PIXEL_BASE + 6))  //PR LO
        eorne  r10, r10, #0x800000                //bit 0 of luma

        tst    r8, #(0x04 << PIXEL_BASE)        //PB HI
        eorne  r10, r10, #0x000040                //bit 3 of chroma
        eorne  r10, r10, #0x400000                //bit 3 of chroma
        tst    r8, #(0x01 << PIXEL_BASE)        //PR HI
        eorne  r10, r10, #0x000020                //bit 2 of chroma
        eorne  r10, r10, #0x200000                //bit 2 of chroma
        tst    r8, #(0x04 << (PIXEL_BASE + 6))  //PB HI
        eorne  r10, r10, #0x000010                //bit 1 of chroma
        eorne  r10, r10, #0x100000                //bit 1 of chroma
        tst    r8, #(0x01 << (PIXEL_BASE + 6))  //PR HI
        eorne  r10, r10, #0x000008                //bit 0 of chroma
        eorne  r10, r10, #0x080000                //bit 0 of chroma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x000080
        bicne  r10, r10, #0x800000

        orr    \reg2, r10, r10, lsl #8
.endm






.macro CAPTURE_0_BITS_8BPP_WIDE_ATARI reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x0004                  //bit 3 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y LO
        eorne  r10, r10, #0x0002                  //bit 2 of luma
        tst    r8, #(0x04 << (PIXEL_BASE + 6))    //PB LO
        eorne  r10, r10, #0x0001                  //bit 1 of luma
        tst    r8, #(0x01 << (PIXEL_BASE + 6))    //PR LO
        eorne  r10, r10, #0x0080                  //bit 0 of luma

        tst    r8, #(0x04 << PIXEL_BASE)          //PB HI
        eorne  r10, r10, #0x0040                  //bit 3 of chroma
        eorne  r10, r10, #0x4000                  //bit 3 of chroma
        tst    r8, #(0x01 << PIXEL_BASE)          //PR HI
        eorne  r10, r10, #0x0020                  //bit 2 of chroma
        eorne  r10, r10, #0x2000                  //bit 2 of chroma
.endm

.macro CAPTURE_0_BITS_8BPP_WIDE_ATARI_0
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        mov    r10, #0
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x0004                  //bit 3 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y LO
        eorne  r10, r10, #0x0002                  //bit 2 of luma
        tst    r8, #(0x04 << (PIXEL_BASE + 6))    //PB LO
        eorne  r10, r10, #0x0001                  //bit 1 of luma
        tst    r8, #(0x01 << (PIXEL_BASE + 6))    //PR LO
        eorne  r10, r10, #0x0080                  //bit 0 of luma

        tst    r8, #(0x04 << PIXEL_BASE)          //PB HI
        eorne  r10, r10, #0x0040                  //bit 3 of chroma
        eorne  r10, r10, #0x4000                  //bit 3 of chroma
        tst    r8, #(0x01 << PIXEL_BASE)          //PR HI
        eorne  r10, r10, #0x0020                  //bit 2 of chroma
        eorne  r10, r10, #0x2000                  //bit 2 of chroma
.endm

.macro CAPTURE_1_BITS_8BPP_WIDE_ATARI
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x0400                  //bit 3 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y LO
        eorne  r10, r10, #0x0200                  //bit 2 of luma
        tst    r8, #(0x04 << (PIXEL_BASE + 6))    //PB LO
        eorne  r10, r10, #0x0100                  //bit 1 of luma
        tst    r8, #(0x01 << (PIXEL_BASE + 6))    //PR LO
        eorne  r10, r10, #0x8000                  //bit 0 of luma

        tst    r8, #(0x04 << PIXEL_BASE)          //PB HI
        eorne  r10, r10, #0x0010                  //bit 1 of chroma
        eorne  r10, r10, #0x1000                  //bit 1 of chroma
        tst    r8, #(0x01 << PIXEL_BASE)          //PR HI
        eorne  r10, r10, #0x0008                  //bit 0 of chroma
        eorne  r10, r10, #0x0800                  //bit 0 of chroma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x0080
        bicne  r10, r10, #0x8000
.endm

.macro CAPTURE_2_BITS_8BPP_WIDE_ATARI
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x00040000                  //bit 3 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y LO
        eorne  r10, r10, #0x00020000                  //bit 2 of luma
        tst    r8, #(0x04 << (PIXEL_BASE + 6))    //PB LO
        eorne  r10, r10, #0x00010000                  //bit 1 of luma
        tst    r8, #(0x01 << (PIXEL_BASE + 6))    //PR LO
        eorne  r10, r10, #0x00800000                  //bit 0 of luma

        tst    r8, #(0x04 << PIXEL_BASE)          //PB HI
        eorne  r10, r10, #0x00400000                  //bit 3 of chroma
        eorne  r10, r10, #0x40000000                  //bit 3 of chroma
        tst    r8, #(0x01 << PIXEL_BASE)          //PR HI
        eorne  r10, r10, #0x00200000                  //bit 2 of chroma
        eorne  r10, r10, #0x20000000                  //bit 2 of chroma
.endm

.macro CAPTURE_3_BITS_8BPP_WIDE_ATARI reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x04000000                  //bit 3 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y LO
        eorne  r10, r10, #0x02000000                  //bit 2 of luma
        tst    r8, #(0x04 << (PIXEL_BASE + 6))    //PB LO
        eorne  r10, r10, #0x01000000                  //bit 1 of luma
        tst    r8, #(0x01 << (PIXEL_BASE + 6))    //PR LO
        eorne  r10, r10, #0x80000000                //bit 0 of luma

        tst    r8, #(0x04 << PIXEL_BASE)          //PB HI
        eorne  r10, r10, #0x00100000                //bit 1 of chroma
        eorne  r10, r10, #0x10000000                //bit 1 of chroma
        tst    r8, #(0x01 << PIXEL_BASE)          //PR HI
        eorne  r10, r10, #0x00080000                //bit 0 of chroma
        eorne  r10, r10, #0x08000000                //bit 0 of chroma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x00800000
        bicne  r10, r10, #0x80000000

        mov    \reg, r10
.endm


.macro CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_ATARI reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000004                //bit 3 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))        //Y LO
        eorne  r10, r10, #0x000002                //bit 2 of luma
        tst    r8, #(0x04 << (PIXEL_BASE + 6))        //PB LO
        eorne  r10, r10, #0x000001                //bit 1 of luma
        tst    r8, #(0x01 << (PIXEL_BASE + 6))        //PR LO
        eorne  r10, r10, #0x000080                //bit 0 of luma

        tst    r8, #(0x04 << PIXEL_BASE)          //PB HI
        eorne  r10, r10, #0x000040                //bit 3 of chroma
        eorne  r10, r10, #0x400000                //bit 3 of chroma
        tst    r8, #(0x01 << PIXEL_BASE)          //PR HI
        eorne  r10, r10, #0x000020                //bit 2 of chroma
        eorne  r10, r10, #0x200000                //bit 2 of chroma

.endm

.macro CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_ATARI reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x040000                //bit 3 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))        //Y LO
        eorne  r10, r10, #0x020000                //bit 2 of luma
        tst    r8, #(0x04 << (PIXEL_BASE + 6))        //PB LO
        eorne  r10, r10, #0x010000                //bit 1 of luma
        tst    r8, #(0x01 << (PIXEL_BASE + 6))        //PR LO
        eorne  r10, r10, #0x800000                //bit 0 of luma

        tst    r8, #(0x04 << PIXEL_BASE)          //PB HI
        eorne  r10, r10, #0x000010                //bit 1 of chroma
        eorne  r10, r10, #0x100000                //bit 1 of chroma
        tst    r8, #(0x01 << PIXEL_BASE)          //PR HI
        eorne  r10, r10, #0x000008                //bit 0 of chroma
        eorne  r10, r10, #0x080000                //bit 0 of chroma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x000080
        bicne  r10, r10, #0x800000

        orr    \reg, r10, r10, lsl #8
.endm





.macro CAPTURE_0_BITS_8BPP_WIDE_C64LC reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000008                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000004                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000002                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000001                //bit 0 of luma
.endm

.macro CAPTURE_0_BITS_8BPP_WIDE_C64LC_0
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8
        mov    r10, #0
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000008                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000004                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000002                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000001                //bit 0 of luma
.endm

.macro CAPTURE_1_BITS_8BPP_WIDE_C64LC
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000800                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000400                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000200                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000100                //bit 0 of luma
.endm

.macro CAPTURE_2_BITS_8BPP_WIDE_C64LC
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x080000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x040000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x020000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x010000                //bit 0 of luma
.endm

.macro CAPTURE_3_BITS_8BPP_WIDE_C64LC reg
        // Pixel 2 in GPIO  7.. 2 -> 23..16
        // Pixel 3 in GPIO 13.. 8 -> 31..24

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x08000000              //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x04000000              //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x02000000              //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x01000000              //bit 0 of luma

        mov    \reg, r10
.endm


.macro CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000008                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000004                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000002                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000001                //bit 0 of luma

.endm

.macro CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_C64LC reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x080000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x040000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x020000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x010000                //bit 0 of luma

        orr    \reg, r10, r10, lsl #8
.endm

        .ltorg

        // *** 8 bit atari ***
        .align 6
        b       preload_capture_line_atari_8bpp
capture_line_atari_8bpp:
        push    {lr}
        mov    r1, r1, lsl #1
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #2
loop_8bpp_Atari:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_ATARI r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_ATARI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_ATARI r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_ATARI r6                // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_ATARI r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_ATARI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_8BPP_ATARI r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_8BPP_ATARI r10               // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop_8bpp_Atari

        pop     {r0, pc}


preload_capture_line_atari_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_atari_8bpp

        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_atari_double_8bpp
capture_line_atari_double_8bpp:
        push    {lr}
        mov    r1, r1, lsl #1
        SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #1
loopd_8bpp_Atari:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_ATARI r11 r5          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_ATARI r12 r6          // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_ATARI r11 r7          // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_BITS_DOUBLE_8BPP_ATARI r12 r10         // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loopd_8bpp_Atari

        pop     {r0, pc}


preload_capture_line_atari_double_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_atari_double_8bpp

        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_atari_sixbits_8bpp
capture_line_atari_sixbits_8bpp:
        push    {lr}
        mov    r1, r1, lsl #1
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #3
loop6_8bpp_Atari:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_ATARI r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_ATARI                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_ATARI                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_ATARI r5               // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_ATARI_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_ATARI                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_ATARI                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_ATARI r6               // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_ATARI_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_ATARI                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_ATARI                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_ATARI r7               // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_ATARI r12            // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_ATARI                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_ATARI                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_ATARI r10            // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6_8bpp_Atari

        pop     {r0, pc}


preload_capture_line_atari_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_atari_sixbits_8bpp



        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_atari_sixbits_double_8bpp
capture_line_atari_sixbits_double_8bpp:
        push    {lr}
        mov    r1, r1, lsl #1
        SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #2
loop6d_8bpp_Atari:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_ATARI r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_ATARI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_ATARI r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_ATARI r6                // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_ATARI r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_ATARI r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_ATARI r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_ATARI r10               // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6d_8bpp_Atari

        pop     {r0, pc}


preload_capture_line_atari_sixbits_double_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_atari_sixbits_double_8bpp

        .ltorg


.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_0
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8


        mov    r9, r9, lsl #4
        bic    r9, r9, #0x00000f00
        bic    r9, r9, #0x000f0000
        bic    r9, r9, #0x0f000000

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r9, r9, #0x00000008                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r9, r9, #0x00000004                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r9, r9, #0x00000002                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r9, r9, #0x00000001                //bit 0 of luma

        and     r5, r9, #0xff
        ldr     r5, [r10, r5, lsl #2]
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        eor     r5, r5, r11
        moveq   r5, r5, lsl #16
        movne   r5, r5, lsr #16
        orreq   r5, r5, r5, lsr #16
        orrne   r5, r5, r5, lsl #16

.endm

.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_1
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r9, r9, #0x00000800                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r9, r9, #0x00000400                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r9, r9, #0x00000200                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r9, r9, #0x00000100                //bit 0 of luma

        and     r8, r9, #0xff00
        ldr     r6, [r10, r8, lsr #6]
        eor     r6, r6, r11
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        moveq   r6, r6, lsl #16
        movne   r6, r6, lsr #16
        orreq   r6, r6, r6, lsr #16
        orrne   r6, r6, r6, lsl #16

.endm

.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r9, r9, #0x00080000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r9, r9, #0x00040000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r9, r9, #0x00020000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r9, r9, #0x00010000                //bit 0 of luma

        and     r8, r9, #0xff0000
        ldr     r7, [r10, r8, lsr #14]
        eor     r7, r7, r11
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        moveq   r7, r7, lsl #16
        movne   r7, r7, lsr #16
        orreq   r7, r7, r7, lsr #16
        orrne   r7, r7, r7, lsl #16

.endm

.macro CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_3
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 15.. 8

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r9, r9, #0x08000000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r9, r9, #0x04000000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r9, r9, #0x02000000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r9, r9, #0x01000000                //bit 0 of luma
        str     r9, [r14], #4

        and     r8, r9, #0xff000000
        ldr     r10, [r10, r8, lsr #22]
        eor     r10, r10, r11
        tst     r3, #BITDUP_LINE_CONDITION_DETECTED
        moveq   r10, r10, lsl #16
        movne   r10, r10, lsr #16
        orreq   r10, r10, r10, lsr #16
        orrne   r10, r10, r10, lsl #16
.endm



        // *** 16 bit ***
        .align 6
        b       preload_capture_line_c64lc_sixbits_double_16bpp
capture_line_c64lc_sixbits_double_16bpp:
        push    {lr}
        tst    r6, #1  //CHECK ODD/EVEN LINE
        biceq  r3, r3, #BITDUP_LINE_CONDITION_DETECTED //use spare flag
        orrne  r3, r3, #BITDUP_LINE_CONDITION_DETECTED

        mov    r1, r1, lsl #1
        SETUP_VSYNC_DEBUG_16BPP_R11

        SKIP_PSYNC_NO_OLD_CPLD
        adrl   r14, bufferc64
        mov    r1, r1, lsr #2

loop_16bpp:
        ldr    r9, [r14]
        adrl r10, c64_artifact_palette_16

        WAIT_FOR_PSYNC_EDGE_FAST        // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST        // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_1                 // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_2                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST        // expects GPLEV0 in r4, result in r8
        CAPTURE_SIX_BITS_DOUBLE_16BPP_C64LC_3                 // input in r8


        WRITE_R5_R6_R7_R10_16BPP

        subs    r1, r1, #1
        bne     loop_16bpp
        bic     r3, r3, #BITDUP_LINE_CONDITION_DETECTED
        pop     {r0, pc}

preload_capture_line_c64lc_sixbits_double_16bpp:
        ldr    r0, =c64_artifact_palette_16
        mov    r1, #256
preload16_loop:
        ldr    r2, [r0], #4
        subs   r1, r1, #1
        bne    preload16_loop
        SETUP_DUMMY_PARAMETERS
        b       capture_line_default_sixbits_double_16bpp

        .ltorg

        .align 6
c64_artifact_palette_16:
        .space (256*4), 0
bufferc64:
        .space 2048, 0

        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_c64lc_sixbits_8bpp
capture_line_c64lc_sixbits_8bpp:
        push    {lr}
        mov    r1, r1, lsl #1
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #3
loop6_8bpp_c64lc:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_C64LC r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_C64LC r5               // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_C64LC_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_C64LC r6               // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_C64LC_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_C64LC                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_C64LC r7               // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_0_BITS_8BPP_WIDE_C64LC r12            // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_1_BITS_8BPP_WIDE_C64LC                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_2_BITS_8BPP_WIDE_C64LC                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_3_BITS_8BPP_WIDE_C64LC r10            // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6_8bpp_c64lc

        pop     {r0, pc}


preload_capture_line_c64lc_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_c64lc_sixbits_8bpp




        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_c64lc_sixbits_double_8bpp
capture_line_c64lc_sixbits_double_8bpp:
        push    {lr}
        mov    r1, r1, lsl #1
        SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        SKIP_PSYNC_NO_OLD_CPLD
        mov    r1, r1, lsr #2
loop6d_8bpp_c64lc:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_C64LC r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_C64LC r6                // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_C64LC r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LOW_BITS_DOUBLE_8BPP_WIDE_C64LC r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HIGH_BITS_DOUBLE_8BPP_WIDE_C64LC r10               // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6d_8bpp_c64lc

        pop     {r0, pc}


preload_capture_line_c64lc_sixbits_double_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_c64lc_sixbits_double_8bpp




.macro CAPTURE_HUE_BITS_8BPP_WIDE_ATARI_0 reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000040                //bit 3 of hue
        eorne  r10, r10, #0x004000                //bit 3 of hue
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000020                //bit 2 of hue
        eorne  r10, r10, #0x002000                //bit 2 of hue
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000010                //bit 1 of hue
        eorne  r10, r10, #0x001000                //bit 1 of hue
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000008                //bit 0 of hue
        eorne  r10, r10, #0x000800                //bit 0 of hue
.endm



.macro CAPTURE_LUMA0_BITS_8BPP_WIDE_ATARI_0
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000004                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000002                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000001                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000080                //bit 0 of luma
.endm

.macro CAPTURE_LUMA1_BITS_8BPP_WIDE_ATARI_0
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x00000400                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x00000200                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x00000100                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x00008000                //bit 0 of luma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x000080
        bicne  r10, r10, #0x008000
.endm

.macro CAPTURE_HUE_BITS_8BPP_WIDE_ATARI_1
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x00400000                //bit 3 of hue
        eorne  r10, r10, #0x40000000                //bit 3 of hue
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x00200000                //bit 2 of hue
        eorne  r10, r10, #0x20000000                //bit 2 of hue
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x00100000                //bit 1 of hue
        eorne  r10, r10, #0x10000000                //bit 1 of hue
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x00080000                //bit 0 of hue
        eorne  r10, r10, #0x08000000                //bit 0 of hue
.endm

.macro CAPTURE_LUMA0_BITS_8BPP_WIDE_ATARI_1
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x040000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x020000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x010000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x800000                //bit 0 of luma
.endm

.macro CAPTURE_LUMA1_BITS_8BPP_WIDE_ATARI_1 reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x04000000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x02000000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x01000000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x80000000                //bit 0 of luma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x00800000
        bicne  r10, r10, #0x80000000

        mov    \reg, r10

.endm



.macro CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000040                //bit 3 of hue
        eorne  r10, r10, #0x400000                //bit 3 of hue
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000020                //bit 2 of hue
        eorne  r10, r10, #0x200000                //bit 2 of hue
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000010                //bit 1 of hue
        eorne  r10, r10, #0x100000                //bit 1 of hue
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000008                //bit 0 of hue
        eorne  r10, r10, #0x080000                //bit 0 of hue
.endm

.macro CAPTURE_LUMA0_BITS_DOUBLE_8BPP_WIDE_ATARI
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000004                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000002                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000001                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000080                //bit 0 of luma
.endm

.macro CAPTURE_LUMA1_BITS_DOUBLE_8BPP_WIDE_ATARI reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x040000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x020000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x010000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x800000                //bit 0 of luma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x000080
        bicne  r10, r10, #0x800000

        orr    \reg, r10, r10, lsl #8

.endm




        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_atarilc_sixbits_8bpp
capture_line_atarilc_sixbits_8bpp:
        push    {lr}
        push {r1}
        add  r1, r1, r1, lsl #1  // *3
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD
        pop {r14}
        pop  {r1}
        push {r14}
        mov    r1, r1, lsr #2
loop6_8bpp_atarilc:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI_0 r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_8BPP_WIDE_ATARI_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_8BPP_WIDE_ATARI_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_8BPP_WIDE_ATARI_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_8BPP_WIDE_ATARI_1 r5                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI_0 r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_8BPP_WIDE_ATARI_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_8BPP_WIDE_ATARI_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_8BPP_WIDE_ATARI_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_8BPP_WIDE_ATARI_1 r6                // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI_0 r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_8BPP_WIDE_ATARI_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_8BPP_WIDE_ATARI_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_8BPP_WIDE_ATARI_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_8BPP_WIDE_ATARI_1 r7                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI_0 r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_8BPP_WIDE_ATARI_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_8BPP_WIDE_ATARI_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_8BPP_WIDE_ATARI_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_8BPP_WIDE_ATARI_1 r10                // input in r8

        WRITE_R5_R6_R7_R10


        subs    r1, r1, #2
        bne     loop6_8bpp_atarilc

        pop     {r0, pc}


preload_capture_line_atarilc_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_atarilc_sixbits_8bpp




        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_atarilc_sixbits_double_8bpp
capture_line_atarilc_sixbits_double_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        push {r1}
        add  r1, r1, r1, lsl #1  // *3
        SKIP_PSYNC_NO_OLD_CPLD
        pop {r14}
        pop  {r1}
        push {r14}
        mov    r1, r1, lsr #1
loop6d_8bpp_atarilc:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_DOUBLE_8BPP_WIDE_ATARI                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_DOUBLE_8BPP_WIDE_ATARI r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_DOUBLE_8BPP_WIDE_ATARI                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_DOUBLE_8BPP_WIDE_ATARI r6                // input in r8
        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_DOUBLE_8BPP_WIDE_ATARI                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_DOUBLE_8BPP_WIDE_ATARI r7               // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA0_BITS_DOUBLE_8BPP_WIDE_ATARI                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA1_BITS_DOUBLE_8BPP_WIDE_ATARI r10               // input in r8
        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6d_8bpp_atarilc

        pop     {r0, pc}


preload_capture_line_atarilc_sixbits_double_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_atarilc_sixbits_double_8bpp


        .ltorg


.macro CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_0 reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000040                //bit 3 of hue
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000020                //bit 2 of hue
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000010                //bit 1 of hue
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000008                //bit 0 of hue
.endm

.macro CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_1
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x004000                //bit 3 of hue
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x002000                //bit 2 of hue
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x001000                //bit 1 of hue
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000800                //bit 0 of hue
.endm

.macro CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x00400000                //bit 3 of hue
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x00200000                //bit 2 of hue
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x00100000                //bit 1 of hue
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x00080000                //bit 0 of hue
.endm

.macro CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_3
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x40000000                //bit 3 of hue
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x20000000                //bit 2 of hue
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x10000000                //bit 1 of hue
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x08000000                //bit 0 of hue
.endm

.macro CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_0
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x00000004                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x00000002                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x00000001                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x00000080                //bit 0 of luma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x000080
        tst    r3, #BIT_NO_SCANLINES
        biceq  r10, r10, #0x000080
.endm

.macro CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_1
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x00000400                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x00000200                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x00000100                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x00008000                //bit 0 of luma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x00008000
        tst    r3, #BIT_NO_SCANLINES
        biceq  r10, r10, #0x00008000
.endm

.macro CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_2
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x040000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x020000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x010000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x800000                //bit 0 of luma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x00800000
        tst    r3, #BIT_NO_SCANLINES
        biceq  r10, r10, #0x00800000
.endm

.macro CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_3 reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x04000000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x02000000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x01000000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x80000000                //bit 0 of luma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x80000000
        tst    r3, #BIT_NO_SCANLINES
        biceq  r10, r10, #0x80000000

        mov    \reg, r10

.endm

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_atarilc2600_sixbits_8bpp
capture_line_atarilc2600_sixbits_8bpp:
        push    {lr}
        push {r1}
        mov    r1, r1, lsl #2
        SETUP_VSYNC_DEBUG_R11_R12
        SKIP_PSYNC_NO_OLD_CPLD
        pop {r14}
        pop  {r1}
        push {r14}
        mov    r1, r1, lsr #2
loop6_8bpp_atarilc2600:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_0 r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_2                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_2                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_3                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_3 r5                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_0 r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_2                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_2                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_3                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_3 r6                // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_0 r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_2                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_2                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_3                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_3 r7                // input in r8

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_0 r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_1                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_2                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_2                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST
        CAPTURE_HUE_BITS_8BPP_WIDE_ATARI2600_3                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_8BPP_WIDE_ATARI2600_3 r10                // input in r8

        WRITE_R5_R6_R7_R10


        subs    r1, r1, #2
        bne     loop6_8bpp_atarilc2600

        pop     {r0, pc}


preload_capture_line_atarilc2600_sixbits_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_atarilc2600_sixbits_8bpp



.macro CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI2600_0 reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        mov    r10, \reg
        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000040                //bit 3 of hue
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000020                //bit 2 of hue
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000010                //bit 1 of hue
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000008                //bit 0 of hue
.endm

.macro CAPTURE_LUMA_BITS_DOUBLE_8BPP_WIDE_ATARI2600_0
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x000004                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x000002                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x000001                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x000080                //bit 0 of luma
.endm

.macro CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI2600_1
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x400000                  //bit 3 of hue
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x200000                //bit 2 of hue
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x100000                //bit 1 of hue
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x080000                //bit 0 of hue
.endm

.macro CAPTURE_LUMA_BITS_DOUBLE_8BPP_WIDE_ATARI2600_1 reg
        // Pixel 0 in GPIO  7.. 2 ->  7.. 0
        // Pixel 1 in GPIO 13.. 8 -> 23..16

        tst    r8, #(0x02 << PIXEL_BASE)          //Y HI
        eorne  r10, r10, #0x040000                //bit 3 of luma
        tst    r8, #(0x10 << PIXEL_BASE)          //Y LO
        eorne  r10, r10, #0x020000                //bit 2 of luma
        tst    r8, #(0x02 << (PIXEL_BASE + 6))    //Y HI second pixel
        eorne  r10, r10, #0x010000                //bit 1 of luma
        tst    r8, #(0x10 << (PIXEL_BASE + 6))    //Y LO second pixel
        eorne  r10, r10, #0x800000                //bit 0 of luma

        tst    r3, #BIT_OSD
        bicne  r10, r10, #0x000080
        bicne  r10, r10, #0x800000
        tst    r3, #BIT_NO_SCANLINES
        biceq  r10, r10, #0x000080
        biceq  r10, r10, #0x800000

        orr    \reg, r10, r10, lsl #8

.endm

        .ltorg

        // *** 8 bit ***
        .align 6
        b       preload_capture_line_atarilc2600_sixbits_double_8bpp
capture_line_atarilc2600_sixbits_double_8bpp:
        push    {lr}
        SETUP_VSYNC_DEBUG_R11_R12_DOUBLE
        push {r1}
        mov    r1, r1, lsl #2
        SKIP_PSYNC_NO_OLD_CPLD
        pop {r14}
        pop  {r1}
        push {r14}
        mov    r1, r1, lsr #1
loop6d_8bpp_atarilc2600:
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI2600_0 r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_DOUBLE_8BPP_WIDE_ATARI2600_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI2600_1                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_DOUBLE_8BPP_WIDE_ATARI2600_1 r5                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI2600_0 r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_DOUBLE_8BPP_WIDE_ATARI2600_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI2600_1                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_DOUBLE_8BPP_WIDE_ATARI2600_1 r6                // input in r8

        WRITE_R5_R6_IF_LAST
        cmp     r1, #1
        popeq   {r0, pc}

        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI2600_0 r11                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_DOUBLE_8BPP_WIDE_ATARI2600_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI2600_1                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_DOUBLE_8BPP_WIDE_ATARI2600_1 r7                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI2600_0 r12                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_DOUBLE_8BPP_WIDE_ATARI2600_0                // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_HUE_BITS_DOUBLE_8BPP_WIDE_ATARI2600_1                  // input in r8
        WAIT_FOR_PSYNC_EDGE_FAST                      // expects GPLEV0 in r4, result in r8
        CAPTURE_LUMA_BITS_DOUBLE_8BPP_WIDE_ATARI2600_1 r10                // input in r8

        WRITE_R5_R6_R7_R10

        subs    r1, r1, #2
        bne     loop6d_8bpp_atarilc2600

        pop     {r0, pc}


preload_capture_line_atarilc2600_sixbits_double_8bpp:
        SETUP_DUMMY_PARAMETERS
        b       capture_line_atarilc2600_sixbits_double_8bpp
