#include <stdio.h>
#include "../filesystem.h"
#include "../logging.h"
#include "../rpi-gpio.h"
#include "../rgb_to_fb.h"
#include "../rgb_to_hdmi.h"

#include "ports.h"
#include "micro.h"
#include "update_cpld.h"

static FIL xsvf_file;
static FILINFO xsvf_info;

static unsigned char xsvf_buffer[1024*1024];

int update_cpld(char *path) {

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

   log_info("Read xsvf file %s (length = %d)", path, xsvf_info.fsize);
   log_info("Programming....");
   RPI_SetGpioPinFunction(TDO_PIN, FS_INPUT);
   xsvf_iDebugLevel = 0;
   xsvf_data = xsvf_buffer;
   int xsvf_ret = xsvfExecute();
   RPI_SetGpioPinFunction(TDO_PIN, FS_OUTPUT);

   if (xsvf_ret != XSVF_ERROR_NONE) {
      log_info("Programming failed, error code %d", xsvf_ret);
   } else {
      log_info("Programming successful");
      for (int i = 5; i > 0; i--) {
         log_info("...rebooting in %d", i);
         delay_in_arm_cycles(1000000000);
      }
      reboot();
   }

   return xsvf_ret;
}
