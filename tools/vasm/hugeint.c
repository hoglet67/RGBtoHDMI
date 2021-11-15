/* hugeint.c implements huge integer operations at 128 bits */
/* (c) in 2014 by Frank Wille */

#include <stdint.h>
#include <stddef.h>
#include "vasm.h"

#define HALF_BITS 32
#define HIHALF(x) ((uint64_t)(x) >> HALF_BITS)
#define LOHALF(x) ((uint64_t)(x) & ((1LL << HALF_BITS) - 1))
#define BASE (1LL << HALF_BITS)
#define COMBINE(a,b) (((uint64_t)(a) << HALF_BITS) | (b))

typedef uint32_t digit;


thuge huge_zero(void)
{
  thuge r;

  r.hi = r.lo = 0;
  return r;
}


thuge huge_from_int(int64_t i)
{
  thuge r;

  r.hi = i<0 ? ~0 : 0;
  r.lo = (uint64_t)i;
  return r;
}


int64_t huge_to_int(thuge h)
{
  return (int64_t)h.lo;
}


#if FLOAT_PARSER
thuge huge_from_float(tfloat f)
{
  thuge r;
  tfloat pf = f<0.0 ? -f : f;

  r.hi = pf / MAX_UINT64_FLOAT;
  r.lo = pf - (tfloat)r.hi * MAX_UINT64_FLOAT;
  if (f < 0.0) {
    r.hi = r.lo!=0 ? (-(int64_t)r.hi)-1 : -(int64_t)r.hi;
    r.lo = -(int64_t)r.lo;
  }
  return r;
}


tfloat huge_to_float(thuge h)
{
  if (HUGESIGN(h))
    return (tfloat)-(int64_t)h.lo - MAX_UINT64_FLOAT *
           (tfloat)(h.lo!=0 ? h.hi-1 : h.hi);
  return (tfloat)h.hi * MAX_UINT64_FLOAT + (tfloat)h.lo;
}
#endif /* FLOAT_PARSER */


thuge huge_from_mem(int be,void *src,size_t size)
{
  uint8_t *s = src;
  thuge r;

  if (size > HUGEBITS/8)
    ierror(0);
  if (be) {
    if ((int8_t)*s < 0)
      r.hi = r.lo = ~0;
    else
      r.hi = r.lo = 0;
    while (size--) {
      r = hshl(r,8);
      r.lo |= *s++;
    }
  }
  else {
    s += size;
    if ((int8_t)*(s-1) < 0)
      r.hi = r.lo = ~0;
    else
      r.hi = r.lo = 0;
    while (size--) {
      r = hshl(r,8);
      r.lo |= *(--s);
    }
  }
  return r;
}


void *huge_to_mem(int be,void *dest,size_t size,thuge h)
{
  uint8_t *d = dest;

  if (size > HUGEBITS/8)
    ierror(0);
  if (be) {
    d += size;
    dest = d;
    while (size--) {
      *(--d) = (uint8_t)h.lo;
      h = hshr(h,8);
    }
  }
  else {
    while (size--) {
      *d++ = (uint8_t)h.lo;
      h = hshr(h,8);
    }
    dest = d;
  }
  return dest;
}


/* check if huge-int can be represented by bits, signed or unsigned */
int huge_chkrange(thuge h,int bits)
{
  uint64_t v,mask;

  if (bits & 7)
    ierror(0);

  if (bits >= HUGEBITS)
    return 1;

  if (bits >= HUGEBITS/2) {
    mask = ~0LL << (bits - HUGEBITS/2);
    v = h.hi & mask;
    return (v & (1LL << (bits - HUGEBITS/2))) ? (v ^ mask) == 0 : v == 0;
  }    

  mask = ~0LL << bits;
  v = h.lo & mask;
  if (v & (1LL << bits))
    return h.hi == ~0 && (v ^ mask) == 0;
  return h.hi == 0 && v == 0;
}


thuge hneg(thuge a)
{
  thuge r;

  r.hi = a.lo!=0 ? (-(int64_t)a.hi)-1 : -(int64_t)a.hi;
  r.lo = -(int64_t)a.lo;
  return r;
}


thuge hcpl(thuge a)
{
  thuge r;

  r.hi = ~a.hi;
  r.lo = ~a.lo;
  return r;
}


thuge hnot(thuge a)
{
  thuge r;

  r.hi = 0;
  r.lo = (a.hi == 0 && a.lo == 0) ? 1 : 0;
  return r;
}


thuge hand(thuge a,thuge b)
{
  thuge r;

  r.hi = a.hi & b.hi;
  r.lo = a.lo & b.lo;
  return r;
}


thuge hor(thuge a,thuge b)
{
  thuge r;

  r.hi = a.hi | b.hi;
  r.lo = a.lo | b.lo;
  return r;
}


