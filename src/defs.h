// defs.h

#ifndef DEFS_H
#define DEFS_H

#define _RPI  1
#define _RPI2 2
#define _RPI3 3
#define _RPI4 4

#ifdef USE_ARM_CAPTURE
    #ifdef RPI4                                  // if using ARM CAPTURE then enable mode7 options only on RPI4 build
    #define USE_ALT_M7DEINTERLACE_CODE           // uses re-ordered code for mode7 deinterlace
    #define USE_CACHED_SCREEN                    // caches the upper half of the screen area and uses it for mode7 deinterlace
    #define CACHED_SCREEN_OFFSET    0x00B00000   // offset to cached screen area
    #define CACHED_SCREEN_SIZE      0x00100000   // size of cached screen area
    #endif
#else                                        // if not using ARM CAPTURE (i.e. using GPU CAPTURE) then enable mode7 options
#define USE_ALT_M7DEINTERLACE_CODE           // uses re-ordered code for mode7 deinterlace
#define USE_CACHED_SCREEN                    // caches the upper half of the screen area and uses it for mode7 deinterlace
#define CACHED_SCREEN_OFFSET    0x00B00000   // offset to cached screen area
#define CACHED_SCREEN_SIZE      0x00100000   // size of cached screen area
#endif

#define USE_MULTICORE

#define DONT_USE_MULTICORE_ON_PI2

// Define how the Pi Framebuffer is initialized
// - if defined, use the property interface (Channel 8)
// - if not defined, use to the the framebuffer interface (Channel 1)
//
// Note: there seem to be some weird caching issues with the property interface
// so using the dedicated framebuffer interface is preferred.
// but that doesn't work on the Pi3
#define USE_PROPERTY_INTERFACE_FOR_FB

#define HIDE_INTERFACE_SETTING

// Define the legal range of HDMI pixel clocks
#define MIN_PIXEL_CLOCK      24.5 //  24.5MHz
#define MAX_PIXEL_CLOCK     340.0 // 340MHz

// Enable multiple buffering and vsync based page flipping
#define MULTI_BUFFER

// The maximum number of buffers used by multi-buffering
// (it can be set to less that this on the OSD)
#define NBUFFERS 4

#define VSYNCINT 16

// Control bits (maintained in r3)

//the BITDUP bits reuse some bits in the inner capture loops

#define BIT_ELK                         0x01   // bit  0, indicates we are an Electron
#define BITDUP_LINE_CONDITION_DETECTED   0x01   // bit  0, indicates grey screen detected
#define BIT_PROBE                       0x02   // bit  1, indicates the mode is being determined
#define BITDUP_FFOSD_DETECTED            0x02   // bit  1, indicates ffosd detected
#define BIT_CALIBRATE                   0x04   // bit  2, indicates calibration is happening
#define BIT_OSD                         0x08   // bit  3, indicates the OSD is visible
#define BIT_FIELD_TYPE1_VALID           0x10   // bit  4, indicates FIELD_TYPE1 is valid
#define BITDUP_ENABLE_GREY_DETECT        0x10   // bit  4, enable grey screen detection
#define BIT_NO_LINE_DOUBLE              0x20   // bit  5, if set then lines aren't duplicated in capture
#define BIT_NO_SCANLINES                0x40   // bit  6, indicates scan lines should be made visible
#define BIT_INTERLACED_VIDEO            0x80   // bit  7, if set then interlaced video detected or teletext enabled
#define BIT_CLEAR                      0x100   // bit  8, indicates the frame buffer should be cleared
#define BITDUP_ENABLE_FFOSD             0x100   // bit  8, indicates FFOSD is enabled
#define BIT_VSYNC                      0x200   // bit  9, indicates the red v sync indicator should be displayed
#define BIT_VSYNC_MARKER               0x400   // bit 10, indicates current line should be replaced by the red vsync indicator
#define BIT_DEBUG                      0x800   // bit 11, indicates the debug grid should be displayed

