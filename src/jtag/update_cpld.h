#ifndef _UPDATE_CPLD_H
#define _UPDATE_CPLD_H

#include "../fatfs/ff.h"

extern unsigned char *xsvf_data;

int update_cpld(char *path, int show_message);

#endif
