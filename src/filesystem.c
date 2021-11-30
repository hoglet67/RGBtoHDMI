#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"
#include "fatfs/ff.h"
#include "filesystem.h"
#include "osd.h"
#include "rgb_to_fb.h"
#include "geometry.h"
#include "rgb_to_hdmi.h"
#include "startup.h"

#define USE_LODEPNG

#ifdef USE_LODEPNG
#include "lodepng.h"
#else
#include "tiny_png_out.h"
#endif

#define CAPTURE_FILE_BASE "capture"
#define CAPTURE_BASE "/Captures"
#define PROFILE_BASE "/Profiles"
#define SAVED_PROFILE_BASE "/Saved_Profiles"
#define PALETTES_BASE "/Palettes"
#define PALETTES_TYPE ".bin"

static FATFS fsObject;
static int capture_id = -1;

#ifdef USE_LODEPNG

static int generate_png(capture_info_t *capinfo, uint8_t **png, unsigned int *png_len ) {
   LodePNGState state;
   lodepng_state_init(&state);
   if (capinfo->bpp < 16) {
       state.info_raw.colortype = LCT_PALETTE;
       state.info_raw.bitdepth = 8;
       state.info_png.color.colortype = LCT_PALETTE;
       state.info_png.color.bitdepth = 8;
       for (int i = 0; i < (1 << capinfo->bpp); i++) {
          int triplet = osd_get_palette(i);
          int r = triplet & 0xff;
          int g = (triplet >> 8) & 0xff;
          int b = (triplet >> 16) & 0xff;
          lodepng_palette_add(&state.info_png.color, r, g, b, 255);
          lodepng_palette_add(&state.info_raw, r, g, b, 255);
       }
   } else {
       state.info_raw.colortype = LCT_RGB;
       state.info_raw.bitdepth = 8;
       state.info_png.color.colortype = LCT_RGB;
       state.info_png.color.bitdepth = 8;
   }

   int width = capinfo->width;
   int width43 = width;
   int height = capinfo->height;
   int capscale = get_capscale();

   int hscale = get_hscale();
   int vscale = get_vscale();

   int hdouble = (hscale & 0x80000000) ? 1 : 0;
   int vdouble = (vscale & 0x80000000) ? 1 : 0;

   hscale &= 0xff;
   vscale &= 0xff;

   int png_width = (width >> hdouble) * hscale;
   int png_height = (height >> vdouble) * vscale;

   if (capinfo->mode7) {
       double ratio = (double) (png_width * 16 / 12) / png_height;
       if (ratio > 1.34 && (capscale == SCREENCAP_HALF43 || capscale == SCREENCAP_FULL43)) {
           width43 = (((int)(((double) width * 4 / 3) / ratio) * 12 / 16) >> 2) << 2;
       }
   } else {
       double ratio = (double) png_width / png_height;
       if (ratio > 1.34 && (capscale == SCREENCAP_HALF43 || capscale == SCREENCAP_FULL43)) {
           width43 = ((int)(((double) width * 4 / 3) / ratio) >> 2) << 2;
       }
   }

   png_width = (width43 >> hdouble) * hscale;

   int leftclip = (width - width43) / 2;
   int rightclip = leftclip + width43;

   log_info("Scaling is %d x %d x=%d y=%d sx=%d sy=%d px=%d py=%d", hscale, vscale, width, height, width/(hdouble + 1), height/(vdouble + 1), png_width, png_height);

   width = (width >> hdouble) << hdouble;
   width43 = (width >> hdouble) << hdouble;
   height = (height >> vdouble) << vdouble;

   if (capinfo->bpp == 16) {
       int left;
       int right;
       int top;
       int bottom;
       int png_left = 0;
       int png_right = 0;

       get_config_overscan(&left, &right, &top, &bottom);
       if (get_startup_overscan() != 0 && (left != 0 || right != 0) && (capscale == SCREENCAP_HALF || capscale == SCREENCAP_FULL)) {
           png_left = left * png_width / get_hdisplay();
           png_right = right * png_width / get_hdisplay();
       }
       log_info("png is %d - %d, %d - %d", left, right, png_left, png_right);

       uint8_t png_buffer[(png_width + png_left + png_right) *3 * png_height]  __attribute__((aligned(32)));
       uint8_t *pp = png_buffer;

           for (int y = 0; y < height; y += (vdouble + 1)) {
                for (int sy = 0; sy < vscale; sy++) {
                    uint8_t *fp = capinfo->fb + capinfo->pitch * y;
                    if (png_left != 0) {
                        for (int x = 0; x < png_left; x += (hdouble + 1)) {
                            *pp++ = 0;
                            *pp++ = 0;
                            *pp++ = 0;
                        }
                    }
                    for (int x = 0; x < width; x += (hdouble + 1)) {
                        uint8_t  single_pixel_lo = *fp++;
                        uint8_t  single_pixel_hi = *fp++;
                        int single_pixel = single_pixel_lo | (single_pixel_hi << 8);
                        uint8_t single_pixel_A = (single_pixel >> 12) & 0x0f;
                        uint8_t single_pixel_R = (single_pixel >> 8) & 0x0f;
                        uint8_t single_pixel_G = (single_pixel >> 4) & 0x0f;
                        uint8_t single_pixel_B = single_pixel & 0x0f;
                        if (single_pixel_A != 0x0f) {
                            single_pixel_R = single_pixel_R * single_pixel_A / 15;
                            single_pixel_G = single_pixel_G * single_pixel_A / 15;
                            single_pixel_B = single_pixel_B * single_pixel_A / 15;
                        }
                        single_pixel_R |= (single_pixel_R << 4);
                        single_pixel_G |= (single_pixel_G << 4);
                        single_pixel_B |= (single_pixel_B << 4);
                        if (hdouble) fp += 2;
                        if (x >= leftclip && x < rightclip) {
                            for (int sx = 0; sx < hscale; sx++) {
                                *pp++ = single_pixel_R;
                                *pp++ = single_pixel_G;
                                *pp++ = single_pixel_B;
                            }
                        }
                    }
                    if (png_right != 0) {
                        for (int x = 0; x < png_right; x += (hdouble + 1)) {
                            *pp++ = 0;
                            *pp++ = 0;
                            *pp++ = 0;
                        }
                    }

                }
           }
       log_info("Encoding png");
       unsigned int result = lodepng_encode(png, png_len, png_buffer, (png_width + png_left + png_right), png_height, &state);
       if (result) {
          log_warn("lodepng_encode32 failed (result = %d)", result);
          return 1;
       }
       return 0;
   } else {
   uint8_t png_buffer[png_width * png_height]  __attribute__((aligned(32)));
   uint8_t *pp = png_buffer;
       if (capinfo->bpp == 8) {
           for (int y = 0; y < height; y += (vdouble + 1)) {
                for (int sy = 0; sy < vscale; sy++) {
                    uint8_t *fp = capinfo->fb + capinfo->pitch * y;
                    for (int x = 0; x < width; x += (hdouble + 1)) {
                        uint8_t single_pixel = *fp++;
                        if (hdouble) fp++;
                        if (x >= leftclip && x < rightclip) {
                            for (int sx = 0; sx < hscale; sx++) {
                                *pp++ = single_pixel;
                            }
                        }
                    }
                }
           }
       } else {
           for (int y = 0; y < height; y += (vdouble + 1)) {
                for (int sy = 0; sy < vscale; sy++) {
                    uint8_t *fp = capinfo->fb + capinfo->pitch * y;
                    uint8_t single_pixel = 0;
                    for (int x = 0; x < width; x += (hdouble + 1)) {
                        if (hdouble) {
                            single_pixel = *fp++;
                            if (x >= leftclip && x < rightclip) {
                                for (int sx = 0; sx < hscale; sx++) {
                                    *pp++ = single_pixel >> 4;
                                }
                            }
                        } else {
                            if ((x & 1) == 0) {
                                single_pixel = *fp++;
                                if (x >= leftclip && x < rightclip) {
                                    for (int sx = 0; sx < hscale; sx++) {
                                        *pp++ = single_pixel >> 4;
                                    }
                                }
                            } else {
                                if (x >= leftclip && x < rightclip) {
                                    for (int sx = 0; sx < hscale; sx++) {
                                        *pp++ = single_pixel & 0x0f;
                                    }
                                }
                            }
                        }
                    }
                }
           }
       }
       log_info("Encoding png");
       unsigned int result = lodepng_encode(png, png_len, png_buffer, png_width, png_height, &state);
       if (result) {
          log_warn("lodepng_encode32 failed (result = %d)", result);
          return 1;
       }
       return 0;
   }


}

