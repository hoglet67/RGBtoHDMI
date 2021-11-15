/* cond.c - conditional assembly support routines */
/* (c) in 2015 by Frank Wille */

#include "vasm.h"

int clev;  /* conditional level */

static char cond[MAXCONDLEV+1];
static char *condsrc[MAXCONDLEV+1];
static int condline[MAXCONDLEV+1];
static int ifnesting;


/* initialize conditional assembly */
void cond_init(void)
{
  cond[0] = 1;
  clev = ifnesting = 0;
}


/* return true, when current level allows assembling */
int cond_state(void)
{
  return cond[clev];
}


/* ensures that all conditional block are closed at the end of the source */
void cond_check(void)
{
  if (clev > 0)
    general_error(66,condsrc[clev],condline[clev]);  /* "endc/endif missing */
}


/* establish a new level of conditional assembly */
void cond_if(char flag)
{
  if (++clev >= MAXCONDLEV)
    general_error(65,clev);  /* nesting depth exceeded */

  cond[clev] = flag;
  condsrc[clev] = cur_src->name;
  condline[clev] = cur_src->line;
}


/* handle skipped if statement */
void cond_skipif(void)
{
  ifnesting++;
}


/* handle else statement after skipped if-branch */
void cond_else(void)
{
  if (ifnesting == 0)
    cond[clev] = 1;
}


/* handle else statement after assembled if-branch */
void cond_skipelse(void)
{
  if (clev > 0)
    cond[clev] = 0;
  else
    general_error(63);  /* else without if */
}


/* handle end-if statement */
void cond_endif(void)
{
  if (ifnesting == 0) {
    if (clev > 0)
      clev--;
    else
      general_error(64);  /* unexpected endif without if */
  }
  else  /* the whole conditional block was ignored */
    ifnesting--;
}
