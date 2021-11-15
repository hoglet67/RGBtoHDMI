/* vasm.h  main header file for vasm */
/* (c) in 2002-2021 by Volker Barthelmann */

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

typedef struct symbol symbol;
typedef struct section section;
typedef struct dblock dblock;
typedef struct sblock sblock;
typedef struct expr expr;
typedef struct macro macro;
typedef struct source source;
typedef struct listing listing;
typedef struct regsym regsym;

#define MAXPADBYTES 8  /* max. pattern size to pad alignments */

#include "cpu.h"
#include "symbol.h"
#include "reloc.h"
#include "syntax.h"
#include "symtab.h"
#include "expr.h"
#include "parse.h"
#include "source.h"
#include "atom.h"
#include "cond.h"
#include "listing.h"
#include "supp.h"

#if defined(BIGENDIAN)&&!defined(LITTLEENDIAN)
#define LITTLEENDIAN (!BIGENDIAN)
#endif

#if !defined(BIGENDIAN)&&defined(LITTLEENDIAN)
#define BIGENDIAN (!LITTLEENDIAN)
#endif

#ifndef MNEMONIC_VALID
#define MNEMONIC_VALID(i) 1
#endif

#ifndef OPERAND_OPTIONAL
#define OPERAND_OPTIONAL(p,t) 0
#endif

#ifndef IGNORE_FIRST_EXTRA_OP
#define IGNORE_FIRST_EXTRA_OP 0
#endif

#ifndef START_PARENTH
#define START_PARENTH(x) ((x)=='(')
#endif

#ifndef END_PARENTH
#define END_PARENTH(x) ((x)==')')
#endif

#ifndef CHKIDEND
#define CHKIDEND(s,e) (e)
#endif

#define MAXPATHLEN 1024

/* operations on bit-vectors */
typedef unsigned int bvtype;
#define BVBITS (sizeof(bvtype)*CHAR_BIT)
#define BVSIZE(x) ((((x)+BVBITS-1)/BVBITS)*sizeof(bvtype))
#define BSET(array,bit) (array)[(bit)/BVBITS]|=1<<((bit)%BVBITS)
#define BCLR(array,bit) (array)[(bit)/BVBITS]&=~(1<<((bit)%BVBITS))
#define BTST(array,bit) ((array)[(bit)/BVBITS]&(1<<((bit)%BVBITS)))

/* section flags */
#define HAS_SYMBOLS      (1<<0)
#define RESOLVE_WARN     (1<<1)
#define UNALLOCATED      (1<<2)
#define LABELS_ARE_LOCAL (1<<3)
#define ABSOLUTE         (1<<4)
#define PREVABS          (1<<5) /* saved ABSOLUTE-flag during RORG-block */
#define IN_RORG          (1<<6)       
#define NEAR_ADDRESSING  (1<<7)
#define SECRSRVD       (1L<<24) /* bits 24-31 are reserved for output modules */

/* section description */
struct section {
  struct section *next;
  bvtype *deps;
  char *name;
  char *attr;
  atom *first;
  atom *last;
  taddr align;
  uint8_t pad[MAXPADBYTES];
  int padbytes;
  uint32_t flags;
  uint32_t memattr;  /* type of memory, used by some object formats */
  taddr org;
  taddr pc;
  unsigned long idx; /* usable by output module */
};

/* mnemonic description */
typedef struct mnemonic {
  char *name;
#if MAX_OPERANDS!=0
  int operand_type[MAX_OPERANDS];
#endif
  mnemonic_extension ext;
} mnemonic;

/* operand size flags (ORed with size in bits) */
#define OPSZ_BITS(x)	((x) & 0xff)
#define OPSZ_FLOAT      0x100  /* operand stored as floating point */
#define OPSZ_SWAP	0x200  /* operand stored with swapped bytes */


