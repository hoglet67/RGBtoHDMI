/* output_test.c example output driver for vasm */
/* (c) in 2002 by Volker Barthelmann */

#include "vasm.h"

static char *copyright="vasm test output module 1.0 (c) 2002 Volker Barthelmann";

static void write_output(FILE *f,section *sec,symbol *sym)
{
  fprintf(f,"Sections:\n");
  for(;sec;sec=sec->next)
    print_section(f,sec);

  fprintf(f,"Symbols:\n");
  for(;sym;sym=sym->next){
    print_symbol(f,sym);
    fprintf(f,"\n");
  }
}

static int output_args(char *p)
{
  /* no args */
  return 0;
}

int init_output_test(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  *cp=copyright;
  *wo=write_output;
  *oa=output_args;
  asciiout=1;
  return 1;
}