#define OFFSET_LAST_BUFFER 12        // bit 12-13 LAST_BUFFER
#define MASK_LAST_BUFFER (3 << OFFSET_LAST_BUFFER)

#define BITDUP_RGB_INVERT            0x1000   //bit 12, indicates 9/12 bit RGB should be inverted
#define BITDUP_Y_INVERT              0x2000   //bit 13, indicates 9/12 bit G should be inverted

#define OFFSET_CURR_BUFFER 14        // bit 14-15 CURR_BUFFER
#define MASK_CURR_BUFFER (3 << OFFSET_CURR_BUFFER)

#define OFFSETDUP_PALETTE_HIGH_NIBBLE 12        // bit 12-15
#define MASKDUP_PALETTE_HIGH_NIBBLE (15 << OFFSETDUP_PALETTE_HIGH_NIBBLE)

#define BIT_INHIBIT_MODE_DETECT      0x10000       // bit 16 inhibit mode detection if sideways scrolling
#define BIT_PSYNC_POL                0x20000       // bit 17, indicates psync inversion (NEEDS TO MATCH PSYNC_PIN below)

#define OFFSET_NBUFFERS    18        // bit 18-19 NBUFFERS
#define MASK_NBUFFERS    (3 << OFFSET_NBUFFERS)

#define OFFSET_INTERLACE   20        // bit 20-22 INTERLACE
#define MASK_INTERLACE   (7 << OFFSET_INTERLACE)

#define BIT_FIELD_TYPE1           0x00800000  // bit 23, indicates the field type of the previous field
#define BITDUP_IIGS_DETECT        0x00800000  // bit 23, if set then apple IIGS mode detection enabled

#define BIT_FIELD_TYPE            0x01000000  // bit 24, indicates the field type (0 = odd, 1 = even) of the last field
#define BITDUP_MODE2_16COLOUR     0x01000000  // bit 24, if set then 16 colour mode 2 is emulated by decoding mode 0

#define BIT_OLD_FIRMWARE_SUPPORT  0x02000000  // bit 25, indicates old CPLD v1 or v2
                                             // then a second time to capture stable data. The v3 CPLD delays PSYNC a
                                             // couple of cycles, so the read that sees the edge will always capture
                                             // stable data. The second read is skipped in this case.
#define BIT_NO_H_SCROLL           0x04000000  // bit 26, if set then smooth H scrolling disabled
#define BIT_NO_SKIP_HSYNC         0x08000000  // bit 27, clear if hsync is ignored (used by cache preload)
#define BIT_HSYNC_EDGE            0x10000000  // bit 28, clear if trailing edge
#define BIT_RPI234                0x20000000  // bit 29, set if Pi 2, 3 or 4 detected
//#define BIT_                    0x40000000  // bit 30,
//#define BIT_                    0x80000000  // bit 31, may get corrupted - check

// R0 return value bits
#define RET_MODESET                0x01
#define RET_SW1                    0x02
#define RET_SW2                    0x04
#define RET_SW3                    0x08
#define RET_EXPIRED                0x10
#define RET_INTERLACE_CHANGED      0x20
#define RET_SYNC_TIMING_CHANGED    0x40
#define RET_SYNC_POLARITY_CHANGED  0x80
#define RET_SYNC_STATE_CHANGED    0x100

//paletteFlags
#define BIT_IN_BAND_ENABLE         0x01  // bit 0, if set in band data detection is enabled
#define BIT_IN_BAND_DETECTED       0x02  // bit 1, if set if in band data is detected
#define BIT_MODE2_PALETTE          0x04  // bit 2, if set mode 2 palette is customised
#define BIT_MODE7_PALETTE          0x08  // bit 3, if set mode 7 palette is customised
#define BIT_SET_MODE2_16COLOUR     0x10  // bit 4, if set mode 2 16 colour is enabled
#define BIT_MULTI_PALETTE         0x020   // bit 5  if set then multiple 16 colour palettes