static void free_png(uint8_t *png) {
   if (png) {
      free(png);
   }
}

#else

// TODO: Fix hard-coded max H resolution of 4096
static uint8_t pixels[3 * 4096]  __attribute__((aligned(32)));

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


static void initialize_capture_id(char * path) {
   FRESULT result;
   DIR dir;
   FILINFO fno;

   // Check if already initialized
   // TODO: This would fail if cards were swapped live...
   //if (capture_id >= 0) { // doesn't work because of different folders for different profiles
   //   return;
   //}

   // Open root directory
   result = f_opendir(&dir, path);
   if (result != FR_OK) {
      log_warn("Failed to open %s", path);
      return;
   }

   // Iterate through files in root directory looking for existing capture files
   int len = 0;
   int baselen = strlen(CAPTURE_FILE_BASE);
   capture_id = -1;
   do {
      result = f_readdir(&dir, &fno);
      if (result != FR_OK) {
         log_warn("Failed to read %s", path);
         break;
      }
      len = strlen(fno.fname);
      if (len > 0) {
         if (strncasecmp(fno.fname, CAPTURE_FILE_BASE, baselen) == 0) {
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
      log_warn("Failed to close %s", path);
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

void capture_screenshot(capture_info_t *capinfo, char *profile) {
   FRESULT result;
   char path[256];
   char filepath[256];
   FIL file;
   uint8_t *png;
   unsigned int png_len;

#ifdef DISABLE_SCREENCAPS
    clear_menu_bits();
    osd_clear();
    osd_set(0, ATTR_DOUBLE_SIZE, "Screen Capture");
    osd_set(2, 0, "Not yet working on Pi 4");
    return;
#endif

   init_filesystem();

   result = f_mkdir(CAPTURE_BASE);
   if (result != FR_OK && result != FR_EXIST) {
       log_warn("Failed to create dir %s (result = %d)",CAPTURE_BASE, result);
   }

   sprintf(path, "%s/%s", CAPTURE_BASE, profile);
   result = f_mkdir(path);
   if (result != FR_OK && result != FR_EXIST) {
           log_warn("Failed to create dir %s (result = %d)", path, result);
   }

   initialize_capture_id(path);

   sprintf(filepath, "%s/%s%d.png",path, CAPTURE_FILE_BASE, capture_id);

   log_info("Screen capture starting, file = %s", filepath);

   result = f_open(&file, filepath, FA_CREATE_NEW | FA_WRITE);
   if (result != FR_OK) {
      log_warn("Failed to create capture file %s (result = %d)", filepath, result);
      return;
   }
   capture_id++;

   if (generate_png(capinfo, &png, &png_len)) {

      log_warn("generate_png failed, not writing data");

   } else {
      clear_menu_bits();
      osd_clear();
      osd_set(0, ATTR_DOUBLE_SIZE, "Screen Capture");
      osd_set(2, 0, filepath);
      log_info("Screen capture PNG length = %d, writing data...", png_len);

      UINT num_written = 0;
      result = f_write(&file, png, png_len, &num_written);
      if (result != FR_OK) {
         log_warn("Failed to write capture file %s (result = %d)", filepath, result);
      } else if (num_written != png_len) {
         log_warn("Capture file %s incomplete (%d < %d bytes)", filepath, num_written, png_len);
      }
   }

   free_png(png);

   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close capture file %s (result = %d)", filepath, result);
   }

   close_filesystem();

   log_info("Screen capture complete");

}

unsigned int file_read_profile(char *profile_name, int saved_config_number, char *sub_profile_name, int updatecmd, char *command_string, unsigned int buffer_size) {
   FRESULT result;
   char path[256];
   char cmdline[100];
   FIL file;
   unsigned int bytes_read = 0;
   unsigned int num_written = 0;
   init_filesystem();

   if (updatecmd) {
   char name[100];
   sprintf(name, "/profile_%s.txt", cpld->name);
   result = f_open(&file, name, FA_WRITE | FA_CREATE_ALWAYS);
       if (result != FR_OK) {
          log_warn("Failed to open %s (result = %d)", path, result);
          close_filesystem();
          return 0;
       } else {

          sprintf(cmdline, "profile=%s\r\n", profile_name);
          int cmdlength = strlen(cmdline);
          sprintf(cmdline + cmdlength, "saved_profile=%d\r\n", saved_config_number);
          cmdlength += strlen(cmdline + cmdlength);
          result = f_write(&file, cmdline, cmdlength, &num_written);

          if (result != FR_OK) {
                log_warn("Failed to write %s (result = %d)", path, result);
                close_filesystem();
                return 0;
             } else if (num_written != cmdlength) {
                log_warn("%s is incomplete (%d < %d bytes)", path, num_written, cmdlength);
                close_filesystem();
                return 0;
             }

          result = f_close(&file);
          if (result != FR_OK) {
             log_warn("Failed to close %s (result = %d)", path, result);
             close_filesystem();
             return 0;
          }
       }
   }

   if (saved_config_number == 0) {
       if (sub_profile_name != NULL) {
            sprintf(path, "%s/%s/%s/%s.txt", SAVED_PROFILE_BASE, cpld->name, profile_name, sub_profile_name);
       } else {
            sprintf(path, "%s/%s/%s.txt", SAVED_PROFILE_BASE, cpld->name, profile_name);
       }
   } else {
       if (sub_profile_name != NULL) {
            sprintf(path, "%s/%s/%s/%s_%d.txt", SAVED_PROFILE_BASE, cpld->name, profile_name, sub_profile_name, saved_config_number);
       } else {
            sprintf(path, "%s/%s/%s_%d.txt", SAVED_PROFILE_BASE, cpld->name, profile_name, saved_config_number);
       }
   }

   result = f_open(&file, path, FA_READ);
   if (result != FR_OK) {
       if (sub_profile_name != NULL) {
            sprintf(path, "%s/%s/%s/%s.txt", PROFILE_BASE, cpld->name, profile_name, sub_profile_name);
       } else {
            sprintf(path, "%s/%s/%s.txt", PROFILE_BASE, cpld->name, profile_name);
       }
       result = f_open(&file, path, FA_READ);
       if (result != FR_OK) {
            log_warn("Failed to open profile %s (result = %d)", path, result);
       close_filesystem();
       return 0;
       }
   }

   log_info("Loading file: %s", path);

   result = f_read (&file, command_string, buffer_size, &bytes_read);

   if (result != FR_OK) {
      bytes_read = 0;
      log_warn("Failed to read profile file %s (result = %d)", path, result);
      close_filesystem();
      return 0;
   } else {
      command_string[bytes_read] = 0;
   }

   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close profile %s (result = %d)", path, result);
      close_filesystem();
      return 0;
   }

   close_filesystem();

   log_debug("Profile loading complete");

   if (bytes_read == 0) {
       command_string[0] = 0;
   }

   return bytes_read;
}

static int string_compare (const void * s1, const void * s2) {
    return strcmp (s1, s2);
}

void scan_cpld_filenames(char cpld_filenames[MAX_CPLD_FILENAMES][MAX_FILENAME_WIDTH], char *path, int *count) {
    FRESULT res;
    DIR dir;
    FILINFO fno;
    *count = 0;
    init_filesystem();
    res = f_opendir(&dir, path);
    if (res == FR_OK) {
       while (1) {
          res = f_readdir(&dir, &fno);
          if (res != FR_OK || fno.fname[0] == 0 || *count == MAX_CPLD_FILENAMES) {
             break;
          }
          if (!(fno.fattrib & AM_DIR)) {
             if (fno.fname[0] != '.' && (strlen(fno.fname) > 5) != 0) {
                char* filetype = fno.fname + strlen(fno.fname)-5;
                if (strcmp(filetype, ".xsvf") == 0) {
                   strncpy(cpld_filenames[*count], fno.fname, MAX_FILENAME_WIDTH);
                   cpld_filenames[*count][strlen(fno.fname) - 5] = 0;
                   (*count)++;
                }
             }
          }
       }
       f_closedir(&dir);
       qsort(cpld_filenames, *count, sizeof *cpld_filenames, string_compare);
    }
    close_filesystem();
}


void scan_profiles(char profile_names[MAX_PROFILES][MAX_PROFILE_WIDTH], int has_sub_profiles[MAX_PROFILES], char *path, size_t *count) {
    FRESULT res;
    DIR dir;
    FIL file;
    char fpath[256];
    static FILINFO fno;
    init_filesystem();
    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0 || *count == MAX_PROFILES) break;
            if (fno.fattrib & AM_DIR) {
                strncpy(profile_names[*count], fno.fname, MAX_PROFILE_WIDTH);
                (*count)++;
            } else {
                if (fno.fname[0] != '.' && strlen(fno.fname) > 4 && strcmp(fno.fname, DEFAULTTXT_STRING) != 0) {
                    char* filetype = fno.fname + strlen(fno.fname)-4;
                    if (strcmp(filetype, ".txt") == 0) {
                        strncpy(profile_names[*count], fno.fname, MAX_PROFILE_WIDTH);
                        profile_names[*count][strlen(fno.fname) - 4] = 0;
                        (*count)++;
                    }
                }
            }
        }
        f_closedir(&dir);
        qsort(profile_names, *count, sizeof *profile_names, string_compare);
        for (int i = 0; i < (*count); i++) {
            sprintf(fpath, "%s/%s.txt", path, profile_names[i]);
            res = f_open(&file, fpath, FA_READ);
            if (res == FR_OK) {
                f_close(&file);
                has_sub_profiles[i] = 0;
            } else {
                has_sub_profiles[i] = 1;
            }
        }
    }
    close_filesystem();
}

