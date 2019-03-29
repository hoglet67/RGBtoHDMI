#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"
#include "fatfs/ff.h"
#include "filesystem.h"
#include "osd.h"

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


   for (int i = 0; i < (1 << capinfo->bpp); i++) {
      int triplet = osd_get_palette(i);
      int r = triplet & 0xff;
      int g = (triplet >> 8) & 0xff;
      int b = (triplet >> 16) & 0xff;
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
      osd_set(0, ATTR_DOUBLE_SIZE, "Screen Capture");
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

unsigned int file_read_profile(char *profile_name, char *sub_profile_name, int profile_number, int sub_profile_number, char *command_string, unsigned int buffer_size) {
   FRESULT result;
   char path[256];
   char cmdline[100];
   FIL file;
   unsigned int bytes_read = 0;
   unsigned int num_written = 0;
   init_filesystem();
   if (sub_profile_name != NULL) {
        sprintf(path, "/Profiles/%s/%s.txt", profile_name, sub_profile_name);
   } else {
        sprintf(path, "/Profiles/%s.txt", profile_name);
   }

   log_info("Loading profile starting, file = %s", path);

   result = f_open(&file, path, FA_READ);
   if (result != FR_OK) {
      log_info("Failed to open profile %s (result = %d)", path, result);
      return 0;
   }
   result = f_read (&file, command_string, buffer_size, &bytes_read);

   if (result != FR_OK) {
      bytes_read = 0;
      log_info("Failed to read profile file %s (result = %d)", path, result);
   } else {
      command_string[bytes_read] = 0;
   }

   result = f_close(&file);
   if (result != FR_OK) {
      log_info("Failed to close profile %s (result = %d)", path, result);
   }

   if (profile_number >= 0 && sub_profile_number >= 0) {
   result = f_open(&file, "/cmdline.txt", FA_WRITE);
   if (result != FR_OK) {
      log_warn("Failed to open %s (result = %d)", path, result);
   } else {

      sprintf(cmdline, "profile=%d subprofile=%d\n", profile_number, sub_profile_number);
      int cmdlength = strlen(cmdline);
      result = f_write(&file, cmdline, cmdlength, &num_written);

      if (result != FR_OK) {
            log_warn("Failed to write %s (result = %d)", path, result);
         } else if (num_written != cmdlength) {
            log_warn("%s is incomplete (%d < %d bytes)", path, num_written, cmdlength);
         }

      result = f_close(&file);
      if (result != FR_OK) {
         log_info("Failed to close %s (result = %d)", path, result);
      }
   }

   }
   close_filesystem();

   log_info("Profile loading complete");

   if (bytes_read == 0) {
       command_string[0] = 0;
   }

   return bytes_read;
}

void scan_profiles(char profile_names[MAX_PROFILES][MAX_PROFILE_WIDTH], int has_sub_profiles[MAX_PROFILES], char *path, size_t *count) {
    FRESULT res;
    DIR dir;
    static FILINFO fno;
    init_filesystem();
    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0 || *count == MAX_PROFILES) break;
            if (fno.fattrib & AM_DIR) {
                    strncpy(profile_names[*count], fno.fname, MAX_PROFILE_WIDTH);
                    has_sub_profiles[(*count)++] = 1;
            } else {
                if (strlen(fno.fname) > 4 && strcmp(fno.fname, DEFAULTTXT_STRING) != 0) {
                    char* filetype = fno.fname + strlen(fno.fname)-4;
                    if (strcmp(filetype, ".txt") == 0) {
                        strncpy(profile_names[*count], fno.fname, MAX_PROFILE_WIDTH);
                        profile_names[*count][strlen(fno.fname) - 4] = 0;
                        has_sub_profiles[(*count)++] = 0;
                    }
                }
            }
        }
        f_closedir(&dir);
    }
    close_filesystem();
}

void scan_sub_profiles(char sub_profile_names[MAX_SUB_PROFILES][MAX_PROFILE_WIDTH], char *sub_path, size_t *count) {
    FRESULT res;
    DIR dir;
    static FILINFO fno;
    char path[100] = "/Profiles/";
    strncat(path, sub_path, 80);
    init_filesystem();
    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0 || *count == MAX_SUB_PROFILES) break;
            if (!(fno.fattrib & AM_DIR)) {
                if (strlen(fno.fname) > 4 && strcmp(fno.fname, "Default.txt") != 0) {
                    char* filetype = fno.fname + strlen(fno.fname)-4;
                    if (strcmp(filetype, ".txt") == 0) {
                        strncpy(sub_profile_names[*count], fno.fname, MAX_PROFILE_WIDTH);
                        sub_profile_names[*count][strlen(fno.fname) - 4] = 0;
                        (*count)++;
                    }
                }
            }
        }
        f_closedir(&dir);
    }
    close_filesystem();
}