#define VERTICAL_OFFSET         6      // start of actual bbc screen from start of buffer

// Offset definitions
#define NUM_OFFSETS  6

#define BIT_BOTH_BUFFERS (BIT_DRAW_BUFFER | BIT_DISP_BUFFER)

#ifdef __ASSEMBLER__
#define GPU_COMMAND_BASE_OFFSET 0x000000a0

//#define GPU_DATA_0  (PERIPHERAL_BASE + 0x000000a4)
//#define GPU_DATA_1  (PERIPHERAL_BASE + 0x000000a8)
//#define GPU_DATA_2  (PERIPHERAL_BASE + 0x000000ac)
//#define GPU_SYNC    (PERIPHERAL_BASE + 0x000000b0)  //gap in data block to allow fast 3 register read on ARM side
//#define GPU_DATA_3  (PERIPHERAL_BASE + 0x000000b4)  //using a single ldr and a two register ldmia
//#define GPU_DATA_4  (PERIPHERAL_BASE + 0x000000b8)  //can't use more than a single unaligned two register ldmia on the peripherals
//#define GPU_DATA_5  (PERIPHERAL_BASE + 0x000000bc)

#define GPU_COMMAND_offset 0x00
#define GPU_DATA_0_offset  0x04
#define GPU_DATA_1_offset  0x08
#define GPU_DATA_2_offset  0x0c
#define GPU_SYNC_offset    0x10
#define GPU_DATA_3_offset  0x14
#define GPU_DATA_4_offset  0x18
#define GPU_DATA_5_offset  0x1c

#define GPIO_BASE_OFFSET  0x200000
#define GPSET0_OFFSET     0x00001C
#define GPCLR0_OFFSET     0x000028
#define GPLEV0_OFFSET     0x000034

#if defined(RPI4)
#define INTPEND2_OFFSET   0x00B204    //SMI interrupt (GPU # 48 used for vsync) is actually in IRQ0_PENDING1 on pi 4 (0xfe00b204)
#else
#define INTPEND2_OFFSET   0x00B208
#endif

#define SMICTRL_OFFSET    0x600000

//#define GPFSEL0 (PERIPHERAL_BASE + 0x200000)  // controls GPIOs 0..9
//#define GPFSEL1 (PERIPHERAL_BASE + 0x200004)  // controls GPIOs 10..19
//#define GPFSEL2 (PERIPHERAL_BASE + 0x200008)  // controls GPIOs 20..29
//#define GPEDS0  (PERIPHERAL_BASE + 0x200040)
//#define GPREN0  (PERIPHERAL_BASE + 0x20004C)
//#define GPFEN0  (PERIPHERAL_BASE + 0x200058)
//#define GPAREN0 (PERIPHERAL_BASE + 0x20007C)
//#define GPAFEN0 (PERIPHERAL_BASE + 0x200088)
//#define FIQCTRL (PERIPHERAL_BASE + 0x00B20C)


// Offsets into capture_info_t structure below
#define O_FB_BASE          0
#define O_FB_PITCH         4
#define O_FB_WIDTH         8
#define O_FB_HEIGHT       12
#define O_FB_SIZEX2       16
#define O_FB_BPP          20
#define O_CHARS_PER_LINE  24
#define O_NLINES          28
#define O_H_OFFSET        32
#define O_V_OFFSET        36
#define O_NCAPTURE        40
#define O_PALETTE_CONTROL 44
#define O_SAMPLE_WIDTH    48
#define O_H_ADJUST        52
#define O_V_ADJUST        56
#define O_SYNCTYPE        60
#define O_DETSYNCTYPE     64
#define O_VSYNCTYPE       68
#define O_VIDEOTYPE       72
#define O_NTSCPHASE       76
#define O_BORDER          80
#define O_DELAY           84
#define O_INTENSITY       88
#define O_AUTOSWITCH      92
#define O_TIMINGSET       96
#define O_SYNCEDGE        100
#define O_MODE7           104
#define O_CAPTURE_LINE    108

