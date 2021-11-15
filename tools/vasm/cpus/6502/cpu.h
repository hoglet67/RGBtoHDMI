/*
** cpu.h 650x/65C02/6510/6280/45gs02 cpu-description header-file
** (c) in 2002,2008,2009,2014,2018,2020,2021 by Frank Wille
*/

#define BIGENDIAN 0
#define LITTLEENDIAN 1
#define VASM_CPU_650X 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 3

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* instruction extension */
#define HAVE_INSTRUCTION_EXTENSION 1
typedef struct {
  utaddr dp;    /* remember current direct page per instruction */
} instruction_ext;

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) DATAOP

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) cpu_available(i)

/* parse cpu-specific directives with label */
#define PARSE_CPU_LABEL(l,s) parse_cpu_label(l,s)

/* we define two additional unary operations, '<' and '>' */
int ext_unary_type(char *);
int ext_unary_eval(int,taddr,taddr *,int);
int ext_find_base(symbol **,expr *,section *,taddr);
#define LOBYTE (LAST_EXP_TYPE+1)
#define HIBYTE (LAST_EXP_TYPE+2)
#define EXT_UNARY_NAME(s) (*s=='<'||*s=='>')
#define EXT_UNARY_TYPE(s) ext_unary_type(s)
#define EXT_UNARY_EVAL(t,v,r,c) ext_unary_eval(t,v,r,c)
#define EXT_FIND_BASE(b,e,s,p) ext_find_base(b,e,s,p)

/* type to store each operand */
typedef struct {
  int type;
  unsigned flags;
  expr *value;
} operand;

/* operand flags */
#define OF_LO (1<<0)
#define OF_HI (1<<1)
#define OF_PC (1<<2)


/* additional mnemonic data */
typedef struct {
  unsigned char opcode;
  unsigned char zp_opcode;  /* !=0 means optimization to zero page allowed */
  uint16_t available;
} mnemonic_extension;

/* available */
#define M6502    1       /* standard 6502 instruction set */
#define ILL      2       /* illegal 6502 instructions */
#define DTV      4       /* C64 DTV instruction set extension */
#define M65C02   8       /* basic 65C02 extensions on 6502 instruction set */
#define WDC02    16      /* basic WDC65C02 extensions on 65C02 instr. set */
#define WDC02ALL 32      /* all WDC65C02 extensions on 65C02 instr. set */
#define CSGCE02  64      /* CSG65CE02 extensions on WDC65C02 instruction set */
#define HU6280   128     /* HuC6280 extensions on WDC65C02 instruction set */
#define M45GS02  256     /* MEGA65 45GS02 extensions on basic WDC02 instr.set */


/* adressing modes */
enum {
  IMPLIED=0,
  DATAOP,       /* data operand */
  ABS,          /* $1234 */
  ABSX,         /* $1234,X */
  ABSY,         /* $1234,Y */
  ABSZ,         /* $1234,Z */
  ZPAGE,        /* $12 - add ZPAGE-ABS to optimize ABS/ABSX/ABSY/ABSZ */
  ZPAGEX,       /* $12,X */
  ZPAGEY,       /* $12,Y */
  ZPAGEZ,       /* $12,Z */
  INDIR,        /* ($1234) - JMP only */
  INDX,         /* ($12,X) */
  INDY,         /* ($12),Y */
  INDZ,         /* ($12),Z */
  INDZ32,       /* [$12],Z */
  IND32,        /* [$12] */
  DPINDIR,      /* ($12) */
  INDIRX,       /* ($1234,X) - JMP only */
  RELJMP,       /* B!cc/JMP construction */
  REL8,         /* $1234 - 8-bit signed relative branch */
  REL16,        /* $1234 - 16-bit signed relative branch */
  IMMED,        /* #$12 */
  WBIT,         /* bit-number (WDC65C02) */
  ACCU,         /* A - all following addressing modes don't need a value! */
  DUMX,         /* dummy X as 'second' operand */
  DUMY,         /* dummy Y as 'second' operand */
  DUMZ,         /* dummy Z as 'second' operand */
};
#define IS_ABS(x) ((x)>=ABS && (x)<=ABSZ)
#define IS_REL
#define FIRST_INDIR INDIR
#define LAST_INDIR INDIRX
/* CAUTION:
   - Do not change the order of ABS,ABSX/Y/Z, and ZPAGE,ZPAGEX/Y/Z
   - All addressing modes >=ACCU (and IMPLIED) do not require a value!
   - All indirect addressing modes are between FIRST_INDIR and LAST_INDIR!
*/

/* cpu-specific symbol-flags */
#define ZPAGESYM (RSRVD_C<<0)   /* symbol will reside in the zero/direct-page */


/* exported by cpu.c */
extern uint16_t cpu_type;
int cpu_available(int);
int parse_cpu_label(char *,char **);
