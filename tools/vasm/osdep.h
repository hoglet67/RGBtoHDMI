/* osdep.h - OS-dependant routines */
/* (c) in 2018,2020 by Frank Wille */

#if defined(AMIGA) || defined(MSDOS) || defined(ATARI) || defined(_WIN32)
#define filenamecmp(a,b) stricmp(a,b)
#else
#define filenamecmp(a,b) strcmp(a,b)
#endif

char *convert_path(char *);
char *append_path_delimiter(char *);
char *remove_path_delimiter(char *);
char *get_filepart(char *);
char *get_workdir(void);
int init_osdep(void);