#else

typedef struct {
   unsigned char *fb;  // framebuffer base address
   int pitch;          // framebuffer pitch (in bytes per line)
   int width;          // framebuffer width (in pixels)
   int height;         // framebuffer height (in pixels, after any doubling is applied)
   int sizex2;         // if 1 then double frame buffer height if 2 then double width 3=both
   int bpp;            // framebuffer bits per pixel (4 or 8)
   int chars_per_line; // active 8-pixel characters per line (83 in Modes 0..6, but 63 in Mode 7)
   int nlines;         // number of active lines to capture each field
   int h_offset;       // horizontal offset (in psync clocks)
   int v_offset;       // vertical offset (in lines)
   int ncapture;       // number of fields to capture, or -1 to capture forever
   int palette_control;// normal / in band data / ntsc artifacting etc
   int sample_width;   // 0(=3 bits) or 1(=6 bits)
   int h_adjust;       // h offset into large frame buffer
   int v_adjust;       // v offset into large frame buffer
   int sync_type;      // expected sync type and polarity
   int detected_sync_type;  // detected sync type and polarity
   int vsync_type;     // expected vertical sync type
   int video_type;     // expected video type / progressive / interlaced (teletext)
   int ntscphase;      // NTSC artifact colour phase
   int border;         // border logical colour
   int delay;          // delay value from sampling menu & 3
   int intensity;      // scanline intensity
   int autoswitch;     // autoswitch detect mode
   int timingset;      // 0 = set1, 1 = set 2
   int sync_edge;      // sync edge setting
   int mode7;          // mode7 flag
   int (*capture_line)(); // the capture line function to use
   int px_sampling;    // whether to sample normally, sub-sample or pixel double
} capture_info_t;

typedef struct {
   int clock;                // sample clock frequency (Hz)
   double line_len;          // length of a line (in sample clocks)
   int clock_ppm;            // sample clock frequency (Hz)
   int lines_per_frame;
} clk_info_t;

#endif // __ASSEMBLER__

// Quad Pixel input on GPIOs 2..13
#define PIXEL_BASE   (2)

#define SW1_PIN      (16) // active low
#define SW2_PIN      (26) // active low
#define SW3_PIN      (19) // active low
#define PSYNC_PIN    (17)
#define CSYNC_PIN    (23)
#define MODE7_PIN    (25)
#define GPCLK_PIN    (21)
#define SP_CLK_PIN   (20)
#define SP_CLKEN_PIN (1)
#define SP_DATA_PIN  (0)
#define MUX_PIN      (24)
#define STROBE_PIN   (22)
#define VERSION_PIN  (18) // active low, connects to GSR

// These pins are overloaded
#define TCK_PIN      (SP_CLK_PIN)
#define TMS_PIN      (SP_CLKEN_PIN)
#define TDI_PIN      (SP_DATA_PIN)
#define TDO_PIN      (MUX_PIN)

// LED1 is left LED, driven by the Pi
// LED2 is the right LED, driven by the CPLD, as a copy of mode 7
// both LEDs are active high
#define LED1_PIN     (27)

#define SW1_MASK      (1U << SW1_PIN)
#define SW2_MASK      (1U << SW2_PIN)
#define SW3_MASK      (1U << SW3_PIN)
#define PSYNC_MASK    (1U << PSYNC_PIN)
#define CSYNC_MASK    (1U << CSYNC_PIN)
#define LED1_MASK     (1U << LED1_PIN)
#define MODE7_MASK    (1U << MODE7_PIN)
#define VERSION_MASK  (1U << VERSION_PIN)
#define STROBE_MASK   (1U << STROBE_PIN)
#define SP_CLK_MASK   (1U << SP_CLK_PIN)
#define SP_DATA_MASK  (1U << SP_DATA_PIN)
#define MUX_MASK      (1U << MUX_PIN)

