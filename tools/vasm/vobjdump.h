/*
 * vobjdump
 * Views the contents of a VOBJ file.
 * Written by Frank Wille <frank@phoenix.owl.de>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

/* symbol types */
#define LABSYM 1
#define IMPORT 2
#define EXPRESSION 3

/* symbol flags */
#define TYPE(sym) ((sym)->flags&7)
#define TYPE_UNKNOWN  0
#define TYPE_OBJECT   1
#define TYPE_FUNCTION 2
#define TYPE_SECTION  3
#define TYPE_FILE     4

#define EXPORT 8
#define INEVAL 16
#define COMMON 32
#define WEAK 64


typedef int64_t taddr;
typedef uint8_t ubyte;

struct vobj_symbol {
  size_t offs;
  const char *name;
  int type,flags,sec,size;
  taddr val;
};

struct vobj_section {
  size_t offs;
  const char *name;
  taddr dsize,fsize;
};

#define makemask(x) ((taddr)(1LL<<(x))-1)
