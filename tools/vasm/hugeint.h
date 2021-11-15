/* hugeint.h implements huge integer operations at 128 bits */
/* (c) in 2014 by Frank Wille */

#ifndef HUGEINT_H
#define HUGEINT_H

#include "tfloat.h"

typedef struct thuge {
  uint64_t hi,lo;
} thuge;

#define HUGEBITS 128
#define HUGESIGN(h) ((h.hi & 0x8000000000000000LL) != 0)
#define MAX_UINT64_FLOAT 1.8446744073709551616e+19


thuge huge_zero(void);
thuge huge_from_int(int64_t);
int64_t huge_to_int(thuge);
#if FLOAT_PARSER
thuge huge_from_float(tfloat);
tfloat huge_to_float(thuge);
#endif
thuge huge_from_mem(int,void *,size_t);
void *huge_to_mem(int,void *,size_t,thuge);
int huge_chkrange(thuge,int);

thuge hneg(thuge);
thuge hcpl(thuge);
thuge hnot(thuge);
thuge hand(thuge,thuge);
thuge hor(thuge,thuge);
thuge hxor(thuge,thuge);
thuge haddi(thuge,int64_t);
thuge hadd(thuge,thuge);
thuge hsub(thuge,thuge);
int hcmp(thuge,thuge);
thuge hshra(thuge,int);
thuge hshr(thuge,int);
thuge hshl(thuge,int);
thuge hmuli(thuge,int64_t);
thuge hmul(thuge,thuge);
thuge hdiv(thuge,thuge);
thuge hmod(thuge,thuge);

#endif