#define GPIO_FLOAT      0x00
#define GPIO_PULLDOWN   0x01
#define GPIO_PULLUP     0x02

#define SYNC_BIT_HSYNC_INVERTED   0x01      // bit  0, indicates hsync/composite sync is inverted
#define SYNC_BIT_VSYNC_INVERTED   0x02      // bit  1, indicates vsync is inverted
#define SYNC_BIT_COMPOSITE_SYNC   0x04      // bit  2, indicates composite sync
#define SYNC_BIT_MIXED_SYNC       0x08      // bit  3, indicates H and V syncs eored in CPLD
#define SYNC_BIT_INTERLACED       0x10      // bit  4, indicates interlaced sync detected
#define SYNC_BIT_MASK             0x07      // masks out bit 3

#define VSYNC_RETRY_MAX 10

#define MAX_STRING_SIZE 255
#define MIN_STRING_SIZE 127
#define MAX_STRING_LIMIT 253
#define MIN_STRING_LIMIT 125

#define MAX_CPLD_FILENAMES 24
#define MAX_FILENAME_WIDTH 40
#define MAX_PROFILES 256
#define MAX_SUB_PROFILES 64
#define MAX_FAVOURITES 10
#define MAX_PROFILE_WIDTH 256
#define MAX_BUFFER_SIZE 2048
#define MAX_CONFIG_BUFFER_SIZE 8192
#define DEFAULT_STRING "Default"
#define ROOT_DEFAULT_STRING "../Default"
#define DEFAULTTXT_STRING "Default.txt"
#define FAVOURITES_PATH "/favourites.txt"
#define FAVOURITES_MENU "Recently used"
#define FAVOURITES_MENU_CLEAR "Clear recently used"
#define NOT_FOUND_STRING "Not Found"
#define NONE_STRING "None"
#define MAX_NAMES 64
#define MAX_NAMES_WIDTH 32
#define MAX_JITTER_LINES 8

#define ONE_BUTTON_FILE "/Button_Mode.txt"
#define FORCE_BLANK_FILE "/cpld_firmware/Delete_This_File_To_Erase_CPLD.txt"
#define FORCE_UPDATE_FILE "/cpld_firmware/Delete_This_File_To_Check_CPLD.txt"
#define FORCE_BLANK_FILE_MESSAGE "Deleting this file will force the CPLD to be erased on the next reset\r\n"
#define FORCE_UPDATE_FILE_MESSAGE "Deleting this file will force a CPLD update check on the next reset\r\n"
#define BLANK_FILE "/cpld_firmware/recovery/blank/BLANK.xsvf"

#define PAXHEADER "PaxHeader"

#define NTSC_SOFT               0x04
#define NTSC_MEDIUM             0x08
#define NTSC_ARTIFACT           0x10
#define NTSC_ARTIFACT_SHIFT        4
#define NTSC_Y_INVERT           0x20
#define NTSC_LAST_ARTIFACT      0x40
#define NTSC_LAST_ARTIFACT_SHIFT   6
#define NTSC_HDMI_BLANK_ENABLE  0x80        //not actually ntsc but uses a spare bit
#define NTSC_LAST_IIGS         0x100        //not actually ntsc but uses a spare bit
#define NTSC_LAST_IIGS_SHIFT       8
#define NTSC_FFOSD_ENABLE      0x200        //not actually ntsc but uses a spare bit
#define NTSC_DONE_FIRST        0x400
#define NTSC_RGB_INVERT        0x800


#define BBC_VERSION 0x79
#define RGB_VERSION 0x94
#define YUV_VERSION 0x91

