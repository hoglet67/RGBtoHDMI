/* tfloat.h Floating point type and string conversion function. */
/* (c) 2014 Frank Wille */

#if defined(__VBCC__) || (defined(_MSC_VER) && _MSC_VER < 1800) || defined(__CYGWIN__)
typedef double tfloat;
#define strtotfloat(n,e) strtod(n,e)
#else
typedef long double tfloat;
#define strtotfloat(n,e) strtold(n,e)
#endif
