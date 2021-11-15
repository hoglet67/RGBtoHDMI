/*
 * cpu.h 6809/6309/68HC12 cpu description file
*/

#define BIGENDIAN 1
#define LITTLEENDIAN 0
#define VASM_CPU_6809 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 8

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* valid parentheses for cpu's operands */
#define START_PARENTH(x) ((x)=='(' || (x)=='[')
#define END_PARENTH(x) ((x)==')' || (x)==']')

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* instruction extension */
#define HAVE_INSTRUCTION_EXTENSION 1
typedef struct {
  uint8_t ocidx; /* opcode index STD,DIR,IDX,EXT */
  uint8_t dp;    /* remember current direct page per instruction */
} instruction_ext;

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) DTA

/* operands may be completely empty */
#define ALLOW_EMPTY_OPS 1

/* make sure operands are cleared when parsing a new mnemonic */
#define CLEAR_OPERANDS_ON_MNEMO 1

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) cpu_available(i)

/* allow commas and blanks at the same time to separate instruction operands */
#define OPERSEP_COMMA 1
#define OPERSEP_BLANK 1

/* we define two additional unary operations, '<' and '>' */
int ext_unary_eval(int,taddr,taddr *,int);
int ext_find_base(symbol **,expr *,section *,taddr);
#define LOBYTE (LAST_EXP_TYPE+1)
#define HIBYTE (LAST_EXP_TYPE+2)
#define EXT_UNARY_NAME(s) (*s=='<'||*s=='>')
#define EXT_UNARY_TYPE(s) (*s=='<'?LOBYTE:HIBYTE)
#define EXT_UNARY_EVAL(t,v,r,c) ext_unary_eval(t,v,r,c)
#define EXT_FIND_BASE(b,e,s,p) ext_find_base(b,e,s,p)


/* type to store each operand */
typedef struct {
  int8_t mode;
  uint8_t flags;
  int8_t opreg;                 /* single register or register list */
  int8_t offs_reg;              /* accumulator A, B or D (E, F, W) */
  int8_t preinc,postinc;        /* -R, +R, R-, R+, --R, R++ */
  expr *value;
  symbol *base;
  taddr curval;
} operand;

/* addressing modes */
enum {
  AM_NONE=0,
  AM_IMM8,
  AM_IMM16,
  AM_IMM32,
  AM_ADDR,      /* will become AM_DIR or AM_EXT later */
  AM_DIR,
  AM_EXT,
  AM_NOFFS,     /* warning: xOFFS must be higher than ADDR/DIR/EXT! */
  AM_COFFS,
  AM_ROFFS,
  AM_REL8,      /* warning: do not change order of REL8, REL9, REL16! */
  AM_REL9,
  AM_REL16,
  AM_REGXB,
  AM_TFR,
  AM_BITR,
  AM_BITN,
  AM_DBR
};

/* addressing mode flags */
#define AF_COSIZ        0x03    /* mask for constant offset size */
#define AF_OFF5         0
#define AF_OFF8         1
#define AF_OFF9         2
#define AF_OFF16        3
#define AF_INDIR        (1<<2)
#define AF_PC           (1<<3)
#define AF_PCREL        (1<<4)  /* curval is calculated relative to PC */
#define AF_LO           (1<<5)  /* expression with '<'-prefix */
#define AF_HI           (1<<6)  /* expression with '>'-prefix */


/* additional mnemonic data */
typedef struct {
  uint16_t opcode[4];           /* opcodes for different addr. modes */
  uint16_t flags;
} mnemonic_extension;

#define NA             ~0       /* mnemo-tab: addressing mode not available */

/* opcodes */
#define OCSTD           0       /* inherent, relative or immediate */
#define OCDIR           1       /* direct page addressing mode: $12 */
#define OCIDX           2       /* indexed addressing mode exp,R */
#define OCEXT           3       /* extended addressing mode: $1234 */

/* flags: cpu instructions */
#define M6809           (1<<0)  /* 6809 is default */
#define HD6309          (1<<1)  /* 6309: new registers, additional instr. */
#define HC12            (1<<2)  /* standard 68HC12 instruction set */
/* other flags */
#define MOVE            (1<<15) /* movb, movw instruction */