//these defines are adjusted for different clock speeds
#define FIELD_TYPE_THRESHOLD_BBC 39000            //  post frame sync times are ~22uS & ~54uS on beeb and ~34uS and ~66uS on Amiga so threshold of 45uS covers both
#define FIELD_TYPE_THRESHOLD_AMIGA 45000          //  post frame sync times are ~22uS & ~54uS on beeb and ~34uS and ~66uS on Amiga so threshold of 45uS covers both
#define ELK_LO_FIELD_SYNC_THRESHOLD 150000  // 150uS
#define ELK_HI_FIELD_SYNC_THRESHOLD 170000  // 170uS
#define ODD_THRESHOLD 22500
#define EVEN_THRESHOLD 54500
#define BBC_HSYNC_THRESHOLD 6144
#define NORMAL_HSYNC_THRESHOLD 9000
#define BLANKING_HSYNC_THRESHOLD 14000
#define EQUALISING_THRESHOLD 3400      // equalising pulses are half sync pulse length and must be filtered out
#define FRAME_MINIMUM 10000000         // 10ms
#define FRAME_TIMEOUT 30000000         // 30ms which is over a frame / field @ 50Hz (20ms)
#define LINE_MINIMUM 10000             // 10uS
#define HSYNC_SCROLL_LO (4000 - 224)
#define HSYNC_SCROLL_HI (4000 + 224)

#define FILTERING_NEAREST_NEIGHBOUR 8
#define FILTERING_SOFT 2
#define FILTERING_VERY_SOFT 6
#define DEFAULT_RESOLUTION "Auto"
#define DEFAULT_REFRESH 1
#define AUTO_REFRESH 2
#define DEFAULT_SCALING 0
#define DEFAULT_FILTERING 8
#define DEFAULT_HDMI_MODE 0

#define DISABLE_PI1_PI2_OVERCLOCK 1
#define DISABLE_SETTINGS_OVERCLOCK 2

#define POWERUP_MESSAGE_TIME 200

#if defined(RPI4)
#define LINE_TIMEOUT (100 * 1000/1000 * 1024)
#else
#define LINE_TIMEOUT (100 * 1024)
#endif

#if defined(RPI4)
#define CRYSTAL 54
#else
#define CRYSTAL 19.2
#endif

#define MAX_CPLD_FREQUENCY 196000000

#define GENLOCK_NLINES_THRESHOLD 350
#define GENLOCK_FORCE 1

#define GENLOCK_PPM_STEP 334
#define GENLOCK_MAX_STEPS 6
#define GENLOCK_THRESHOLDS {0, 5, 10, 16, 25, 35}
#define GENLOCK_LOCKED_THRESHOLD 2
#define GENLOCK_FRAME_DELAY 12
#define GENLOCK_SLEW_RATE_THRESHOLD 5000

#define MEASURE_NLINES 100
#define PLL_PPM_LO 1
#define PLL_PPM_LO_LIMIT 6
#define PLL_RESYNC_THRESHOLD_LO 3
#define PLL_RESYNC_THRESHOLD_HI 9
#define AVERAGE_VSYNC_TOTAL 125

#define BIT_NORMAL_FIRMWARE_V1 0x01
#define BIT_NORMAL_FIRMWARE_V2 0x02

// PLL registers, from:
// https://github.com/F5OEO/librpitx/blob/master/src/gpio.h
#define PLLA_ANA1 (0x1014/4)
#define PLLA_CTRL (0x1100/4)
#define PLLA_FRAC (0x1200/4)
#define PLLA_DSI0 (0x1300/4)
#define PLLA_CORE (0x1400/4)
#define PLLA_PER  (0x1500/4)
#define PLLA_CCP2 (0x1600/4)

#define PLLB_ANA1 (0x10f4/4)
#define PLLB_CTRL (0x11e0/4)
#define PLLB_FRAC (0x12e0/4)
#define PLLB_ARM  (0x13e0/4)
#define PLLB_SP0  (0x14e0/4)
#define PLLB_SP1  (0x15e0/4)
#define PLLB_SP2  (0x16e0/4)

