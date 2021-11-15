/* syntax.h  syntax header file for vasm */
/* (c) in 2002,2005,2009-2015,2017
   by Volker Barthelmann and Frank Wille */

/* macros to recognize identifiers */
int isidchar(char);
int iscomment(char *);
#define ISIDSTART(x) ((x)=='.'||(x)=='@'||(x)=='_'||isalpha((unsigned char)(x)))
#define ISIDCHAR(x) isidchar(x)
#define ISBADID(p,l) ((l)==1&&(*(p)=='.'||*(p)=='@'||*(p)=='_'))
#define ISEOL(p) (*(p)=='\0'||iscomment(p))
#ifdef VASM_CPU_M68K
char *chkidend(char *,char *);
#define CHKIDEND(s,e) chkidend((s),(e))
#endif

/* result of a boolean operation */
#define BOOLEAN(x) -(x)

/* we have a special skip() function for expressions, called exp_skip() */
char *exp_skip(char *);
#define EXPSKIP() s=exp_skip(s)

/* ignore operand field, when the instruction has no operands */
#define IGNORE_FIRST_EXTRA_OP 1

/* symbol which contains the number of macro arguments */
#define NARGSYM "NARG"

/* symbol which contains the number of the current macro argument */
#define CARGSYM "CARG"

/* symbol which contains the current rept-endr iteration count */
#define REPTNSYM "REPTN"

/* overwrite macro defaults */
#define MAXMACPARAMS 35
#define SKIP_MACRO_ARGNAME(p) (NULL)
void my_exec_macro(source *);
#define EXEC_MACRO(s) my_exec_macro(s)
