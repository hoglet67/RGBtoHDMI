#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"
#include "fatfs/ff.h"
#include "filesystem.h"

#define USE_LODEPNG

#ifdef USE_LODEPNG
#include "lodepng.h"
#else
#include "tiny_png_out.h"
#endif

#define CAPTURE_BASE "capture"

static FATFS fsObject;
static int capture_id = -1;

#ifdef USE_LODEPNG

static int generate_png(capture_info_t *capinfo, uint8_t **png, unsigned int *png_len ) {

   LodePNGState state;
   lodepng_state_init(&state);
   state.info_raw.colortype = LCT_PALETTE;
   state.info_raw.bitdepth = capinfo->bpp;
   state.info_png.color.colortype = LCT_PALETTE;
   state.info_png.color.bitdepth = 8;

   int width = capinfo->pitch;
   int height = capinfo->height;
   if (capinfo->bpp == 4) {
      width *= 2;
   }

   // TODO: Take account of current palette
   for (int i = 0; i < (1 << capinfo->bpp); i++) {
      int r = (i & 0x01) ? 255 : 0;
      int g = (i & 0x02) ? 255 : 0;
      int b = (i & 0x04) ? 255 : 0;
      lodepng_palette_add(&state.info_png.color, r, g, b, 255);
      lodepng_palette_add(&state.info_raw, r, g, b, 255);
   }

   unsigned result = lodepng_encode(png, png_len, capinfo->fb, width, height, &state);

   if (result) {
      log_warn("lodepng_encode32 failed (result = %d)", result);
      return 1;
   }

   return 0;

}

static void free_png(uint8_t *png) {
   if (png) {
      free(png);
   }
}

#else

// TODO: Fix hard-coded max H resolution of 4096
static uint8_t pixels[3 * 4096];

static uint8_t png_buffer[8 * 1024 * 1024] __attribute__((aligned(0x4000)));

static int generate_png(capture_info_t *capinfo, uint8_t **png, unsigned int *png_len ) {
   enum TinyPngOut_Status result;
   struct TinyPngOut state;

   *png = NULL;
   *png_len = 0;

   result = TinyPngOut_init(&state, capinfo->width, capinfo->height, png_buffer);

   if (result != TINYPNGOUT_OK) {

      log_warn("TinyPngOut_init failed (result = %d)", result);
      return 1;

   } else {

      // TODO: Take account of current palette

      for (int y = 0; y < capinfo->height; y++) {
         uint8_t *fp = capinfo->fb + capinfo->pitch * y;
         uint8_t *pp = pixels;
         if (capinfo->bpp == 8) {
            for (int x = 0; x < capinfo->width; x++) {
               uint8_t single_pixel = *fp++;
               *pp++ = (single_pixel & 0x01) ? 255 : 0;
               *pp++ = (single_pixel & 0x02) ? 255 : 0;
               *pp++ = (single_pixel & 0x04) ? 255 : 0;
            }
         } else {
            for (int x = 0; x < capinfo->width << 1; x++) {
               uint8_t double_pixel = *fp++;
               *pp++ = (double_pixel & 0x10) ? 255 : 0;
               *pp++ = (double_pixel & 0x20) ? 255 : 0;
               *pp++ = (double_pixel & 0x40) ? 255 : 0;
               *pp++ = (double_pixel & 0x01) ? 255 : 0;
               *pp++ = (double_pixel & 0x02) ? 255 : 0;
               *pp++ = (double_pixel & 0x04) ? 255 : 0;
            }
         }

         result = TinyPngOut_write(&state, pixels, capinfo->width);

         if (result != TINYPNGOUT_OK) {
            log_warn("TinyPngOut_write failed (result = %d)", result);
            return 1;
         }
      }
   }

   *png = png_buffer;
   *png_len = state.output_len;
   return 0;
}

static void free_png(uint8_t *png) {
}

#endif


static void initialize_capture_id() {
   FRESULT result;
   DIR dir;
   FILINFO fno;

   // Check if already initialized
   // TODO: This would fail if cards were swapped live...
   if (capture_id >= 0) {
      return;
   }

   // Open root directory
   result = f_opendir(&dir, "/");
   if (result != FR_OK) {
      log_warn("Failed to open root directory");
      return;
   }

   // Iterate through files in root directory looking for existing capture files
   int len = 0;
   int baselen = strlen(CAPTURE_BASE);
   capture_id = -1;
   do {
      result = f_readdir(&dir, &fno);
      if (result != FR_OK) {
         log_warn("Failed to read root directory");
         break;
      }
      len = strlen(fno.fname);
      if (len > 0) {
         if (strncasecmp(fno.fname, CAPTURE_BASE, baselen) == 0) {
            int id = atoi(fno.fname + baselen);
            if (id > capture_id) {
               capture_id = id;
            }
         }
      }
   } while (len > 0);
   // And increment it to the next free one
   capture_id++;

   // Close root directory
   result = f_closedir(&dir);
   if (result != FR_OK) {
      log_warn("Failed to close root directory");
   }
}


void init_filesystem() {
   FRESULT result;

   // Mount file system
   result = f_mount(&fsObject, "", 1);
   if (result != FR_OK) {
      log_warn("Failed to initialize file system");
      return;
   }
}

void close_filesystem() {
   FRESULT result;

   // UnMount file system
   result = f_mount(NULL, "", 1);
   if (result != FR_OK) {
      log_warn("Failed to close file system");
   }
}

void capture_screenshot(capture_info_t *capinfo) {
   FRESULT result;
   char path[100];
   FIL file;
   uint8_t *png;
   unsigned int png_len;

   init_filesystem();

   initialize_capture_id();

   sprintf(path, "%s%d.png", CAPTURE_BASE, capture_id);

   log_info("Screen capture starting, file = %s", path);

   result = f_open(&file, path, FA_CREATE_NEW | FA_WRITE);
   if (result != FR_OK) {
      log_warn("Failed to create capture file %s (result = %d)", path, result);
      return;
   }
   capture_id++;

   if (generate_png(capinfo, &png, &png_len)) {

      log_warn("generate_png failed, not writing data");

   } else {

      log_info("Screen capture PNG length = %d, writing data...", png_len);

      UINT num_written = 0;
      result = f_write(&file, png, png_len, &num_written);
      if (result != FR_OK) {
         log_warn("Failed to write capture file %s (result = %d)", path, result);
      } else if (num_written != png_len) {
         log_warn("Capture file %s incomplete (%d < %d bytes)", path, num_written, png_len);
      }
   }

   free_png(png);

   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close capture file %s (result = %d)", path, result);
   }

   close_filesystem();

   log_info("Screen capture complete");

}
