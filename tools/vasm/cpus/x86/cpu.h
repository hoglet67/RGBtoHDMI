/* cpu.h x86 cpu-description header-file */
/* (c) in 2005-2006,2017 by Frank Wille */

#define LITTLEENDIAN 1
#define BIGENDIAN 0
#define VASM_CPU_X86 1
#define MNEMOHTABSIZE 0x8000

/* maximum number of operands in one mnemonic */
#define MAX_OPERANDS 3

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 1

/* data type to represent a target-address */
typedef int64_t taddr;
typedef uint64_t utaddr;

/* we support floating point constants */
#define FLOAT_PARSER 1

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* default alignment mode for .align directive */
#define CPU_DEF_ALIGN 1  /* alignment in bytes */

/* operand class for n-bit data definitions */
int x86_data_operand(int);
#define DATA_OPERAND(n) x86_data_operand(n)

/* make sure operand is cleared upon first entry into parse_operand() */
#define CLEAR_OPERANDS_ON_START 1


/* register symbols */
#define HAVE_REGSYMS
#define REGSYMHTSIZE 64

/* reg_flags: */
#define RegRex	    0x1         /* Extended register.  */
#define RegRex64    0x2         /* Extended 8 bit register.  */


/* type to store each operand */
typedef struct {
  int type;                     /* real type of operand */
  int parsed_type;              /* type recognized by parser */
  unsigned int flags;
  expr *value;                  /* displacement or immediate value */
  regsym *basereg;              /* base-reg. for SIB or single register */
  regsym *indexreg;
  expr *scalefactor;
  regsym *segoverride;
  int log2_scale;               /* 0-3 for scale factor 1,2,4,8 */
} operand;

/* register operand types */
#define Reg8               0x1
#define Reg16              0x2
#define Reg32              0x4
#define Reg64              0x8

/* immediate operand types*/
#define Imm8              0x10
#define Imm8S             0x20
#define Imm16             0x40
#define Imm32             0x80
#define Imm32S           0x100
#define Imm64            0x200
#define Imm1             0x400  /* for 1-bit shift-instructions */

/* memory operand types */
#define Disp8            0x800
#define Disp16          0x1000
#define Disp32          0x2000
#define Disp32S         0x4000
#define Disp64          0x8000
#define BaseIndex      0x10000  /* indirect base-register has optional index */

/* special operand types */
#define ShiftCntReg    0x20000  /* Shift-count register (%cl) */
#define IOPortReg      0x40000  /* I/O port register (%dx) */
#define CtrlReg        0x80000
#define DebugReg      0x100000
#define TestReg       0x200000
#define Acc           0x400000  /* Accumulator (%al or %ax or %eax) */
#define SegReg2       0x800000  /* Segment register with 2 bits */
#define SegReg3      0x1000000  /* Segment register with 3 bits */
#define MMXReg       0x2000000
#define XMMReg       0x4000000
#define FloatReg     0x8000000  /* Float register %st(n) */
#define FloatAcc    0x10000000  /* Float stack top %st(0), float-accumulator */
#define EsSeg       0x20000000  /* String instr. oper. with fixed ES seg. */
#define JmpAbs      0x40000000  /* Absolute jump/call instruction */
#define InvMem      0x80000000  /* modrm-byte doesn't support memory form */

/* data operands (@@@ define their own flags?) */
#define FloatData   (FloatAcc)
#define Data8       (Disp8)
#define Data16      (Disp16)
#define Data32      (Disp32)
#define Data64      (Disp64)
#define Float32     (Disp32|FloatData)
#define Float64     (Disp64|FloatData)

/* operand type groups */
#define Reg         (Reg8|Reg16|Reg32|Reg64)
#define WordReg     (Reg16|Reg32|Reg64)
#define AnyReg      (Reg|MMXReg|XMMReg|SegReg2|SegReg3|CtrlReg|DebugReg|TestReg)
#define ImpliedReg  (IOPortReg|ShiftCntReg|Acc|FloatAcc)
#define Imm         (Imm8|Imm8S|Imm16|Imm32S|Imm32|Imm64)
#define EncImm      (Imm8|Imm16|Imm32|Imm32S)
#define Disp        (Disp8|Disp16|Disp32|Disp32S|Disp64)
#define AnyMem      (Disp8|Disp16|Disp32|Disp32S|BaseIndex|InvMem)

/* currently we do not differentiate between those memory types, */
/* but the opcode table is prepared for it */
#define LLongMem    AnyMem
#define LongMem     AnyMem
#define ShortMem    AnyMem
#define WordMem     AnyMem
#define ByteMem     AnyMem

/* flags */
#define OPER_REG        1       /* operand is a direct register */
#define OPER_PCREL      2       /* PC-relative operand */


/* x86 opcode prefixes */
#define WAIT_PREFIX     0
#define LOCKREP_PREFIX  1
#define ADDR_PREFIX     2
#define DATA_PREFIX     3
#define SEG_PREFIX      4
#define REX_PREFIX      5       /* must be the last one */
#define MAX_PREFIXES    6       /* maximum number of prefixes */


/* x86 segment register numbers */
#define ESEG_REGNUM     0
#define CSEG_REGNUM     1
#define SSEG_REGNUM     2
#define DSEG_REGNUM     3
#define FSEG_REGNUM     4
#define GSEG_REGNUM     5


