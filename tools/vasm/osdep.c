/* osdep.c - OS-dependant routines */
/* (c) in 2018,2020 by Frank Wille */

#include <string.h>
char *mystrdup(char *);
void *mymalloc(size_t);
struct symbol *internal_abs(char *);

#define MAX_WORKDIR_LEN 1024

#if defined(UNIX)
#include <unistd.h>

#elif defined(AMIGA)
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#ifdef __amigaos4__
#include <dos/obsolete.h>
#endif

#elif defined(ATARI)
#include <tos.h>

#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


char *convert_path(char *path)
{
  char *newpath;

#if defined(AMIGA)
  char *p = newpath = mymalloc(strlen(path)+1);

  while (*path) {
    if (*path=='.') {
      if (*(path+1)=='\0') {
        path++;
        continue;
      }
      else if (*(path+1)=='/' || *(path+1)=='\\') {
        path += 2;
        continue;
      }
      else if (*(path+1)=='.' &&
               (*(path+2)=='/' || *(path+2)=='\\'))
        path += 2;
    }
    if (*path == '\\') {
      *p++ = '/';
      path++;
    }
    else
      *p++ = *path++;
  }
  *p = '\0';

#elif defined(MSDOS) || defined(ATARI) || defined(_WIN32)
  char *p;

  newpath = mystrdup(path);
  for (p=newpath; *p; p++) {
    if (*p == '/')
      *p = '\\';
  }

#else /* Unixish */
  char *p;

  newpath = mystrdup(path);
  for (p=newpath; *p; p++) {
    if (*p == '\\')
      *p = '/';
  }
#endif

  return newpath;
}


char *append_path_delimiter(char *old)
{
  int len = strlen(old);
  char *new;

#if defined(AMIGA)
  if (len>0 && old[len-1]!='/' && old[len-1]!=':') {
    new = mymalloc(len+2);
    strcpy(new,old);
    new[len] = '/';
    new[len+1] = '\0';
  }
#elif defined(MSDOS) || defined(ATARI) || defined(_WIN32)
  if (len>0 && old[len-1]!='\\' && old[len-1]!=':') {
    new = mymalloc(len+2);
    strcpy(new,old);
    new[len] = '\\';
    new[len+1] = '\0';
  }
#else
  if (len>0 && old[len-1]!='/') {
    new = mymalloc(len+2);
    strcpy(new,old);
    new[len] = '/';
    new[len+1] = '\0';
  }
#endif
  else
    new = mystrdup(old);

  return new;
}


char *remove_path_delimiter(char *old)
{
  int len = strlen(old);
  char *new;

#if defined(AMIGA)
  if (len>1 && (old[len-1]=='/' || old[len-1]==':')) {
#elif defined(MSDOS) || defined(ATARI) || defined(_WIN32)
  if (len>1 && (old[len-1]=='\\' || old[len-1]==':')) {
#else
  if (len>1 && old[len-1]=='/') {
#endif
    new = mymalloc(len);
    memcpy(new,old,len-1);
    new[len-1] = '\0';
  }
  else
    new = mystrdup(old);

  return new;
}


char *get_filepart(char *path)
{
  char *filepart;

  if ((filepart = strrchr(path,'/')) != NULL ||
      (filepart = strrchr(path,'\\')) != NULL ||
      (filepart = strrchr(path,':')) != NULL)
    return filepart+1;
  return path;
}


#if defined(UNIX)
char *get_workdir(void)
{
  static char buf[MAX_WORKDIR_LEN];

  if (getcwd(buf,MAX_WORKDIR_LEN) == NULL)
    buf[0] = '\0';
  return buf;
}

#elif defined(AMIGA)
char *get_workdir(void)
{
  static char buf[MAX_WORKDIR_LEN];

#ifndef __amigaos4__
  if (DOSBase->dl_lib.lib_Version < 36)
    buf[0] = '\0';
  else
#endif
    if (!GetCurrentDirName(buf,MAX_WORKDIR_LEN))
      buf[0] = '\0';
  return buf;
}

#elif defined(ATARI)
char *get_workdir(void)
{
  static char buf[MAX_WORKDIR_LEN];
  WORD drv = Dgetdrv();

  buf[0] = 'A' + drv;
  buf[1] = ':';
  Dgetpath(buf+2,drv+1);
  return buf;
}

#elif defined(_WIN32)
char *get_workdir(void)
{
  static char buf[MAX_WORKDIR_LEN];

  GetCurrentDirectoryA(MAX_WORKDIR_LEN,buf);
  return buf;
}

#else  /* portable default */
char *get_workdir(void)
{
  return "";
}
#endif

int init_osdep(void)
{
#if defined(UNIX)
  internal_abs("__UNIXFS");
#elif defined(AMIGA)
  internal_abs("__AMIGAFS");
#elif defined(MSDOS) || defined(ATARI) || defined(_WIN32)
  internal_abs("__MSDOSFS");
#endif
  return 1;
}
