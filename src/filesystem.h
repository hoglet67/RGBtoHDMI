#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include "defs.h"
void init_filesystem();

void capture_screenshot(capture_info_t *capinfo);

void close_filesystem();

unsigned int file_read_profile(char *profile_name, int profile_number, char *command_string, unsigned int buffer_size);
#endif
