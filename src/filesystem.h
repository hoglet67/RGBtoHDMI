#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include "defs.h"
void init_filesystem();

void capture_screenshot(capture_info_t *capinfo);

void close_filesystem();

void scan_profiles(char profile_names[MAX_PROFILES][MAX_PROFILE_WIDTH], int has_sub_profiles[MAX_PROFILES], char *path, size_t *count);
void scan_sub_profiles(char sub_profile_names[MAX_SUB_PROFILES][MAX_PROFILE_WIDTH], char *sub_path, size_t *count);

unsigned int file_read_profile(char *profile_name, char *sub_profile_name, int profile_number, int sub_profile_number, char *command_string, unsigned int buffer_size);
#endif
