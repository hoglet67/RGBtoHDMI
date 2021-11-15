/* symtab.h  hashtable header file for vasm */
/* (c) in 2002-2004,2008,2014 by Volker Barthelmann and Frank Wille */

#include <stdlib.h>

typedef union hashdata {
  void *ptr;
  uint32_t idx;
} hashdata;

typedef struct hashentry {
  char *name;
  hashdata data;
  struct hashentry *next;
} hashentry;

typedef struct hashtable {
  hashentry **entries;
  size_t size;
  int collisions;
} hashtable;

hashtable *new_hashtable(size_t);
size_t hashcode(char *);
size_t hashcodelen(char *,int);
size_t hashcode_nc(char *);
size_t hashcodelen_nc(char *,int);
void add_hashentry(hashtable *,char *,hashdata);
void rem_hashentry(hashtable *,char *,int);
int find_name(hashtable *,char *,hashdata *);
int find_namelen(hashtable *,char *,int,hashdata *);
int find_name_nc(hashtable *,char *,hashdata *);
int find_namelen_nc(hashtable *,char *,int,hashdata *);