void scan_sub_profiles(char sub_profile_names[MAX_SUB_PROFILES][MAX_PROFILE_WIDTH], char *sub_path, size_t *count) {
    FRESULT res;
    DIR dir;
    static FILINFO fno;
    char path[100] = "/Profiles/";
    strncat(path, cpld->name, 80);
    strncat(path, "/", 80);
    strncat(path, sub_path, 80);
    init_filesystem();
    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0 || *count == MAX_SUB_PROFILES) break;
            if (!(fno.fattrib & AM_DIR)) {
                if (fno.fname[0] != '.' && strlen(fno.fname) > 4 && strcmp(fno.fname, DEFAULTTXT_STRING) != 0) {
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
        qsort(sub_profile_names, *count, sizeof *sub_profile_names, string_compare);
    }
    close_filesystem();
}

void scan_rnames(char names[MAX_NAMES][MAX_NAMES_WIDTH], char *path, char *type, int truncate, size_t *count) {
    FRESULT res;
    DIR dir;
    static FILINFO fno;
    init_filesystem();
    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0 || *count == MAX_NAMES) break;
            if (!(fno.fattrib & AM_DIR)) {
                if (fno.fname[0] != '.' && strlen(fno.fname) > 4) {
                    char* filetype = fno.fname + strlen(fno.fname) - 4;
                    if (strcmp(filetype, type) == 0) {
                        strncpy(names[*count], fno.fname, MAX_NAMES_WIDTH);
                        names[(*count)++][strlen(fno.fname) - truncate] = 0;
                    }
                }
            }
        }
        f_closedir(&dir);
        //mask out bit so numbers starting >5 sort before other numbers so 640, 720 & 800 appear before 1024 1280 etc
        for (int i = 0; i < *count; i++) {
            if (names[i][0] > '5' && names[i][0] <= '9') {
                names[i][0] &= 0xef;
            }
        }
        qsort(names, *count, sizeof *names, string_compare);
        //restore masked bit
        for (int i = 0; i < *count; i++) {
           if (names[i][0] > ('5' & 0xef) && names[i][0] <= ('9' & 0xef)) {
               names[i][0] |= 0x10;
           }
        }
    }
    close_filesystem();
}

