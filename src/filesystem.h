#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include "defs.h"
void init_filesystem();

void capture_screenshot(capture_info_t *capinfo);

void close_filesystem();

void scan_profiles(char profile_names[MAX_PROFILES][MAX_PROFILE_WIDTH], int has_sub_profiles[MAX_PROFILES], char *path, size_t *count);
void scan_sub_profiles(char sub_profile_names[MAX_SUB_PROFILES][MAX_PROFILE_WIDTH], char *sub_path, size_t *count);

unsigned int file_read_profile(char *profile_name, char *sub_profile_name, int updatecmd, char *command_string, unsigned int buffer_size);
void scan_resolutions(char resolution_names[MAX_RESOLUTION][MAX_RESOLUTION_WIDTH], char *path, size_t *count);
int file_save_config(char *resolution_name, int interpolation);
int file_load(char *path, char *buffer, unsigned int buffer_size);
int file_save(char *dirpath, char *name, char *buffer, unsigned int buffer_size);
int file_restore(char *dirpath, char *name);
#endif
