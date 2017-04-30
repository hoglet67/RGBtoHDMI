// defs.h

#ifndef DEFS_H
#define DEFS_H

// Define the size of the Pi Framebuffer
//
// Nominal width should be 640x512, but making this
// a bit larger deals with two problems:
// 1. Slight differences in the horizontal placement
//    in the different screen modes
// 2. Slight differences in the vertical placement
//    due to *TV settings

#define SCREEN_WIDTH             672
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

#define DEFAULT_GPCLK_DIVISOR     18      // 64MHz
#define DEFAULT_CHARS_PER_LINE    81

#define MODE7_GPCLK_DIVISOR       24      // 48MHz
#define MODE7_CHARS_PER_LINE      63

// Pi 2/3 Multicore options
#if defined(RPI2) || defined(RPI3)

// Indicate the platform has multiple cores
#define HAS_MULTICORE

// Indicates we want to make active use of multiple cores
#define USE_MULTICORE

// Needs to match kernel_old setting in config.txt
//#define KERNEL_OLD

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

#endif // __ASSEMBLER__

// Quad Pixel input on GPIOs 2..13
#define PIXEL_BASE   (2)

#define PSYNC_PIN    (17)
#define CSYNC_PIN    (18)
#define MODE7_PIN    (19)
#define GPCLK_PIN    (21)

#define LED_PIN      (47)

#define PSYNC_MASK    (1 << PSYNC_PIN)
#define CSYNC_MASK    (1 << CSYNC_PIN)
#define MODE7_MASK    (1 << MODE7_PIN)

#define INTERLACED_FLAG (1 << 31)

#endif
