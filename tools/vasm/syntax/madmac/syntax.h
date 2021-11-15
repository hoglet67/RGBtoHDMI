/* syntax.h  syntax header file for vasm */
/* (c) in 2015,2017 by Frank Wille */

/* macros to recognize identifiers */
#define ISIDSTART(x) ((x)=='.'||(x)=='?'||(x)=='_'||isalpha((unsigned char)(x)))
#define ISIDCHAR(x) ((x)=='$'||(x)=='?'||(x)=='_'||isalnum((unsigned char)(x)))
#define ISBADID(p,l) ((l)==1&&(*(p)=='.'||*(p)=='?'||*(p)=='_'))
#define ISEOL(p) (*(p)=='\0' || *(p)==commentchar)

/* result of a boolean operation */
#define BOOLEAN(x) (x)

/* overwrite macro defaults */
#define MAXMACPARAMS 64