extern int done,final_pass,nostdout,asciiout;
extern int warn_unalloc_ini_dat;
extern int mnemonic_cnt;
extern int nocase,no_symbols,pic_check,exec_out,chklabels;
extern int secname_attr,unnamed_sections;
extern taddr inst_alignment;
extern hashtable *mnemohash;
extern source *cur_src;
extern section *current_section;
extern char *filename;
extern char *debug_filename;  /* usually an absolute C source file name */
extern char *inname,*outname;
extern char *output_format;
extern char emptystr[];
extern char vasmsym_name[];
extern int num_secs;
extern unsigned space_init;

extern unsigned long long taddrmask;
#define ULLTADDR(x) (((unsigned long long)x)&taddrmask)
extern taddr taddrmin,taddrmax;

/* provided by main assembler module */
extern int debug;

void leave(void);
void set_default_output_format(char *);
void set_section(section *);
section *new_section(char *,char *,int);
section *new_org(taddr);
section *find_section(char *,char *);
void switch_section(char *,char *);
void switch_offset_section(char *,taddr);
void add_align(section *,taddr,expr *,int,unsigned char *);
section *default_section(void);
void push_section(void);
section *pop_section(void);
#if NOT_NEEDED
section *restore_section(void);
section *restore_org(void);
#endif
int end_rorg(void);
void try_end_rorg(void);
void start_rorg(taddr);
void print_section(FILE *,section *);

#define setfilename(x) filename=(x)
#define getfilename() filename
#define setdebugname(x) debug_filename=(x)
#define getdebugname() debug_filename

/* provided by error.c */
extern int errors,warnings;
extern int max_errors;
extern int no_warn;

void general_error(int,...);
void syntax_error(int,...);
void cpu_error(int,...);
void output_error(int,...);
void output_atom_error(int,atom *,...);
void modify_gen_err(int,...);
void modify_syntax_err(int,...);
void modify_cpu_err(int,...);
void disable_message(int);
void disable_warning(int);

#define ierror(x) general_error(4,(x),__LINE__,__FILE__)

/* provided by cpu.c */
extern int bitsperbyte;
extern int bytespertaddr;
extern mnemonic mnemonics[];
extern char *cpu_copyright;
extern char *cpuname;
extern int debug;

int init_cpu();
int cpu_args(char *);
char *parse_cpu_special(char *);
operand *new_operand();
int parse_operand(char *text,int len,operand *out,int requires);
size_t instruction_size(instruction *,section *,taddr);
dblock *eval_instruction(instruction *,section *,taddr);
dblock *eval_data(operand *,size_t,section *,taddr);
#if HAVE_INSTRUCTION_EXTENSION
void init_instruction_ext(instruction_ext *);
#endif
#if MAX_QUALIFIERS!=0
char *parse_instruction(char *,int *,char **,int *,int *);
int set_default_qualifiers(char **,int *);
#endif
#if HAVE_CPU_OPTS
void cpu_opts_init(section *);
void cpu_opts(void *);
void print_cpu_opts(FILE *,void *);
#endif

/* provided by syntax.c */
extern char *syntax_copyright;
extern char commentchar;
extern int dotdirs;
extern hashtable *dirhash;
extern char *defsectname;
extern char *defsecttype;

int init_syntax();
int syntax_args(char *);
void parse(void);
char *parse_macro_arg(struct macro *,char *,struct namelen *,struct namelen *);
int expand_macro(source *,char **,char *,int);
char *skip(char *);
void eol(char *);
char *const_prefix(char *,int *);
char *const_suffix(char *,char *);
char *get_local_label(char **);

/* provided by output_xxx.c */
#ifdef OUTTOS
extern int tos_hisoft_dri;
#endif
#ifdef OUTHUNK
extern int hunk_onlyglobal;
#endif

int init_output_test(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_elf(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_bin(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_srec(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_vobj(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_hunk(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_aout(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_tos(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_xfile(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_cdef(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_ihex(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
int init_output_o65(char **,void (**)(FILE *,section *,symbol *),int (**)(char *));
