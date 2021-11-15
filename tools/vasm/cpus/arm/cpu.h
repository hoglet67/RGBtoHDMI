/* cpu.h ARM cpu-description header-file */
/* (c) in 2004,2014,2016 by Frank Wille */

#define LITTLEENDIAN (!arm_be_mode)
#define BIGENDIAN (arm_be_mode)
#define VASM_CPU_ARM 1

/* maximum number of operands in one mnemonic */
#define MAX_OPERANDS 6

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 2
/* but no qualifiers for macros */
#define NO_MACRO_QUALIFIERS

/* valid parentheses for cpu's operands */
#define START_PARENTH(x) ((x)=='(' || (x)=='{')
#define END_PARENTH(x) ((x)==')' || (x)=='}')

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* minimum instruction alignment */
#define INST_ALIGN 0  /* Handled internally! */

/* default alignment for n-bit data */
#define DATA_ALIGN(n) ((n)<=8 ? 1 : ((n)<=16 ? 2 : 4))

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) (n==64 ? DATA64_OP : DATA_OP)

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) cpu_available(i)


/* type to store each operand */
typedef struct {
  uint16_t type;              /* type of operand from mnemonic.operand_type */
  uint16_t flags;             /* see below */
  expr *value;                /* single register, immed. val. or branch loc.*/
} operand;

/* flags: */
#define OFL_SHIFTOP     (0x0003)  /* mask for shift-operation */
#define OFL_IMMEDSHIFT  (0x0004)  /* uses immediate shift value */
#define OFL_WBACK       (0x0008)  /* set write-back flag in opcode */
#define OFL_UP          (0x0010)  /* set up-flag, add offset to base */
#define OFL_SPSR        (0x0020)  /* 1:SPSR, 0:CPSR */
#define OFL_FORCE       (0x0040)  /* LDM/STM PSR & force user bit */


/* operand types - WARNING: the order is important! See defines below. */
enum {
  /* ARM operands */
  NOOP=0,
  DATA_OP,    /* data operand */
  DATA64_OP,  /* 64-bit data operand (greater than taddr) */
  BRA24,      /* 24-bit branch offset to label */
  PCL12,      /* 12-bit PC-relative offset with up/down-flag to label */
  PCLCP,      /* 8-bit * 4 PC-relative offset with up/down-flag to label */
  PCLRT,      /* 8-bit rotated PC-relative offset to label */
  CPOP4,      /* 4-bit coprocessor operation code at 23..20 */
  CPOP3,      /* 3-bit coprocessor operation code at 23..21 */
  CPTYP,      /* 3-bit coprocessor operation type at 7..5 */
  SWI24,      /* 24-bit immediate at 23..0 (SWI instruction) */
  IROTV,      /* explicit 4-bit rotate value at 11..8 */
  REG03,      /* Rn at 3..0 */
  REG11,      /* Rn at 11..8 */
  REG15,      /* Rn at 15..12 */
  REG19,      /* Rn at 19..16 */
  R19WB,      /* Rn at 19..16 with optional write-back '!' */
  R19PR,      /* [Rn, pre-indexed at 19..16 */
  R19PO,      /* [Rn], post-indexed or indir. without index, at 19..16 */
  R3UD1,      /* +/-Rn], pre-indexed at 3..0 with optional write-back '!' */
  R3UD2,      /* +/-Rn, at 3..0, pre- or post-indexed */
  IMUD1,      /* #+/-Imm12] pre-indexed with ']' and optional w-back '!' */
  IMUD2,      /* #+/-Imm12 post-indexed */
  IMCP1,      /* #+/-Imm10>>2 pre-indexed with ']' and optional w-back '!' */
  IMCP2,      /* #+/-Imm10>>2 post-indexed */
  IMMD8,      /* #Immediate, 8-bit */
  IMROT,      /* #Imm32, 8-bit auto-rotated */
  SHIFT,      /* <shift-op> Rs | <shift-op> #Imm5 | RRX = ROR #0 */
  SHIM1,      /* <shift-op> #Imm5 | RRX, pre-indexed with terminating ] or ]! */
  SHIM2,      /* <shift-op> #Imm5 | RRX, post-indexed */
  CSPSR,      /* CPSR or SPSR */
  PSR_F,      /* PSR-field: SPSR_<field>, CPSR_<field> */
  RLIST,      /* register list */

  /* THUMB operands */
  TRG02,      /* Rn at 2..0 */
  TRG05,      /* Rn at 5..3 */
  TRG08,      /* Rn at 8..6 */
  TRG10,      /* Rn at 10..8 */
  THR02,      /* Hi-Rn at 2..0 */
  THR05,      /* Hi-Rn at 5..3 */
  TR5IN,      /* [Rn at 5..3 */
  TR8IN,      /* Rn] at 8..6 */
  TR10W,      /* Rn! at 10..8 with write-back '!' */
  TPCRG,      /* "PC" */
  TSPRG,      /* "SP" */
  TPCPR,      /* "[PC" */
  TSPPR,      /* "[SP" */
  TRLST,      /* register list for r0-r7 */
  TRLLR,      /* extended register list, includes LR */
  TRLPC,      /* extended register list, includes PC */
  TUIM3,      /* 3-bit unsigned immediate at 8..6 */
  TUIM5,      /* 5-bit unsigned immediate at 10..6 */
  TUIM8,      /* 8-bit unsigned immediate at 7..0 */
  TUIM9,      /* 9-bit unsigned immediate >> 2 at 6..0 */
  TUIMA,      /* 10-bit unsigned immediate >> 2 at 7..0 */
  TUI5I,      /* 5-bit unsigned immediate at 10..6 with terminating ] */
  TUI6I,      /* 6-bit unsigned immediate >> 1 at 10..6 with terminating ] */
  TUI7I,      /* 7-bit unsigned immediate >> 2 at 10..6 with terminating ] */
  TUIAI,      /* 10-bit unsigned immediate >> 2 at 7..0 with terminating ] */
  TSWI8,      /* 8-bit immediate at 7..0 (SWI instruction) */
  TPCLW,      /* PC-relative label, has to fit into 10-bit uns.imm. >> 2 */
  TBR08,      /* 9-bit branch offset >> 1 to label at 7..0 */
  TBR11,      /* 12-bit branch offset >> 1 to label at 10..0 */
  TBRHL       /* 23-bit branch offset >> 1 splitted into two 11-bit instr. */
};

