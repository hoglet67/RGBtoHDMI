#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include "defs.h"
#include "osd.h"
void init_filesystem();
void capture_screenshot(capture_info_t *capinfo, char *profile);
void close_filesystem();
void scan_cpld_filenames(char cpld_filenames[MAX_CPLD_FILENAMES][MAX_FILENAME_WIDTH], char *path, int *count);
void scan_profiles(char *prefix, char manufacturer_names[MAX_PROFILES][MAX_PROFILE_WIDTH], char profile_names[MAX_PROFILES][MAX_PROFILE_WIDTH], int has_sub_profiles[MAX_PROFILES], char *path, size_t *mcount, size_t *count);
void scan_sub_profiles(char sub_profile_names[MAX_SUB_PROFILES][MAX_PROFILE_WIDTH], char *sub_path, size_t *count);
void write_profile_choice(char *profile_name, int saved_config_number, char* cpld_name);
unsigned int file_read_profile(char *profile_name, int saved_config_number, char *sub_profile_name, int updatecmd, char *command_string, unsigned int buffer_size);
void scan_rnames(char names[MAX_NAMES][MAX_NAMES_WIDTH], char *path, char *type, int truncate, size_t *count);
int file_save_config(char *resolution_name, int refresh, int scaling, int filtering, int current_frontend, int current_hdmi_mode, int current_hdmi_auto, char *auto_workaround_path);
int file_load(char *path, char *buffer, unsigned int buffer_size);
int file_save_custom_profile(char *name, char *buffer, unsigned int buffer_size);
int file_save(char *dirpath, char *name, char *buffer, unsigned int buffer_size, int saved_config_number);
int file_restore(char *dirpath, char *name, int saved_config_number);
   int create_and_scan_palettes(char names[MAX_NAMES][MAX_NAMES_WIDTH], uint32_t palette_array[MAX_NAMES][MAX_PALETTE_ENTRIES]);
int file_save_bin(char *path, char *buffer, unsigned int buffer_size);
int check_file(char* file_path, char* string);
int test_file(char* file_path);
int file_delete(char* path);
#endif
