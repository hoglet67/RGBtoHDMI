#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"
#include "fatfs/ff.h"
#include "filesystem.h"
#include "osd.h"
#include "rgb_to_fb.h"
#include "geometry.h"

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
   state.info_raw.colortype = LCT_PALETTE;
   state.info_raw.bitdepth = 8;
   state.info_png.color.colortype = LCT_PALETTE;
   state.info_png.color.bitdepth = 8;

   int width = capinfo->width;
   int width43 = width;
   int height = capinfo->height;
   int capscale = get_capscale();

   if (geometry_get_mode()) {
       double ratio = (double) (width * 16 / 12) / height;
       if (ratio > 1.34 && (capscale == SCREENCAP_HALF43 || capscale == SCREENCAP_FULL43)) {
           width43 = (((height * 4 / 3) * 12 / 16) >> 2) << 2;
       }
   } else {
       double ratio = (double) width / height;
       if (ratio > 1.34 && (capscale == SCREENCAP_HALF43 || capscale == SCREENCAP_FULL43)) {
           width43 = ((height * 4 / 3) >> 2) << 2;
       }
   }

   int leftclip = (width - width43) / 2;
   int rightclip = leftclip + width43;

   for (int i = 0; i < (1 << capinfo->bpp); i++) {
      int triplet = osd_get_palette(i);
      int r = triplet & 0xff;
      int g = (triplet >> 8) & 0xff;
      int b = (triplet >> 16) & 0xff;
      lodepng_palette_add(&state.info_png.color, r, g, b, 255);
      lodepng_palette_add(&state.info_raw, r, g, b, 255);
   }

   int hscale = get_hscale();
   int vscale = get_vscale();

   int hdouble = (hscale & 0x80000000) ? 1 : 0;
   int vdouble = (vscale & 0x80000000) ? 1 : 0;

   hscale &= 0xff;
   vscale &= 0xff;

   int png_width = (width43 >> hdouble) * hscale;
   int png_height = (height >> vdouble) * vscale;

   log_info("Scaling is %d x %d x=%d y=%d sx=%d sy=%d px=%d py=%d", hscale, vscale, width, height, width/(hdouble + 1), height/(vdouble + 1), png_width, png_height);

   width = (width >> hdouble) << hdouble;
   width43 = (width >> hdouble) << hdouble;
   height = (height >> vdouble) << vdouble;

   uint8_t png_buffer[png_width * png_height];
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
   unsigned int result = lodepng_encode(png, png_len, png_buffer, png_width, png_height, &state);
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

