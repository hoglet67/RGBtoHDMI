#include <stdio.h>
#include <stdint.h>
#include "../filesystem.h"
#include "../logging.h"
#include "../osd.h"
#include "../rpi-gpio.h"
#include "../rgb_to_fb.h"
#include "../rgb_to_hdmi.h"

#include "ports.h"
#include "micro.h"
#include "update_cpld.h"

static FIL xsvf_file;
static FILINFO xsvf_info;

static unsigned char xsvf_buffer[1024*1024];

static char message[80];

int update_cpld(char *path, int show_message) {

   FRESULT result;
   UINT num_read;

   init_filesystem();

   result = f_stat(path, &xsvf_info);
   if (result != FR_OK) {
      log_warn("Failed to stat xsvf file %s (result = %d)", path, result);
      return 10;
   }

   result = f_open(&xsvf_file, path, FA_READ);
   if (result != FR_OK) {
      log_warn("Failed to open xsvf file %s (result = %d)", path, result);
      return 11;
   }

   result = f_read(&xsvf_file, xsvf_buffer, xsvf_info.fsize, &num_read);
   if (result != FR_OK) {
      log_warn("Failed to read xsvf file %s (result = %d)", path, result);
      return 12;
   }

   if (num_read != xsvf_info.fsize) {
      log_warn("Read incorrect amount of data (%d vs %d)", num_read, xsvf_info.fsize);
      return 13;
   }

   result = f_close(&xsvf_file);
   if (result != FR_OK) {
      log_warn("Failed to close xsvf file %s (result = %d)", path, result);
      return 14;
   }

   close_filesystem();
   RPI_SetGpioPinFunction(MUX_PIN,      FS_OUTPUT);

   log_info("Read xsvf file %s (length = %d)", path, xsvf_info.fsize);
   log_info("Programming....");
   RPI_SetGpioPinFunction(TDO_PIN, FS_INPUT);
   xsvf_iDebugLevel = 1;
   xsvf_data = xsvf_buffer;
   int xsvf_ret = xsvfExecute();
   RPI_SetGpioPinFunction(TDO_PIN, FS_OUTPUT);

   RPI_SetGpioPinFunction(MUX_PIN,      FS_INPUT);

   if (xsvf_ret != XSVF_ERROR_NONE) {
      sprintf(message, "Failed, error = %d", xsvf_ret);
      log_info(message);
      osd_set_clear(1, 0, message);
   } else {
      if (show_message) {
          for (int i = 5; i > 0; i--) {
             sprintf(message, "Successful, rebooting in %d secs ", i);
             log_info(message);
             osd_set_clear(1, 0, message);
             delay_in_arm_cycles_cpu_adjust(1000000000);
          }
      }
      reboot();
   }

   return xsvf_ret;
}
