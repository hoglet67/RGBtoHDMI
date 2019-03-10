#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"
#include "fatfs/ff.h"
#include "tiny_png_out.h"
#include "filesystem.h"

#define CAPTURE_BASE "capture"

// TODO: Fix hard-coded max H resolution of 4096
static uint8_t pixels[3 * 4096];

// TODO: Fix hard-coded buffer size if 8MB
static uint8_t png_buffer[8 * 1024 * 1024] __attribute__((aligned(32)));

static FATFS fsObject;
static int capture_id = -1;

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
   enum TinyPngOut_Status tiny_result;
   struct TinyPngOut tiny_state;
   char path[100];
   FIL file;

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

   tiny_result = TinyPngOut_init(&tiny_state, capinfo->width, capinfo->height, png_buffer);

   if (tiny_result != TINYPNGOUT_OK) {

      log_warn("TinyPngOut_init failed (result = %d)", tiny_result);

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

         tiny_result = TinyPngOut_write(&tiny_state, pixels, capinfo->width);

         if (tiny_result != TINYPNGOUT_OK) {
            log_warn("TinyPngOut_write failed (result = %d)", tiny_result);
            break;
         }
      }
   }

   int png_len = tiny_state.output_len;
   log_info("Screen capture PNG length = %d, writing data...", png_len);

   UINT num_written = 0;
   result = f_write(&file, png_buffer, png_len, &num_written);
   if (result != FR_OK) {
      log_warn("Failed to write capture file %s (result = %d)", path, result);
   } else if (num_written != png_len) {
      log_warn("Capture file %s incomplete (%d < %d bytes)", path, num_written, png_len);
   }

   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close capture file %s (result = %d)", path, result);
   }

   close_filesystem();

   log_info("Screen capture complete");

}