thuge hxor(thuge a,thuge b)
{
  thuge r;

  r.hi = a.hi ^ b.hi;
  r.lo = a.lo ^ b.lo;
  return r;
}


thuge haddi(thuge a,int64_t b)
{
  thuge r;

  r.lo = a.lo + (uint64_t)b;
  if (b < 0)
    r.hi = (r.lo>a.lo) ? a.hi-1 : a.hi;
  else
    r.hi = (r.lo<a.lo) ? a.hi+1 : a.hi;
  return r;
}


thuge hadd(thuge a,thuge b)
{
  thuge r;

  r.lo = a.lo + b.lo;
  if (HUGESIGN(b))
    r.hi = (r.lo>a.lo) ? a.hi + b.hi - 1 : a.hi + b.hi;
  else
    r.hi = (r.lo<a.lo) ? a.hi + b.hi + 1 : a.hi + b.hi;
  return r;
}


thuge hsub(thuge a,thuge b)
{
  thuge r;

  r.lo = a.lo - b.lo;
  if (HUGESIGN(b))
    r.hi = (r.lo<a.lo) ? a.hi - b.hi + 1 : a.hi - b.hi;
  else
    r.hi = (r.lo>a.lo) ? a.hi - b.hi - 1 : a.hi - b.hi;
  return r;
}


int hcmp(thuge a,thuge b)
{
  thuge c = hsub(a,b);

  if (c.hi == 0 && c.lo == 0)
    return 0;
  return HUGESIGN(c) ? -1 : 1;
}


thuge hshra(thuge a,int b)
{
  thuge r;

  if (b >= HUGEBITS/2) {
    r.hi = HUGESIGN(a) ? ~0 : 0;
    r.lo = (int64_t)a.hi >> (b - HUGEBITS/2);
  }
  else if (b == 0) {
    r.hi = a.hi;
    r.lo = a.lo;
  }
  else {
    r.hi = (int64_t)a.hi >> b;
    r.lo = (a.hi << (HUGEBITS/2 - b)) | (a.lo >> b);
  }
  return r;
}


thuge hshr(thuge a,int b)
{
  thuge r;

  if (b >= HUGEBITS/2) {
    r.hi = 0;
    r.lo = a.hi >> (b - HUGEBITS/2);
  }
  else if (b == 0) {
    r.hi = a.hi;
    r.lo = a.lo;
  }
  else {
    r.hi = a.hi >> b;
    r.lo = (a.hi << (HUGEBITS/2 - b)) | (a.lo >> b);
  }
  return r;
}


thuge hshl(thuge a,int b)
{
  thuge r;

  if (b >= HUGEBITS/2) {
    r.hi = a.lo << (b - HUGEBITS/2);
    r.lo = 0;
  }
  else if (b == 0) {
    r.hi = a.hi;
    r.lo = a.lo;
  }
  else {
    r.hi = (a.lo >> (HUGEBITS/2 - b)) | (a.hi << b);
    r.lo = a.lo << b;
  }
  return r;
}


thuge hmuli(thuge a,int64_t b)
{
  uint64_t tmp,carry,ub;
  thuge r;

  ub = (uint64_t)b;
  r.lo = a.lo * ub;
  r.hi = a.hi * ub;
  tmp = HIHALF(a.lo) * LOHALF(ub);
  carry = tmp + LOHALF(a.lo) * HIHALF(ub);
  if (carry < tmp)
    r.hi += BASE;
  r.hi += HIHALF(carry);
  tmp = HIHALF(a.lo) * HIHALF(ub);
  r.hi += tmp;
  if (tmp + (carry << HALF_BITS) < tmp)
    r.hi++;
  return r;
}


thuge hmul(thuge a,thuge b)
{
  uint64_t tmp,carry;
  thuge r;

  r.lo = a.lo * b.lo;
  r.hi = a.hi * b.lo + b.hi * a.lo;
  tmp = HIHALF(a.lo) * LOHALF(b.lo);
  carry = tmp + LOHALF(a.lo) * HIHALF(b.lo);
  if (carry < tmp)
    r.hi += BASE;
  r.hi += HIHALF(carry);
  tmp = HIHALF(a.lo) * HIHALF(b.lo);
  r.hi += tmp;
  if (tmp + (carry << HALF_BITS) < tmp)
    r.hi++;
  return r;
}


static void shift_digits(digit *p,int len,int sh)
{
  int i;

  for (i=0; i<len; i++) {
    p[i] = (digit)(LOHALF((uint64_t)p[i] << sh) |
                   ((uint64_t)p[i + 1] >> (HALF_BITS - sh)));
  }
  p[i] = (digit)(LOHALF((uint64_t)p[i] << sh));
}


