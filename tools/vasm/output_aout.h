/* output_aout.h header file for a.out objects */
/* (c) in 2008,2020 by Frank Wille */

#include "stabs.h"

/* a.out header */
struct aout_hdr {
  uint8_t a_midmag[4];
  uint8_t a_text[4];
  uint8_t a_data[4];
  uint8_t a_bss[4];
  uint8_t a_syms[4];
  uint8_t a_entry[4];
  uint8_t a_trsize[4];
  uint8_t a_drsize[4];
};

/* a_magic */
#define OMAGIC 0407    /* old impure format */
#define NMAGIC 0410    /* read-only text */
#define ZMAGIC 0413    /* demand load format */
#define QMAGIC 0314    /* not supported */

/* a_mid - machine id */
#define MID_SUN010      1       /* sun 68010/68020 binary */
#define MID_SUN020      2       /* sun 68020-only binary */
#define MID_PC386       100     /* 386 PC binary. (so quoth BFD) */
#define MID_HP200       200     /* hp200 (68010) BSD binary */
#define MID_I386        134     /* i386 BSD binary */
#define MID_M68K        135     /* m68k BSD binary with 8K page sizes */
#define MID_M68K4K      136     /* m68k BSD binary with 4K page sizes */
#define MID_NS32532     137     /* ns32532 */
#define MID_SPARC       138     /* sparc */
#define MID_PMAX        139     /* pmax */
#define MID_VAX1K       140     /* vax 1K page size binaries */
#define MID_ALPHA       141     /* Alpha BSD binary */
#define MID_MIPS        142     /* big-endian MIPS */
#define MID_ARM6        143     /* ARM6 */
#define MID_SH3         145     /* SH3 */
#define MID_POWERPC     149     /* big-endian PowerPC */
#define MID_VAX         150     /* vax */
#define MID_SPARC64     151     /* LP64 sparc */
#define MID_HP300       300     /* hp300 (68020+68881) BSD binary */
#define MID_HPUX        0x20C   /* hp200/300 HP-UX binary */
#define MID_HPUX800     0x20B   /* hp800 HP-UX binary */

/* a_flags */
#define EX_DYNAMIC      0x20
#define EX_PIC          0x10
#define EX_DPMASK       0x30

/* a_midmag macros */
#define SETMIDMAG(a,mag,mid,flag) setval(1,(a)->a_midmag,4, \
                  ((flag)&0x3f)<<26|((mid)&0x3ff)<<16|((mag)&0xffff))


/* Relocation info structures */
struct relocation_info {
  uint8_t r_address[4];
  uint8_t r_info[4];
};

#define RELB_symbolnum 0            /* ordinal number of add symbol */
#define RELS_symbolnum 24
#define RELB_reloc     24           /* the whole reloc field */
#define RELS_reloc     8

/* standard relocs: M68k, x86, ... */
#define RSTDB_pcrel     24          /* 1 if value should be pc-relative */
#define RSTDS_pcrel     1
#define RSTDB_length    25          /* log base 2 of value's width */
#define RSTDS_length    2
#define RSTDB_extern    27          /* 1 if need to add symbol to value */
#define RSTDS_extern    1
#define RSTDB_baserel   28          /* linkage table relative */
#define RSTDS_baserel   1
#define RSTDB_jmptable  29          /* relocate to jump table */
#define RSTDS_jmptable  1
#define RSTDB_relative  30          /* load address relative */
#define RSTDS_relative  1
#define RSTDB_copy      31          /* run time copy */
#define RSTDS_copy      1


/* vasm specific - used to generate a.out files */

#define STRHTABSIZE 0x10000
#define SYMHTABSIZE 0x10000

struct StrTabNode {
  struct node n;
  struct StrTabNode *hashchain;
  char *str;
  uint32_t offset;
};

struct StrTabList {
  struct list l;
  struct StrTabNode **hashtab;
  uint32_t nextoffset;
};

struct SymbolNode {
  struct node n;
  struct SymbolNode *hashchain;
  char *name;
  struct nlist32 s;
  uint32_t index;
};

struct SymTabList {
  struct list l;
  struct SymbolNode **hashtab;
  uint32_t nextindex;
};

struct RelocNode {
  struct node n;
  struct relocation_info r;
};


#if defined(VASM_CPU_M68K)
/* MID defined in cpus/m68k/cpu.h */
#elif defined(VASM_CPU_PPC)
#define MID MID_POWERPC
#elif defined(VASM_CPU_ARM)
#define MID MID_ARM6
#elif defined(VASM_CPU_X86)
#define MID MID_PC386
#elif defined(VASM_CPU_JAGRISC)
#define MID (0)
#endif