int file_save_raw(char *path, char *buffer, unsigned int buffer_size) {
   FRESULT result;
   FIL file;
   unsigned int bytes_written = 0;
   log_info("Saving file %s", path);
   result = f_open(&file, path,  FA_WRITE | FA_CREATE_ALWAYS);
   if (result != FR_OK) {
      log_warn("Failed to open %s (result = %d)", path, result);
      return 0;
   }
   result = f_write(&file, buffer, buffer_size, &bytes_written);
   if (result != FR_OK) {
      log_warn("Failed to read %s (result = %d)", path, result);
      return 0;
   }
   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close %s (result = %d)", path, result);
      return 0;
   }

   //log_info("%s reading complete", path);
   return bytes_written;
}

int file_save_bin(char *path, char *buffer, unsigned int buffer_size) {
    init_filesystem();
    int result = file_save_raw(path, buffer, buffer_size);
    close_filesystem();
    return result;
}

int file_load_raw(char *path, char *buffer, unsigned int buffer_size) {
   FRESULT result;
   FIL file;
   unsigned int bytes_read = 0;
   log_info("Loading file %s", path);
   result = f_open(&file, path, FA_READ);
   if (result != FR_OK) {
      log_warn("Failed to open %s (result = %d)", path, result);
      return 0;
   }
   result = f_read (&file, buffer, buffer_size, &bytes_read);
   if (result != FR_OK) {
      log_warn("Failed to read %s (result = %d)", path, result);
      return 0;
   }
   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close %s (result = %d)", path, result);
      return 0;
   }
   buffer[bytes_read] = 0;
   //log_info("%s reading complete", path);
   return bytes_read;
}