/* operand types */

/* single operand types per instruction */
#define INH             0       /* -- inherent - no operand */
#define DTA             1       /* any 8/16/32-bit data */
#define DT1             2       /* 8-bit value/mask */
#define RLS		3       /* rr relative 8-bit short branch */
#define RLD             4       /* rr relative 9-bit loop branch */
#define RLL             5       /* qq rr relative 16-bit long branch */
#define TFR             6       /* xb transfer registers */
#define TFM             7       /* xb transfer memory */
#define PPL             8       /* xb push/pull register list */
#define BMR             9       /* bit manipulation register */
#define BIT             10      /* bit number 0-7 */
#define DBR             11      /* dbcc,ibcc,tbcc register */
#define OTMASK          0xf     /* lowest four bits for single modes */

/* flags for TFM and pseudo modes with these flags */
#define TFM_PLUS        (1<<14) /* r+ */
#define TFM_MINUS       (1<<15) /* r- */
#define TMP             (TFM|TFM_PLUS)
#define TMM             (TFM|TFM_MINUS)

/* the remaining operand types may be combined */
#define IM1             (1<<4)  /* ii immediate 8-bit: #$12 */
#define IM2             (1<<5)  /* jj kk immediate 16-bit: #$1234 */
#define IM4             (1<<6)  /* jj kk ll mm immediate 32-bit: #$12345678 */
#define DIR             (1<<7)  /* dd direct page: $12 */
#define IDX0            (1<<8)  /* xb indexed 5-bit or register offset */
#define IDX1            (1<<9)  /* xb ff indexed 8/9-bit */
#define IDX2            (1<<10) /* xb ee ff indexed 16-bit */
#define IIXN            (1<<11) /* xb indir. indexed no offs. or auto-incr. */
#define IIXC            (1<<12) /* xb ee ff indirect indexed 8/16-bit offset */
#define IIXR            (1<<13) /* xb indirect indexed register-offset */
#define EXT             (1<<14) /* hh ll extended: $1234 */

#define IND             (IIXN|IIXC|IIXR)          /* indirect indexed */
#define DIX             (IDX0|IDX1|IDX2)          /* direct indexed */
#define IDX             (DIX|IND)                 /* all indexed */
#define AL1             (IM1|DIR|IDX|EXT)         /* all with 8bit imm. */
#define AL2             (IM2|DIR|IDX|EXT)         /* all with 16bit imm. */
#define AL4             (IM4|DIR|IDX|EXT)         /* all with 32bit imm. */
#define MEM             (DIR|IDX|EXT)             /* memory addr. only */
#define MNI             (DIR|DIX|EXT)             /* non-indirect memory */
#define IXE             (IDX|EXT)                 /* indexed and extended */
#define IX0             (IDX0|IIXN|IIXR)          /* indexed xb only */
#define DI0             IDX0                      /* direct indexed xb only */
#define IMM             (IM1|IM2|IM4)             /* all immeditate */


/* CPU registers */
enum {
  REG_D=0,REG_X=1,REG_Y=2,REG_U=3,
  REG_S=4,REG_PC=5,REG_W=6,REG_V=7,
  REG_A=8,REG_B=9,REG_CC=10,REG_DP=11,
  REG_0=13,REG_E=14,REG_F=15
};

struct CPUReg {
  char name[4];
  int len;
  int16_t value;
  uint32_t avail;
  uint16_t cpu;         /* compares with cpu_type for availability */
};

