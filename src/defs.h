// defs.h

#ifndef DEFS_H
#define DEFS_H

// Define how the Pi Framebuffer is initialized
// - if defined, use the property interface (Channel 8)
// - if not defined, use to the the framebuffer interface (Channel 1)
//
// Note: there seem to be some weird caching issues with the property interface
// so using the dedicated framebuffer interface is preferred.

// #define USE_PROPERTY_INTERFACE_FOR_FB

// Enable double buffering and vsync based page flipping
#define DOUBLE_BUFFER

#define VSYNCINT 16

// Field State bits (maintained in r3)
//
// bit 0 indicates mode 7
// bit 1 is the buffer being written to
// bit 2 is the buffer being displayed
// bit 3 is the field type (0 = odd, 1 = even) of the last field

#define BIT_MODE7        0x01
#define BIT_DRAW_BUFFER  0x02
#define BIT_DISP_BUFFER  0x04
#define BIT_ELK          0x08
#define BIT_PROBE        0x10
#define BIT_CALIBRATE    0x20
#define BIT_CAL_COUNT    0x40
#define BIT_INITIALIZE   0x80
#define BIT_FIELD_TYPE   0x100

// R0 return value bits
#define BIT_CAL          0x02

#define BIT_BOTH_BUFFERS (BIT_DRAW_BUFFER | BIT_DISP_BUFFER)

// Define the size of the Pi Framebuffer
//
// Nominal width should be 640x512, but making this
// a bit larger deals with two problems:
// 1. Slight differences in the horizontal placement
//    in the different screen modes
// 2. Slight differences in the vertical placement
//    due to *TV settings

#define SCREEN_WIDTH_MODE06      672
#define SCREEN_WIDTH_MODE7       504
#define SCREEN_HEIGHT            540

// The only supported depth is 4 bits per pixel
// Don't change this!
#define SCREEN_DEPTH               4

// Define the lines within the 312/313 line field
// that are actually scanned.
//
// Note: NUM_ACTIVE*2 <= SCREEN_HEIGHT
#define NUM_INACTIVE              21
#define NUM_ACTIVE               270

// Define the pixel clock for sampling
//
// This is a 4x pixel clock sent to the CPLD
//
// Don't change this!
#define CORE_FREQ          384000000

#define GPCLK_SOURCE               5      // PLLC (CORE_FREQ * 3)

#define DEFAULT_GPCLK_DIVISOR     12      // 96MHz
#define DEFAULT_CHARS_PER_LINE    81

#define MODE7_GPCLK_DIVISOR       12      // 96MHz
#define MODE7_CHARS_PER_LINE      63

// Pi 2/3 Multicore options
#if defined(RPI2) || defined(RPI3)

// Indicate the platform has multiple cores
#define HAS_MULTICORE

#endif

#ifdef __ASSEMBLER__

#define GPFSEL0 (PERIPHERAL_BASE + 0x200000)  // controls GPIOs 0..9
#define GPFSEL1 (PERIPHERAL_BASE + 0x200004)  // controls GPIOs 10..19
#define GPFSEL2 (PERIPHERAL_BASE + 0x200008)  // controls GPIOs 20..29
#define GPSET0  (PERIPHERAL_BASE + 0x20001C)
#define GPCLR0  (PERIPHERAL_BASE + 0x200028)
#define GPLEV0  (PERIPHERAL_BASE + 0x200034)
#define GPEDS0  (PERIPHERAL_BASE + 0x200040)
#define FIQCTRL (PERIPHERAL_BASE + 0x00B20C)

#define INTPEND2 (PERIPHERAL_BASE + 0x00B208)
#define SMICTRL  (PERIPHERAL_BASE + 0x600000)

#endif // __ASSEMBLER__

// Quad Pixel input on GPIOs 2..13
#define PIXEL_BASE   (2)

#define SW1_PIN      (16) // active low
#define SW2_PIN      (26) // active low
#define SW3_PIN      (19) // active low
#define PSYNC_PIN    (17)
#define CSYNC_PIN    (18)
#define MODE7_PIN    (22)
#define GPCLK_PIN    (21)
#define SP_CLK_PIN   (20)
#define SP_CLKEN_PIN (1)
#define SP_DATA_PIN  (23)
#define ELK_PIN      (24)
#define LINK_PIN     (25)
#define SPARE_PIN    (0)

#define CAL_PIN      SW1_PIN

// LED1 is left LED, driven by the Pi
// LED2 is the right LED, driven by the CPLD, as a copy of mode 7
// both LEDs are active low
#define LED1_PIN     (27)

#define CAL_MASK      (1 << CAL_PIN)
#define PSYNC_MASK    (1 << PSYNC_PIN)
#define CSYNC_MASK    (1 << CSYNC_PIN)

#define INTERLACED_FLAG (1 << 31)

#endif