int file_load(char *path, char *buffer, unsigned int buffer_size) {
    init_filesystem();
    int result = file_load_raw(path, buffer, buffer_size);
    close_filesystem();
    return result;
}

int file_save(char *dirpath, char *name, char *buffer, unsigned int buffer_size, int saved_config_number) {
   FRESULT result;
   FIL file;
   unsigned int num_written = 0;
   unsigned int bytes_read = 0;
   char path[256];
   char comparison_path[256];
   char comparison_buffer[MAX_BUFFER_SIZE];
   char temp_buffer[MAX_BUFFER_SIZE];
   int status = 0;
   init_filesystem();

   result = f_mkdir(SAVED_PROFILE_BASE);
   if (result != FR_OK && result != FR_EXIST) {
       log_warn("Failed to create dir %s (result = %d)",SAVED_PROFILE_BASE, result);
   }
   sprintf(path, "%s/%s", SAVED_PROFILE_BASE, cpld->name);

   result = f_mkdir(path);
   if (result != FR_OK && result != FR_EXIST) {
       log_warn("Failed to create dir %s (result = %d)",path, result);
   }

   if (dirpath != NULL) {
       sprintf(path, "%s/%s/%s", SAVED_PROFILE_BASE, cpld->name, dirpath);
       result = f_mkdir(path);
       if (result != FR_OK && result != FR_EXIST) {
           log_warn("Failed to create dir %s (result = %d)", dirpath, result);
       }
       if (saved_config_number == 0) {
          sprintf(path, "%s/%s/%s/%s.txt", SAVED_PROFILE_BASE, cpld->name, dirpath, name);
       } else {
          sprintf(path, "%s/%s/%s/%s_%d.txt", SAVED_PROFILE_BASE, cpld->name, dirpath, name, saved_config_number);
       }
       sprintf(comparison_path, "%s/%s/%s/%s.txt", PROFILE_BASE, cpld->name, dirpath, name);
   } else {
       if (saved_config_number == 0) {
          sprintf(path, "%s/%s/%s.txt", SAVED_PROFILE_BASE, cpld->name, name);
       } else {
          sprintf(path, "%s/%s/%s_%d.txt", SAVED_PROFILE_BASE, cpld->name, name, saved_config_number);
       }
       sprintf(comparison_path, "%s/%s/%s.txt", PROFILE_BASE, cpld->name, name);
   }

   log_info("Loading comparison file %s", comparison_path);

   result = f_open(&file, comparison_path, FA_READ);
   if (result != FR_OK) {
      log_warn("Failed to open %s (result = %d)", comparison_path, result);
   }

   comparison_buffer[0] = 0;    // if comparison file missing then default to zero length
   result = f_read (&file, comparison_buffer, MAX_BUFFER_SIZE - 1, &bytes_read);

   if (result != FR_OK) {
      log_warn("Failed to read %s (result = %d)", comparison_path, result);
   }

   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close %s (result = %d)", comparison_path, result);
   }

   // see if entries in buffer differ from comparison buffer
   comparison_buffer[bytes_read] = 0;
   strcpy(temp_buffer, buffer);
   char *temp_pointer = temp_buffer;
   int different = 0;
   while (temp_pointer[0] != 0) {
       char *eol = strstr(temp_pointer, "\r\n");
       int eol_size = 0;
       if (eol != NULL) {
           eol[0] = 0;
           eol[1] = 0;
           eol_size = 2;
       } else {
           eol = strchr(temp_pointer, '\n');
           if (eol != NULL) {
               eol[0] = 0;
               eol_size = 1;
           }
       }
       if (strstr(comparison_buffer, temp_pointer) == NULL) {
           different = 1;
       }
       temp_pointer += strlen(temp_pointer) + eol_size;
   }

   // now see if entries in comparison buffer differ from buffer in case entries are present in original profile but removed from new profile
   strcpy(temp_buffer, comparison_buffer);
   temp_pointer = temp_buffer;
   while (temp_pointer[0] != 0) {
       char *eol = strstr(temp_pointer, "\r\n");
       int eol_size = 0;
       if (eol != NULL) {
           eol[0] = 0;
           eol[1] = 0;
           eol_size = 2;
       } else {
           eol = strchr(temp_pointer, '\n');
           if (eol != NULL) {
               eol[0] = 0;
               eol_size = 1;
           }
       }
       if (strstr(buffer, temp_pointer) == NULL) {
           different = 1;
       }
       temp_pointer += strlen(temp_pointer) + eol_size;
   }

   if (different) {
       log_info("Saving file %s", path);

       result = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS);
       if (result != FR_OK) {
          log_warn("Failed to open %s (result = %d)", path, result);
          close_filesystem();
          return result;
       }

       result = f_write(&file, buffer, buffer_size, &num_written);

       if (result != FR_OK) {
          log_warn("Failed to read %s (result = %d)", path, result);
          close_filesystem();
          return result;
       }

       result = f_close(&file);
       if (result != FR_OK) {
          log_warn("Failed to close %s (result = %d)", path, result);
          close_filesystem();
          return result;
       }
       log_info("%s writing complete", path);

   } else {
       log_info("File matches default deleting %s", path);
       result = f_unlink(path);
       if (result != FR_OK && result != FR_NO_FILE) {
           log_warn("Failed to delete %s (result = %d)", path, result);
           close_filesystem();
           return result;
       }
       log_info("%s deleting complete", path);
       status = -1;
   }
   close_filesystem();
   return status;
}