/* avail: defines in which situations the register is available */
#define R_DCIX0         (1<<0)  /* index reg. without constant offset */
#define R_DCIX5         (1<<1)  /* index reg. with 5-bit constant offset */
#define R_DCIX8         (1<<2)  /* index reg. with 8-bit constant offset */
#define R_DCIX16        (1<<3)  /* index reg. with 16-bit constant offset */
#define R_ICIX0         (1<<4)  /* indir. index reg. without const. offset */
#define R_ICIX8         (1<<5)  /* indir. index reg. with 8-bit const. offs. */
#define R_ICIX16        (1<<6)  /* indir. index reg. with 16-bit const. offs. */
#define R_PI1IX         (1<<7)  /* index reg. with byte pre-incr. */
#define R_PD1IX         (1<<8)  /* index reg. with byte pre-decr. */
#define R_PD2IX         (1<<9)  /* index reg. with word pre-decr. */
#define R_QI1IX         (1<<10) /* index reg. with byte post-incr. */
#define R_QI2IX         (1<<11) /* index reg. with word post-incr. */
#define R_QD1IX         (1<<12) /* index reg. with byte post-decr. */
#define R_IPDIX         (1<<13) /* indirect index reg. with word pre-decr. */
#define R_IQIIX         (1<<14) /* indirect index reg. with word post-incr. */
#define R_DAIX          (1<<15) /* index reg. with direct accum. offset */
#define R_IAIX          (1<<16) /* index reg. with indirect accum. offset */
#define R_DOFF          (1<<17) /* direct offset register */
#define R_IOFF          (1<<18) /* indirect offset register */
#define R_IRP           (1<<19) /* inter-register operations */
#define R_STK           (1<<20) /* stack push/pull register lists */
#define R_BMP           (1<<21) /* bit-manipulation register */
#define R_DBR           (1<<22) /* decr./incr./test-branch register */

/* used to get any register */
#define R_ANY           (0xffffffff)

/* direct index registers with constant offset */
#define R_DCIDX         (R_DCIX0|R_DCIX5|R_DCIX8|R_DCIX16)

/* indirect index registers with constant offset */
#define R_ICIDX         (R_ICIX0|R_ICIX8|R_ICIX16)

/* direct index registers with increment/decrement */
#define R_DPIDX         (R_PI1IX|R_PD1IX|R_PD2IX|R_QI1IX|R_QI2IX|R_QD1IX)

/* indirect index registers with increment/decrement */
#define R_IPIDX         (R_IPDIX|R_IQIIX)

/* any direct/indirect index register */
#define R_DIDX          (R_DCIDX|R_DPIDX|R_DAIX)
#define R_IIDX          (R_ICIDX|R_IPIDX|R_IAIX)

/* Index availability for 6809 X,Y,U,S: */
#define R_CIX09         (R_DCIX0|R_DCIX5|R_DCIX8|R_DCIX16|R_ICIX0|R_ICIX8|R_ICIX16)
#define R_AIX09         (R_DAIX|R_IAIX)
#define R_PIX09         (R_PD1IX|R_PD2IX|R_QI1IX|R_QI2IX|R_IPIDX)
#define R_IDX09         (R_CIX09|R_AIX09|R_PIX09)

/* Index availability for 6309 W: */
#define R_CIXW          (R_DCIX0|R_DCIX16|R_ICIX0|R_ICIX16)
#define R_PIXW          (R_PD2IX|R_QI2IX|R_IPIDX)
#define R_IDXW          (R_CIXW|R_AIX09|R_PIXW)

/* Index availability for 6809 PCR: */
#define R_IDXPCR        (R_DCIX8|R_DCIX16|R_ICIX8|R_ICIX16)

/* Index availability for HC12 X,Y,SP: */
#define R_CIX12         (R_DCIX0|R_DCIX5|R_DCIX8|R_DCIX16|R_ICIX16)
#define R_PIX12         (R_PD1IX|R_PI1IX|R_QD1IX|R_QI1IX)
#define R_IDX12         (R_CIX12|R_AIX09|R_PIX12)

/* Index availability for HC12 PC: */
#define R_IDX12PC       (R_CIX12|R_AIX09)

/* Offset availability for 6809 A,B,D and 6309 E, F, W: */
#define R_OFF           (R_DOFF|R_IOFF)


/* cpu-specific symbol-flags */
#define DPAGESYM (RSRVD_C<<0)   /* symbol will reside in the direct-page */


/* exported by cpu.c */
int cpu_available(int);