#define PLLC_ANA1  (0x1034/4)
#define PLLC_CTRL  (0x1120/4)
#define PLLC_FRAC  (0x1220/4)
#define PLLC_CORE2 (0x1320/4)
#define PLLC_CORE1 (0x1420/4)
#define PLLC_PER   (0x1520/4)
#define PLLC_CORE0 (0x1620/4)

#define PLLD_ANA1 (0x1054/4)
#define PLLD_CTRL (0x1140/4)
#define PLLD_FRAC (0x1240/4)
#define PLLD_DSI0 (0x1340/4)
#define PLLD_CORE (0x1440/4)
#define PLLD_PER  (0x1540/4)
#define PLLD_DSI1 (0x1640/4)

#define PLLH_ANA1 (0x1074/4)
#define PLLH_CTRL (0x1160/4)
#define PLLH_FRAC (0x1260/4)
#define PLLH_AUX  (0x1360/4)
#define PLLH_RCAL (0x1460/4)
#define PLLH_PIX  (0x1560/4)
#define PLLH_STS  (0x1660/4)

#define XOSC_CTRL (0x1190/4)
#define XOSC_FREQUENCY 19200000

#define PI4_HDMI0_PLL (volatile uint32_t *)(_get_peripheral_base() + 0xf00f00)
#define PI4_HDMI0_DIVIDER   (0x28/4)
#define PI4_HDMI0_RM_OFFSET (0x98/4)   //actually offset 0x18 from the 0xf00f80 block

#if defined(RPI4)
#define PIXELVALVE2_HORZA (volatile uint32_t *)(_get_peripheral_base() + 0x20a00c)
#define PIXELVALVE2_HORZB (volatile uint32_t *)(_get_peripheral_base() + 0x20a010)
#define PIXELVALVE2_VERTA (volatile uint32_t *)(_get_peripheral_base() + 0x20a014)
#define PIXELVALVE2_VERTB (volatile uint32_t *)(_get_peripheral_base() + 0x20a018)
#define EMMC_LEGACY       (volatile uint32_t *)(_get_peripheral_base() + 0x2000d0)
#else
#define PIXELVALVE2_HORZA (volatile uint32_t *)(_get_peripheral_base() + 0x80700c)
#define PIXELVALVE2_HORZB (volatile uint32_t *)(_get_peripheral_base() + 0x807010)
#define PIXELVALVE2_VERTA (volatile uint32_t *)(_get_peripheral_base() + 0x807014)
#define PIXELVALVE2_VERTB (volatile uint32_t *)(_get_peripheral_base() + 0x807018)
#endif

#define PM_RSTC  (volatile uint32_t *)(_get_peripheral_base() + 0x10001c)
#define PM_WDOG (volatile uint32_t *)(_get_peripheral_base() + 0x100024)
#define PM_PASSWORD 0x5a000000
#define PM_RSTC_WRCFG_FULL_RESET 0x00000020

#define CM_PASSWORD                         0x5a000000
#define CM_PLL_LOADCORE                       (1 << 4)
#define CM_PLL_HOLDCORE                       (1 << 5)
#define CM_PLL_LOADPER                        (1 << 6)
#define CM_PLL_HOLDPER                        (1 << 7)
#define A2W_PLL_CHANNEL_DISABLE               (1 << 8)
#define GZ_CLK_BUSY                           (1 << 7)
#define GZ_CLK_ENA                            (1 << 4)
#define GP_CLK1_CTL (volatile uint32_t *)(_get_peripheral_base() + 0x101078)
#define GP_CLK1_DIV (volatile uint32_t *)(_get_peripheral_base() + 0x10107C)
#define CM_PLLA     (volatile uint32_t *)(_get_peripheral_base() + 0x101104)
#define CM_PLLC     (volatile uint32_t *)(_get_peripheral_base() + 0x101124)
#define CM_PLLD     (volatile uint32_t *)(_get_peripheral_base() + 0x101144)
#define CM_BASE     (volatile uint32_t *)(_get_peripheral_base() + 0x101000)