int file_restore(char *dirpath, char *name, int saved_config_number) {
   FRESULT result;
   char path[256];

   init_filesystem();

   result = f_mkdir(SAVED_PROFILE_BASE);
   if (result != FR_OK && result != FR_EXIST) {
       log_warn("Failed to create dir %s (result = %d)",SAVED_PROFILE_BASE, result);
   }

   if (dirpath != NULL) {
       sprintf(path, "%s/%s/%s", SAVED_PROFILE_BASE, cpld->name, dirpath);
       result = f_mkdir(path);
       if (result != FR_OK && result != FR_EXIST) {
           log_warn("Failed to create dir %s (result = %d)", dirpath, result);
       }
       if (saved_config_number == 0) {
          sprintf(path, "%s/%s/%s/%s.txt", SAVED_PROFILE_BASE, cpld->name, dirpath, name);
       } else {
          sprintf(path, "%s/%s/%s/%s_%d.txt", SAVED_PROFILE_BASE, cpld->name, dirpath, name, saved_config_number);
       }
   } else {
       if (saved_config_number == 0) {
          sprintf(path, "%s/%s/%s.txt", SAVED_PROFILE_BASE, cpld->name, name);
       } else {
          sprintf(path, "%s/%s/%s_%d.txt", SAVED_PROFILE_BASE, cpld->name, name, saved_config_number);
       }
   }
   log_info("File restored by deleting %s", path);
   result = f_unlink(path);
   if (result != FR_OK && result != FR_NO_FILE) {
       log_warn("Failed to restore by delete %s (result = %d)", path, result);
       close_filesystem();
       return 0;
   }

   close_filesystem();
   return 1;
}