#define ARMOPER(x)    ((x)>=BRA24 && (x)<=RLIST)
#define STDOPER(x)    ((x)>=DATA_OP && (x)<=IMROT)
#define CPOPCODE(x)   ((x)>=CPOP4 && (x)<=CPTYP)
#define REGOPER(x)    ((x)>=REG03 && (x)<=R3UD2)
#define REG19OPER(x)  ((x)>=REG19 && (x)<=R19PO)
#define REG15OPER(x)  ((x)==REG15)
#define REG11OPER(x)  ((x)==REG11)
#define REG03OPER(x)  ((x)==REG03 || (x)==R3UD1 || (x)==R3UD2)
#define UPDOWNOPER(x) ((x)>=R3UD1 && (x)<=IMCP2)
#define IMMEDOPER(x)  ((x)>=IMUD1 && (x)<=IMROT)
#define SHIFTOPER(x)  ((x)>=SHIFT && (x)<=SHIM2)

#define THUMBOPER(x)  ((x)==0 || (x)>=TRG02)
#define THREGOPER(x)  ((x)>=TRG02 && (x)<=TSPPR)
#define THPCORSP(x)   ((x)>=TPCRG && (x)<=TSPPR)
#define THREGLIST(x)  ((x)>=TRLST && (x)<=TRLPC)
#define THIMMOPER(x)  ((x)>=TUIM3 && (x)<=TUIAI)
#define THIMMINDIR(x) ((x)>=TUI5I && (x)<=TUIAI)
#define THBRANCH(x)   ((x)>=TBR08 && (x)<=TBRHL)


/* additional mnemonic data */
typedef struct {
  uint32_t opcode;
  uint32_t available;
  uint32_t flags;
} mnemonic_extension;

/* flags: */
#define DIFR19    (0x00000001)  /* DIFFRxx registers must be different */
#define DIFR15    (0x00000002)
#define DIFR11    (0x00000004)
#define DIFR03    (0x00000008)
#define NOPC      (0x00000010)  /* R15 is not allowed as source or dest. */
#define NOPCR03   (0x00000020)  /* R15 is not allowed for Rm (3..0) */
#define NOPCWB    (0x00000040)  /* R15 is not allowed in Write-Back mode */
#define SETCC     (0x00000100)  /* instruction supports S-bit */
#define SETPSR    (0x00000200)  /* instruction supports P-bit */
#define THUMB     (0x10000000)  /* THUMB instruction */


/* register symbols */
#define HAVE_REGSYMS
#define REGSYMHTSIZE 256


/* cpu types for availability check */
#define ARM2          (1L<<0)
#define ARM250        (1L<<1)
#define ARM3          (1L<<2)
#define ARM6          (1L<<3)
#define ARM60         (1L<<4)
#define ARM600        (1L<<5)
#define ARM610        (1L<<6)
#define ARM7          (1L<<7)
#define ARM710        (1L<<8)
#define ARM7500       (1L<<9)
#define ARM7d         (1L<<10)
#define ARM7di        (1L<<11)
#define ARM7dm        (1L<<12)
#define ARM7dmi       (1L<<13)
#define ARM7tdmi      (1L<<14)
#define ARM8          (1L<<15)
#define ARM810        (1L<<16)
#define ARM9          (1L<<17)
#define ARM920        (1L<<18)
#define ARM920t       (1L<<19)
#define ARM9tdmi      (1L<<20)
#define SA1           (1L<<21)
#define STRONGARM     (1L<<22)
#define STRONGARM110  (1L<<23)
#define STRONGARM1100 (1L<<24)

/* ARM architectures */
#define AA2   (ARM2|ARM250|ARM3)
#define AA3   (ARM6|ARM60|ARM600|ARM610|ARM7|ARM710|ARM7500|ARM7d|ARM7di)
#define AA3M  (ARM7dm|ARM7dmi)
#define AA4   (ARM8|ARM810|ARM9|ARM920|SA1|STRONGARM|STRONGARM110|STRONGARM1100)
#define AA4T  (ARM7tdmi|ARM920t|ARM9tdmi)

#define AA4TUP (AA4T)
#define AA4UP  (AA4|AA4T)
#define AA3MUP (AA3M|AA4|AA4T)
#define AA3UP  (AA3|AA3M|AA4|AA4T)
#define AA2UP  (AA2|AA3|AA3M|AA4|AA4T)
#define AAANY  (~0)


/* exported by cpu.c */
extern int arm_be_mode;

int cpu_available(int);
