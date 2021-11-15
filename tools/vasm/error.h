/* error.h - error output and modification routines */
/* (c) in 2002-2009,2015,2018,2020 by Volker Barthelmann and Frank Wille */

#ifndef ERROR_H
#define ERROR_H

#define FIRST_GENERAL_ERROR 1
#define FIRST_SYNTAX_ERROR 1001
#define FIRST_CPU_ERROR 2001
#define FIRST_OUTPUT_ERROR 3001

struct err_out {
  char *text;
  int flags;
};
/*  Flags for err_out.flags    */
#define ERROR       1
#define WARNING     2
#define INTERNAL    8
#define FATAL      16
#define MESSAGE    32
#define DISABLED   64
#define NOLINE    256

#endif