int file_save_config(char *resolution_name, int refresh, int scaling, int filtering, int current_frontend, int current_hdmi_mode) {
   FRESULT result;
   char path[256];
   char buffer [16384];
   FIL file;
   unsigned int bytes_read = 0;
   unsigned int bytes_read2 = 0;
   unsigned int num_written = 0;
   init_filesystem();

   log_info("Loading config");

   result = f_open(&file, "/config.txt", FA_READ);
   if (result != FR_OK) {
      log_warn("Failed to open default config (result = %d)", result);
      close_filesystem();
      return 0;
   }
   result = f_read (&file, buffer, 8192, &bytes_read);

   if (result != FR_OK) {
      bytes_read = 0;
      log_warn("Failed to read config (result = %d)", result);
      close_filesystem();
      return 0;
   }

   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close config (result = %d)", result);
      close_filesystem();
      return 0;
   }

   char * endptr = buffer;
   while (endptr < (buffer + bytes_read)) {
      if (strncasecmp(endptr, "[all]", 5) == 0) {
          endptr = endptr + 5;
          log_info("Found end marker in config file");
          break;
      }
      endptr++;
   }

   bytes_read = endptr - buffer;

   sprintf((char*)(buffer + bytes_read), "\r\n");
   bytes_read += 2;

   if (current_hdmi_mode == 0) {
       sprintf((char*)(buffer + bytes_read), "\r\nhdmi_drive=1\r\n");
   } else {
       sprintf((char*)(buffer + bytes_read), "\r\nhdmi_drive=2\r\n");
       bytes_read += strlen((char*) (buffer + bytes_read));
       sprintf((char*)(buffer + bytes_read), "hdmi_pixel_encoding=%d\r\n", current_hdmi_mode - 1);
   }
   bytes_read += strlen((char*) (buffer + bytes_read));

   sprintf((char*)(buffer + bytes_read), "\r\n#refresh=%d\r\n", refresh);
   bytes_read += strlen((char*) (buffer + bytes_read));

   sprintf((char*)(buffer + bytes_read), "\r\n#resolution=%s\r\n", resolution_name);
   bytes_read += strlen((char*) (buffer + bytes_read));
   if (refresh == REFRESH_50) {
       sprintf(path, "/Resolutions/50Hz/%s@50Hz.txt", resolution_name);
   } else {
       sprintf(path, "/Resolutions/60Hz/%s@60Hz.txt", resolution_name);
   }
   log_info("Loading file: %s", path);

   result = f_open(&file, path, FA_READ);

   if (result != FR_OK) {
       sprintf(path, "/Resolutions/%s.txt", resolution_name);
       log_info("Not found, loading file: %s", path);
       result = f_open(&file, path, FA_READ);
       if (result != FR_OK) {
          log_warn("Failed to open resolution %s (result = %d)", path, result);
          close_filesystem();
          return 0;
       }
   }

   result = f_read (&file, (char*) (buffer + bytes_read), 1024, &bytes_read2);

   if (result != FR_OK) {
      log_warn("Failed to read resolution file %s (result = %d)", path, result);
      close_filesystem();
      return 0;
   }

   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close resolution file %s (result = %d)", path, result);
      close_filesystem();
      return 0;
   }
   bytes_read += bytes_read2;

   sprintf((char*)(buffer + bytes_read), "\r\n#scaling=%d\r\n", scaling);
   bytes_read += strlen((char*) (buffer + bytes_read));
   sprintf((char*) (buffer + bytes_read), "scaling_kernel=%d\r\n\r\n", filtering);
   bytes_read += strlen((char*) (buffer + bytes_read));
   log_info("Save scaling kernel = %d", filtering);

   for (int i = 0; i < 16; i++) {
       int frontend = get_existing_frontend(i);
       if (i == ((cpld->get_version() >> VERSION_DESIGN_BIT) & 0x0F)) {
           frontend = current_frontend;
       }
       if (frontend != 0) {
           sprintf((char*)(buffer + bytes_read), "#interface_%X=%d\r\n", i, frontend);
           bytes_read += strlen((char*) (buffer + bytes_read));
           log_info("Save interface_%X = %d", i, frontend);
       }
   }

   buffer[bytes_read]=0;
   result = f_open(&file, "/config.txt", FA_WRITE | FA_CREATE_ALWAYS);
   if (result != FR_OK) {
      log_warn("Failed to open config.txt (result = %d)", result);
      close_filesystem();
      return 0;
   } else {

      result = f_write(&file, buffer, bytes_read, &num_written);

      if (result != FR_OK) {
            log_warn("Failed to write config.txt (result = %d)", result);
            close_filesystem();
            return 0;
      } else if (num_written != bytes_read) {
            log_warn("config.txt is incomplete (%d < %d bytes)", num_written, bytes_read);
            close_filesystem();
            return 0;
      }

      result = f_close(&file);
      if (result != FR_OK) {
         log_warn("Failed to close config.txt (result = %d)", result);
         close_filesystem();
         return 0;
      }
   }

   close_filesystem();
   log_info("Config.txt update is complete");
   return 1;
}