static thuge divmod(thuge a,thuge b,thuge *modptr)
{
  digit uspace[5],vspace[5],qspace[5];
  digit *u=uspace,*v=vspace,*q=qspace;
  digit v1,v2;
  uint64_t qhat,rhat,t;
  int m,n,d,j,i;
  thuge r;

  if (b.hi == 0 && b.lo == 0)
    ierror(0);  /* division by zero */

  if (a.hi<b.hi || (a.hi==b.hi && a.lo<b.lo)) {
    if (modptr) {
      modptr->hi = a.hi;
      modptr->lo = a.lo;
    }
    r.hi = r.lo = 0;
    return r;
  }

  u[0] = 0;
  u[1] = (digit)HIHALF(a.hi);
  u[2] = (digit)LOHALF(a.hi);
  u[3] = (digit)HIHALF(a.lo);
  u[4] = (digit)LOHALF(a.lo);
  v[1] = (digit)HIHALF(b.hi);
  v[2] = (digit)LOHALF(b.hi);
  v[3] = (digit)HIHALF(b.lo);
  v[4] = (digit)LOHALF(b.lo);

  for (n=4; v[1]==0; v++) {
    if (--n == 1) {
      uint64_t rbj;
      digit q1,q2,q3,q4;

      t = v[2];
      q1 = (digit)(u[1] / t);
      rbj = COMBINE(u[1] % t,u[2]);
      q2 = (digit)(rbj / t);
      rbj = COMBINE(rbj % t,u[3]);
      q3 = (digit)(rbj / t);
      rbj = COMBINE(rbj % t,u[4]);
      q4 = (digit)(rbj / t);
      if (modptr) {
        modptr->hi = 0;
        modptr->lo = rbj % t;
      }
      r.hi = COMBINE(q1,q2);
      r.lo = COMBINE(q3,q4);
      return r;
    }
  }

  for (m=4-n; u[1]==0; u++)
    m--;
  for (i=4-m; --i>=0;)
    q[i] = 0;
  q += 4 - m;

  d = 0;
  for (t=v[1]; t<BASE/2; t<<=1)
    d++;
  if (d > 0) {
    shift_digits(&u[0],m+n,d);
    shift_digits(&v[1],n-1,d);
  }
  j = 0;
  v1 = v[1];
  v2 = v[2];

  do {
    digit uj0, uj1, uj2;

    uj0 = u[j+0];
    uj1 = u[j+1];
    uj2 = u[j+2];
    if (uj0 == v1) {
      qhat = BASE;
      rhat = uj1;
      goto qhat_too_big;
    }
    else {
      uint64_t nn = COMBINE(uj0,uj1);
      qhat = nn / v1;
      rhat = nn % v1;
    }
    while (v2 * qhat > COMBINE(rhat,uj2)) {
qhat_too_big:
      qhat--;
      if ((rhat += v1) >= BASE)
        break;
    }
    for (t=0,i=n; i>0; i--) {
      t = u[i+j] - v[i] * qhat - t;
      u[i+j] = (digit)LOHALF(t);
      t = (BASE - HIHALF(t)) & (BASE - 1);
    }
    t = u[j] - t;
    u[j] = (digit)LOHALF(t);
    if (HIHALF(t)) {
      qhat--;
      for (t=0, i=n; i>0; i--) {
        t += u[i+j] + v[i];
        u[i+j] = (digit)LOHALF(t);
        t = HIHALF(t);
      }
      u[j] = (digit)LOHALF(u[j] + t);
    }
    q[j] = (digit)qhat;
  } while (++j <= m);

  if (modptr) {
    if (d) {
      for (i=m+n; i>m; --i) {
        u[i] = (digit)(((uint64_t)u[i] >> d) |
                       LOHALF((uint64_t)u[i-1] << (HALF_BITS - d)));
      }
      u[i] = 0;
    }
    modptr->hi = COMBINE(uspace[1],uspace[2]);
    modptr->lo = COMBINE(uspace[3],uspace[4]);
  }

  r.hi = COMBINE(qspace[1],qspace[2]);
  r.lo = COMBINE(qspace[3],qspace[4]);
  return r;
}


thuge hdiv(thuge a,thuge b)
{
  int neg = 0;
  thuge r;

  if (HUGESIGN(a)) {
    neg ^= 1;
    a = hneg(a);
  }
  if (HUGESIGN(b)) {
    neg ^= 1;
    b = hneg(b);
  }
  r = divmod(a,b,NULL);
  return neg ? hneg(r) : r;
}


thuge hmod(thuge a,thuge b)
{
  int neg = 0;
  thuge r;

  if (HUGESIGN(a)) {
    neg ^= 1;
    a = hneg(a);
  }
  if (HUGESIGN(b)) {
    neg ^= 1;
    b = hneg(b);
  }
  (void)divmod(a,b,&r);
  return neg ? hneg(r) : r;
}