unsigned int file_read_profile(char *profile_name, char *sub_profile_name, int updatecmd, char *command_string, unsigned int buffer_size) {
   FRESULT result;
   char path[256];
   char cmdline[100];
   FIL file;
   unsigned int bytes_read = 0;
   unsigned int num_written = 0;
   init_filesystem();

   if (sub_profile_name != NULL) {
        sprintf(path, "%s/%s/%s/%s.txt", SAVED_PROFILE_BASE, cpld->name, profile_name, sub_profile_name);
   } else {
        sprintf(path, "%s/%s/%s.txt", SAVED_PROFILE_BASE, cpld->name, profile_name);
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
             if ((strlen(fno.fname) > 5) != 0) {
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
                if (strlen(fno.fname) > 4 && strcmp(fno.fname, DEFAULTTXT_STRING) != 0) {
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
                if (strlen(fno.fname) > 4 && strcmp(fno.fname, DEFAULTTXT_STRING) != 0) {
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

void scan_names(char names[MAX_NAMES][MAX_NAMES_WIDTH], char *path, char *type, size_t *count) {
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
                if (strlen(fno.fname) > 4) {
                    char* filetype = fno.fname + strlen(fno.fname)-4;
                    if (strcmp(filetype, type) == 0) {
                        strncpy(names[*count], fno.fname, MAX_NAMES_WIDTH);
                        names[(*count)++][strlen(fno.fname) - 4] = 0;
                    }
                }
            }
        }
        f_closedir(&dir);
        qsort(names, *count, sizeof *names, string_compare);
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

int file_save(char *dirpath, char *name, char *buffer, unsigned int buffer_size) {
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
       sprintf(path, "%s/%s/%s/%s.txt", SAVED_PROFILE_BASE, cpld->name, dirpath, name);
       sprintf(comparison_path, "%s/%s/%s/%s.txt", PROFILE_BASE, cpld->name, dirpath, name);
   } else {
       sprintf(path, "%s/%s/%s.txt", SAVED_PROFILE_BASE, cpld->name, name);
       sprintf(comparison_path, "%s/%s/%s.txt", PROFILE_BASE, cpld->name, name);
   }

   log_info("Loading comparison file %s", comparison_path);

   result = f_open(&file, comparison_path, FA_READ);
   if (result != FR_OK) {
      log_warn("Failed to open %s (result = %d)", comparison_path, result);
      close_filesystem();
      return result;
   }
   result = f_read (&file, comparison_buffer, MAX_BUFFER_SIZE - 1, &bytes_read);

   if (result != FR_OK) {
      log_warn("Failed to read %s (result = %d)", comparison_path, result);
      close_filesystem();
      return result;
   }

   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close %s (result = %d)", comparison_path, result);
      close_filesystem();
      return result;
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

int file_restore(char *dirpath, char *name) {
   FRESULT result;
   char path[256];
   char *root = "/Saved_Profiles";

   init_filesystem();

   result = f_mkdir(root);
   if (result != FR_OK && result != FR_EXIST) {
       log_warn("Failed to create dir %s (result = %d)",root, result);
   }

   if (dirpath != NULL) {
       sprintf(path, "%s/%s/%s", root, cpld->name, dirpath);
       result = f_mkdir(path);
       if (result != FR_OK && result != FR_EXIST) {
           log_warn("Failed to create dir %s (result = %d)", dirpath, result);
       }
       sprintf(path, "%s/%s/%s/%s.txt", root, cpld->name, dirpath, name);
   } else {
       sprintf(path, "%s/%s/%s.txt", root, cpld->name, name);
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

int file_save_config(char *resolution_name, int scaling, int current_frontend) {
   FRESULT result;
   char path[256];
   char buffer [16384];
   FIL file;
   unsigned int bytes_read = 0;
   unsigned int bytes_read2 = 0;
   unsigned int num_written = 0;
   init_filesystem();

   log_info("Loading default config");

   result = f_open(&file, "/default_config.txt", FA_READ);
   if (result != FR_OK) {
      log_warn("Failed to open default config (result = %d)", result);
      close_filesystem();
      return 0;
   }
   result = f_read (&file, buffer, 8192, &bytes_read);

   if (result != FR_OK) {
      bytes_read = 0;
      log_warn("Failed to read default config (result = %d)", result);
      close_filesystem();
      return 0;
   }

   result = f_close(&file);
   if (result != FR_OK) {
      log_warn("Failed to close default config (result = %d)", result);
      close_filesystem();
      return 0;
   }
   sprintf((char*)(buffer + bytes_read), "\r\n#resolution=%s\r\n", resolution_name);
   bytes_read += strlen((char*) (buffer + bytes_read));
   sprintf(path, "/Resolutions/%s.txt", resolution_name);
   log_info("Loading file: %s", path);

   result = f_open(&file, path, FA_READ);
   if (result != FR_OK) {
      log_warn("Failed to open resolution %s (result = %d)", path, result);
      close_filesystem();
      return 0;
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
   int val = 8;
   if (scaling == 1 || scaling == 3 || scaling == 5) {
       val = 2;
   }
   if (scaling == 2 || scaling == 4 || scaling == 6) {
       val = 6;
   }

   sprintf((char*)(buffer + bytes_read), "\r\n#scaling=%d\r\n", scaling);
   bytes_read += strlen((char*) (buffer + bytes_read));
   sprintf((char*) (buffer + bytes_read), "scaling_kernel=%d\r\n\r\n", val);
   bytes_read += strlen((char*) (buffer + bytes_read));
   log_info("Save scaling kernel = %d", val);

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
                if (strlen(fno.fname) > 4) {
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