int file_save_palette(char *name, char *buffer, unsigned int buffer_size) {
   FRESULT result;
   FIL file;
   unsigned int num_written = 0;
   char path[256];
   result = f_mkdir(PALETTES_BASE);
   if (result != FR_OK && result != FR_EXIST) {
       log_warn("Failed to create dir %s (result = %d)",PALETTES_BASE, result);
   }
   sprintf(path, "%s/%s.bin", PALETTES_BASE, name);
   result = f_open(&file, path, FA_WRITE | FA_CREATE_NEW);
   if (result == FR_EXIST) {
      //log_info("Palette file %s already exists", path);
      return 1;
   }
   if (result != FR_OK) {
      log_warn("Failed to open %s (result = %d)", path, result);
      return result;
   }
   result = f_write(&file, buffer, buffer_size, &num_written);

   if (result != FR_OK) {
      log_warn("Failed to write %s (result = %d)", path, result);
      return result;
   }
   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close %s (result = %d)", path, result);
      return result;
   }
   log_info("Palette %s writing complete", path);
   return 1;
}

int create_and_scan_palettes(char names[MAX_NAMES][MAX_NAMES_WIDTH], uint32_t palette_array[MAX_NAMES][MAX_PALETTE_ENTRIES]) {
    int count = 0;
    char path[256];
    FRESULT res;
    DIR dir;
    static FILINFO fno;
    init_filesystem();

    for (int i = 0;i < NUM_PALETTES; i++) {
        file_save_palette((char *) names[i], (char *) palette_array[i], sizeof(palette_array[i]));
    }

    res = f_opendir(&dir, PALETTES_BASE);
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0 || count == MAX_NAMES) break;
            if (!(fno.fattrib & AM_DIR)) {
                if (fno.fname[0] != '.' && strlen(fno.fname) > 4) {
                    char* filetype = fno.fname + strlen(fno.fname)-4;
                    if (strcmp(filetype, PALETTES_TYPE) == 0) {
                        strncpy(names[count], fno.fname, MAX_NAMES_WIDTH);
                        names[count++][strlen(fno.fname) - 4] = 0;
                    }
                }
            }
        }
        f_closedir(&dir);
        qsort(names, count, sizeof *names, string_compare);
    }

    if (count !=0) {
      for (int i = 0; i < count; i++) {
         sprintf(path, "%s/%s%s", PALETTES_BASE, names[i], PALETTES_TYPE);
         //log_info("READING PALETTE: %s", names[i]);
         file_load_raw(path, (char*) palette_array[i], sizeof(palette_array[i]));
      }
    } else {
         strcpy(names[0], NOT_FOUND_STRING);
    }

    close_filesystem();

    return count;
}

int check_file(char* file_path, char* string){
    FRESULT result;
    FIL file;
    FILINFO info;
    unsigned int num_written = 0;
    init_filesystem();
    result = f_stat(file_path, &info);
    if (result != FR_OK) {
        log_info("Re-creating %s", file_path);
        result = f_open(&file, file_path, FA_WRITE | FA_CREATE_NEW);
        if (result == FR_OK) {
           f_write(&file, string, strlen(string), &num_written);
           f_close(&file);
           log_info("File %s re-created", file_path);
           close_filesystem();
           return (1);
        }
    }
    close_filesystem();
    return (0);
}

int test_file(char* file_path){
    FRESULT result;
    FILINFO info;
    init_filesystem();
    result = f_stat(file_path, &info);
    close_filesystem();
    if (result == FR_OK) {
        return (1);
    }
    return (0);
}