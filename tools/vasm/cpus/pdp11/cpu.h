/*
** cpu.h PDP-11 cpu-description header-file
** (c) in 2020 by Frank Wille
*/

#define BIGENDIAN 0
#define LITTLEENDIAN 1
#define VASM_CPU_PDP11 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 2

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* minimum instruction alignment */
#define INST_ALIGN 2

/* default alignment for n-bit data */
#define DATA_ALIGN(n) ((n)<=8?1:2)

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) EXP

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) cpu_available(i)

/* parse cpu-specific directives with label */
/*#define PARSE_CPU_LABEL(l,s) parse_cpu_label(l,s)*/


/* operand types */
enum {
  _NO_OP=0,     /* no operand */
  ANY,          /* any addressing mode */
  REG,          /* register R0-R7 */
  EXP           /* numeric expression */
};

/* type to store each operand */
typedef struct {
  uint8_t mode;
  int8_t reg;
  expr *value;
} operand;

/* real addressing modes */
#define MMASK   7       /* mask for real addressing modes */
#define MREG    0       /* Rn : register direct */
#define MREGD   1       /* (Rn) or @Rn : register deferred */
#define MAINC   2       /* (Rn)+ : autoincrement */
#define MAINCD  3       /* @(Rn)+ : autoincrement deferred */
#define MADEC   4       /* -(Rn) : autodecrement */
#define MADECD  5       /* @-(Rn) : autodecrement deferred */
#define MIDX    6       /* d(Rn) : indexed */
#define MIDXD   7       /* @d(Rn) : indexed deferred */
/* pseudo addressing modes */
#define MPC     8       /* PC-based pseudo addressing modes, reg=7 */
#define MIMM    (MPC+2) /* #x : immediate */
#define MABS    (MPC+3) /* @#a : absolute address */
#define MREL    (MPC+6) /* a : pc-relative address */
#define MRELD   (MPC+7) /* @a : pc-relative address deferred */
#define MEXP    0x10    /* x : a numeric expression */
#define MBCCJMP 0x11    /* a : pc-relative address for B!cc .+6, JMP a */

/* evaluated expressions */
struct MyOpVal {
  taddr value;
  symbol *base;
  int btype;
};


/* additional mnemonic data */
typedef struct {
  uint16_t opcode;
  uint8_t format;
  uint8_t flags;
} mnemonic_extension;

/* instruction format */
enum {
  NO,                    /* oooooooooooooooo: no operand */
  DO,                    /* oooommmrrrmmmrrr: double operand */
  RO,                    /* ooooooorrrmmmrrr: register and operand */
  SO,                    /* oooooooooommmrrr: single operand */
  RG,                    /* ooooooooooooorrr: register only */
  BR,                    /* oooooooobbbbbbbb: branches */
  RB,                    /* ooooooorrrbbbbbb: register and branch */
  TP                     /* oooooooocccccccc: traps */
};

/* flags: cpu availability */
#define STD      1       /* standard PDP-11 instruction set */
#define EIS      2       /* extended instruction set */
#define FIS      4       /* floating instruction set */
#define FPP      8       /* floating point processor */
#define CIS     16       /* commercial instruction set */
#define PSW     32       /* processor status word instructions */
#define MSP     64       /* memory space instructions */


/* register symbols */
#define HAVE_REGSYMS
#define REGSYMHTSIZE 64
#define RTYPE_R  0       /* Register R0..R7 */


/* exported by cpu.c */
int cpu_available(int);
/*int parse_cpu_label(char *,char **);*/
