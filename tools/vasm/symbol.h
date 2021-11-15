/* symbol.h - manage all kinds of symbols */
/* (c) in 2014-2020 by Volker Barthelmann and Frank Wille */

#ifndef SYMBOL_H
#define SYMBOL_H

/* symbol types */
#define LABSYM 1
#define IMPORT 2
#define EXPRESSION 3

/* symbol flags */
#define TYPE_MASK     7
#define TYPE_UNKNOWN  0
#define TYPE_OBJECT   1
#define TYPE_FUNCTION 2
#define TYPE_SECTION  3
#define TYPE_FILE     4
#define TYPE_LAST     4
#define TYPE(sym) ((sym)->flags&TYPE_MASK)

#define EXPORT (1<<3)
#define INEVAL (1<<4)
#define COMMON (1<<5)
#define WEAK (1<<6)
#define LOCAL (1<<7)        /* only informational */
#define VASMINTERN (1<<8)
#define PROTECTED (1<<9)
#define REFERENCED (1<<10)  /* referenced by a relocation */
#define ABSLABEL (1<<11)
#define EQUATE (1<<12)
#define REGLIST (1<<13)
#define USED (1<<14)        /* used in any expression */
#define NEAR (1<<15)        /* may refer symbol with near addressing modes */
#define RSRVD_C (1L<<20)    /* bits 20..23 are reserved for cpu modules */
#define RSRVD_S (1L<<24)    /* bits 24..27 are reserved for syntax modules */
#define RSRVD_O (1L<<28)    /* bits 28..31 are reserved for output modules */

struct symbol {
  struct symbol *next;
  int type;
  uint32_t flags;
  char *name;
  expr *expr;
  expr *size;
  section *sec;
  taddr pc;
  taddr align;
  unsigned long idx; /* usable by output module */
};

/* type of symbol references */
#define LOCREF(symp) ((symp)->type==LABSYM&&!((symp)->flags&WEAK))
#define EXTREF(symp) ((symp)->type==IMPORT||((symp)->flags&WEAK))


#ifdef HAVE_REGSYMS
/* register symbols */
struct regsym {
  char *reg_name;
  int reg_type;
  unsigned int reg_flags;
  unsigned int reg_num;
};
#endif /* HAVE_REGSYMS */


extern symbol *first_symbol;

void print_symbol(FILE *,symbol *);
char *get_bind_name(symbol *);
void add_symbol(symbol *);
symbol *find_symbol(char *);
void refer_symbol(symbol *,char *);
void save_symbols(void);
void restore_symbols(void);

int check_symbol(char *);
char *set_last_global_label(char *);
int is_local_label(char *);
char *make_local_label(char *,int,char *,int);
symbol *new_abs(char *,expr *);
symbol *new_equate(char *,expr *);
symbol *new_import(char *);
symbol *new_labsym(section *,char *);
symbol *new_tmplabel(section *);
symbol *internal_abs(char *);
expr *set_internal_abs(char *,taddr);

#ifdef HAVE_REGSYMS
void add_regsym(regsym *);
regsym *find_regsym(char *,int);
regsym *find_regsym_nc(char *,int);
regsym *new_regsym(int,int,char *,int,unsigned int,unsigned int);
int undef_regsym(char *,int,int);
#endif /* HAVE_REGSYMS */

int init_symbol(void);

#endif
