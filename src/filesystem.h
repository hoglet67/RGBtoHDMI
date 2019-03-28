#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include "defs.h"
void init_filesystem();

void capture_screenshot(capture_info_t *capinfo);

void close_filesystem();

char **scan_profiles(char *path, size_t *count);
char **scan_sub_profiles(char *path, size_t *count);

unsigned int file_read_profile(char *profile_name, char *sub_profile_name, int profile_number, int sub_profile_number, char *command_string, unsigned int buffer_size);
#endif