#define SCALER_DISPLIST1 (volatile uint32_t *)(_get_peripheral_base() + 0x400024)
#if defined(RPI4)
#define SCALER_DISPLAY_LIST (volatile uint32_t *)(_get_peripheral_base() + 0x404000)
#else
#define SCALER_DISPLAY_LIST (volatile uint32_t *)(_get_peripheral_base() + 0x402000)
#endif

#define PIXEL_FORMAT 1  // RGBA4444
#ifdef RPI4
#define PIXEL_ORDER 2   // ABGR in BCM2711
#else
#define PIXEL_ORDER 3   // ABGR
#endif

#define GREY_PIXELS 0xaaa
#define GREY_DETECTED_LINE_COUNT 200
#define ARTIFACT_DETECTED_LINE_COUNT 100
#define DPMS_FRAME_COUNT 200
#define IIGS_DETECTED_LINE_COUNT 128

#define  SIZEX2_DOUBLE_HEIGHT    1
#define  SIZEX2_DOUBLE_WIDTH     2
#define  SIZEX2_BASIC_SCANLINES  4

// can't use enums in assembler
#define   PALETTECONTROL_OFF                   0
#define   PALETTECONTROL_INBAND                1
#define   PALETTECONTROL_NTSCARTIFACT_CGA      2
#define   PALETTECONTROL_NTSCARTIFACT_BW       3
#define   PALETTECONTROL_NTSCARTIFACT_BW_AUTO  4
#define   PALETTECONTROL_ATARI_GTIA            5
#define   PALETTECONTROL_ATARI_LUMACODE        6
#define   PALETTECONTROL_C64_LUMACODE          7
#define   NUM_CONTROLS                         8

#define   INHIBIT_PALETTE_DIMMING_16_BIT 0x80000000

#define   AUTOSWITCH_OFF         0
#define   AUTOSWITCH_PC          1
#define   AUTOSWITCH_MODE7       2
#define   AUTOSWITCH_VSYNC       3
#define   AUTOSWITCH_IIGS        4
#define   AUTOSWITCH_IIGS_MANUAL 5
#define   AUTOSWITCH_MANUAL      6

#define   NUM_AUTOSWITCHES  7

#define  VSYNC_AUTO                    0
#define  VSYNC_INTERLACED              1
#define  VSYNC_INTERLACED_160          2
#define  VSYNC_NONINTERLACED           3
#define  VSYNC_NONINTERLACED_DEJITTER  4
#define  VSYNC_BLANKING                5
#define  VSYNC_POLARITY                6
#define  VSYNC_FORCE_INTERLACE         7
#define  NUM_VSYNC                     8

#define  VIDEO_PROGRESSIVE   0
#define  VIDEO_INTERLACED    1
#define  VIDEO_TELETEXT      2
#define  VIDEO_LINE_DOUBLED  3
#define  NUM_VIDEO           4

#define  SAMPLE_WIDTH_1    0
#define  SAMPLE_WIDTH_3    1
#define  SAMPLE_WIDTH_6    2
#define  SAMPLE_WIDTH_9LO  3
#define  SAMPLE_WIDTH_9HI  4
#define  SAMPLE_WIDTH_12   5

#define  MODE_SET1       0
#define  MODE_SET2       1

#define  SYNC_ABORT_FLAG   0x80000000
#define  LEADING_SYNC_FLAG 0x00010000
#define  SIMPLE_SYNC_FLAG  0x00008000
#define  HIGH_LATENCY_FLAG 0x00004000
#define  OLD_FIRMWARE_FLAG 0x00002000

#define  CPLD_NORMAL      0
#define  CPLD_BLANK       1
#define  CPLD_UNKNOWN     2
#define  CPLD_WRONG       3
#define  CPLD_MANUAL      4
#define  CPLD_UPDATE      5
#define  CPLD_NOT_FITTED  6

#endif

#define Bit32u uint32_t
#define Bit8u uint8_t
#define Bitu uint32_t