/* Mod-R/M byte, which follows the opcode */
typedef struct {
  unsigned char mode;           /* how to interpret regmem & reg */
  unsigned char reg;            /* register operand or extended opcode */
  unsigned char regmem;         /* register or memory operand (addr.mode) */
} modrm_byte;

/* SIB (scale, index, base) byte, which follows modrm_byte in some
   i386 addressing modes */
typedef struct {
  unsigned char scale;          /* scale-factor (1, 2, 4 or 8) */
  unsigned char index;          /* index register */
  unsigned char base;           /* base register */
} sib_byte;

#define EBP_REGNUM 5
#define ESP_REGNUM 4

/* modrm_byte.regmem for 2-byte opcode escape */
#define TWO_BYTE_ESCAPE     (ESP_REGNUM)
/* sib_byte.index for no index register */
#define NO_INDEXREG         (ESP_REGNUM)
/* sib_byte.base for no base register */
#define NO_BASEREG          (EBP_REGNUM)
#define NO_BASEREG16        6


/* instruction extension */
#define HAVE_INSTRUCTION_EXTENSION 1

typedef struct {
  uint32_t base_opcode;
  unsigned char flags;
  unsigned char num_prefixes;
  unsigned char prefix[MAX_PREFIXES];
  modrm_byte rm;
  sib_byte sib;
  short last_size;
} instruction_ext;

/* flags: */
#define HAS_REG_OPER      0x1   /* inst. has direct-register operands */
#define SUFFIX_CHECKED    0x2   /* suffix assigned and checked */
#define MODRM_BYTE        0x4   /* needs mod/rm byte */
#define SIB_BYTE          0x8   /* needs sib byte */
#define NEGOPT            0x40  /* negatively optimized, bytes gained */
#define POSOPT            0x80  /* positively optimized, bytes gained */
#define OPTFAILED         (POSOPT|NEGOPT)  /* no longer try to optimize this */

/* max_opt_tries:
   An instruction optimized more frequent than that is been considered
   'oscillating' (e.g. by effects of alignment-directives) and should
   no longer be optimized! */
#define MAX_OPT_TRIES     10


/* additional mnemonic data */
typedef struct {
  uint32_t base_opcode;
  uint32_t extension_opcode;
  uint32_t opcode_modifier;
  uint32_t available;
} mnemonic_extension;

/* extension_opcode */
#define NO_EXTOPCODE      0xffff

/* cpu available flags: */
#define CPU086            0x1
#define CPU186            0x2
#define CPU286            0x4
#define CPU386            0x8
#define CPU486           0x10
#define CPU586           0x20
#define CPU686           0x40
#define CPUP4            0x80
#define CPUK6           0x100
#define CPUAthlon       0x200
#define CPUSledgehammer 0x400
#define CPUMMX          0x800
#define CPUSSE         0x1000
#define CPUSSE2        0x2000
#define CPU3dnow       0x4000
#define CPUPNI         0x8000
#define CPUPadLock    0x10000
#define CPU64       0x4000000
#define CPUNo64     0x8000000
#define CPUAny (CPU086|CPU186|CPU286|CPU386|CPU486|CPU586|CPU686|CPUP4|CPUSledgehammer|CPUMMX|CPUSSE|CPUSSE2|CPUPNI|CPU3dnow|CPUK6|CPUAthlon|CPUPadLock)

/* opcode_modifier: */
#define W                  0x1  /* operands can be words or dwords */
#define M                  0x4  /* has Modrm byte */
#define FloatR             0x8  /* swapped src/dest for floats: MUST BE 0x8 */
#define ShortForm         0x10  /* reg. enc. in 3 low-order bits of opcode */
#define FloatMF           0x20  /* FP insn memory format bit, sized by 0x4 */
#define Jmp               0x40  /* relative conditional and uncond. branches */
#define JmpDword          0x80  /* relative 16/32 bit calls */
#define JmpByte          0x100  /* loop and jecxz */
#define JmpInterSeg      0x200  /* inter segment calls and branches */
#define FloatD           0x400  /* direction for FP insns:  MUST BE 0x400 */
#define Seg2ShortForm    0x800  /* 2-bit segment reg. encoded in opcode */
#define Seg3ShortForm   0x1000  /* 3-bit segment reg. encoded in opcode  */
#define Size16          0x2000  /* needs a size prefix in 32-bit mode */
#define Size32          0x4000  /* needs a size prefix in 16-bit mode */
#define Size64          0x8000  /* needs a size prefix in 16-bit mode */
#define IgnoreSize     0x10000  /* operand size prefix is ignored */
#define DefaultSize    0x20000  /* default instruction size depends on mode */
#define b_Illegal      0x40000  /* b suffix is illegal */
#define w_Illegal      0x80000  /* w suffix is illegal */
#define l_Illegal     0x100000  /* l suffix is illegal */
#define s_Illegal     0x200000  /* s suffix is illegal */
#define q_Illegal     0x400000  /* q suffix is illegal */
#define x_Illegal     0x800000  /* x suffix is illegal */
#define FWait        0x1000000  /* needs an FWAIT prefix */
#define StrInst      0x2000000  /* string instructions */
#define FakeLastReg  0x4000000  /* duplicate last reg. for clr and imul */
#define IsPrefix     0x8000000  /* opcode is a prefix */
#define ImmExt      0x10000000  /* has extension in 8 bit immediate */
#define NoRex64     0x20000000  /* doesn't require REX64 prefix */
#define Rex64       0x40000000  /* requires REX64 prefix */
#define Deprecated  0x80000000  /* deprecated FPU instruction */
