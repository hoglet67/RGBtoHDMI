#include <stdio.h>
#include "defs.h"
#include "cpld.h"

// The CPLD version number
static int cpld_version;

// =============================================================
// Param definitions for OSD
// =============================================================


static param_t params[] = {
   {          -1,          NULL,          NULL, 0,   0, 1 }
};

// =============================================================
// Public methods
// =============================================================

static void cpld_init(int version) {
   cpld_version = version;
}

static int cpld_get_version() {
   return cpld_version;
}

static void cpld_calibrate(capture_info_t *capinfo, int elk) {
}

static void cpld_set_mode(int mode) {
}

static int cpld_analyse(int manual_setting) {
   return SYNC_BIT_COMPOSITE_SYNC;
}

static void cpld_update_capture_info(capture_info_t *capinfo) {
}

static param_t *cpld_get_params() {
   return params;
}

static int cpld_get_value(int num) {
   return 0;
}

static const char *cpld_get_value_string(int num) {
   return NULL;
}

static void cpld_set_value(int num, int value) {
}

static int cpld_show_cal_summary(int line) {
    return line;
}

static void cpld_show_cal_details(int line) {
}

static void cpld_show_cal_raw(int line) {
}

static int cpld_old_firmware_support() {
    return 0;
}

static int cpld_get_divider() {
    return 0;
}

static int cpld_get_delay() {
    return 0;
}
static int cpld_frontend_info() {
    return 0;
}
static void cpld_set_frontend(int value)
{
}

cpld_t cpld_null = {
   .name = "Unprogrammed",
   .default_profile = "BBC_Micro",
   .init = cpld_init,
   .get_version = cpld_get_version,
   .calibrate = cpld_calibrate,
   .set_mode = cpld_set_mode,
   .analyse = cpld_analyse,
   .old_firmware_support = cpld_old_firmware_support,
   .frontend_info = cpld_frontend_info,
   .set_frontend = cpld_set_frontend,
   .get_divider = cpld_get_divider,
   .get_delay = cpld_get_delay,
   .update_capture_info = cpld_update_capture_info,
   .get_params = cpld_get_params,
   .get_value = cpld_get_value,
   .get_value_string = cpld_get_value_string,
   .set_value = cpld_set_value,
   .show_cal_summary = cpld_show_cal_summary,
   .show_cal_details = cpld_show_cal_details,
   .show_cal_raw = cpld_show_cal_raw
};
