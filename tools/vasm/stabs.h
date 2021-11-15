/* stabs.h a.out stabs */
/* (c) 2015 Frank Wille */

#ifndef STABS_H
#define STABS_H

/* stab definition node, created by .stab* directives */
struct stabdef {
  struct stabdef *next;
  union {
    char *ptr;
    int32_t idx;
  } name;
  int type;
  int other;
  int desc;
  symbol *base;
  taddr value;
};

extern struct stabdef *first_nlist,*last_nlist;


/* Symbol table entry format */
struct nlist32 {
  int32_t n_strx;           /* string table offset */
  uint8_t n_type;           /* type defines */
  int8_t n_other;           /* spare */
  int16_t n_desc;           /* used by stab entries */
  uint32_t n_value;         /* address/value of the symbol */
};

#define N_EXT   0x01        /* external (global) bit, OR'ed in */
#define N_TYPE  0x1e        /* mask for all the type bits */
#define N_STAB  0x0e0       /* mask for debugger symbols */

/* symbol types */
#define N_UNDF  0x00        /* undefined */
#define N_ABS   0x02        /* absolute address */
#define N_TEXT  0x04        /* text segment */
#define N_DATA  0x06        /* data segment */
#define N_BSS   0x08        /* bss segment */
#define N_INDR  0x0a        /* alias definition */
#define N_SIZE  0x0c        /* pseudo type, defines a symbol's size */
#define N_WEAKU 0x0d        /* GNU: Weak undefined symbol */
#define N_WEAKA 0x0e        /* GNU: Weak absolute symbol */
#define N_WEAKT 0x0f        /* GNU: Weak text symbol */
#define N_WEAKD 0x10        /* GNU: Weak data symbol */
#define N_WEAKB 0x11        /* GNU: Weak bss symbol */
#define N_COMM  0x12        /* common reference */
#define N_SETA  0x14        /* absolute set element symbol */
#define N_SETT  0x16        /* text set element symbol */
#define N_SETD  0x18        /* data set element symbol */
#define N_SETB  0x1a        /* bss set element symbol */
#define N_SETV  0x1c        /* set vector symbol */
#define N_FN    0x1e        /* file name (N_EXT on) */
#define N_WARN  0x1e        /* warning message (N_EXT off) */

/* debugging symbols */
#define N_GSYM          0x20    /* global symbol */
#define N_FNAME         0x22    /* F77 function name */
#define N_FUN           0x24    /* procedure name */
#define N_STSYM         0x26    /* data segment variable */
#define N_LCSYM         0x28    /* bss segment variable */
#define N_MAIN          0x2a    /* main function name */
#define N_PC            0x30    /* global Pascal symbol */
#define N_RSYM          0x40    /* register variable */
#define N_SLINE         0x44    /* text segment line number */
#define N_DSLINE        0x46    /* data segment line number */
#define N_BSLINE        0x48    /* bss segment line number */
#define N_SSYM          0x60    /* structure/union element */
#define N_SO            0x64    /* main source file name */
#define N_LSYM          0x80    /* stack variable */
#define N_BINCL         0x82    /* include file beginning */
#define N_SOL           0x84    /* included source file name */
#define N_PSYM          0xa0    /* parameter variable */
#define N_EINCL         0xa2    /* include file end */
#define N_ENTRY         0xa4    /* alternate entry point */
#define N_LBRAC         0xc0    /* left bracket */
#define N_EXCL          0xc2    /* deleted include file */
#define N_RBRAC         0xe0    /* right bracket */
#define N_BCOMM         0xe2    /* begin common */
#define N_ECOMM         0xe4    /* end common */
#define N_ECOML         0xe8    /* end common (local name) */
#define N_LENG          0xfe    /* length of preceding entry */

/* n_other & 0x0f */
#define AUX_UNKNOWN     0
#define AUX_OBJECT      1
#define AUX_FUNC        2
#define AUX_LABEL       3
#define AUX_IGNORE   0xff   /* vlink-specific, used to ignore this symbol */
/* n_other & 0xf0 >> 4 */
#define BIND_LOCAL      0   /* not used? */
#define BIND_GLOBAL     1   /* not used? */
#define BIND_WEAK       2

#endif /* STABS_H */
