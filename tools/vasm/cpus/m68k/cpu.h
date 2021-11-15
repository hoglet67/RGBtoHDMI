/*
** cpu.h Motorola M68k, CPU32 and ColdFire cpu-description header-file
** (c) in 2002,2006-2021 by Frank Wille
*/

#define BIGENDIAN 1
#define LITTLEENDIAN 0
#define VASM_CPU_M68K 1
#define MNEMOHTABSIZE 0x4000

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 6

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 1

/* maximum number of additional command-line-flags for this cpu */

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* we support floating point constants */
#define FLOAT_PARSER 1

/* instruction extension */
#define HAVE_INSTRUCTION_EXTENSION 1
typedef struct {
  union {
    struct {
      unsigned char flags;
      signed char last_size;
      signed char orig_ext;
      char unused;
    } real;
    struct {
      struct instruction *next;
    } copy;
  } un;
} instruction_ext;
#define IFL_RETAINLASTSIZE    1   /* retain current last_size value */
#define IFL_UNSIZED           2   /* instruction had no size extension */

/* we use OPTS atoms for cpu-specific options */
#define HAVE_CPU_OPTS 1
typedef struct {
  int cmd;
  int arg;
} optcmd;
/* optcmd commands - warning: print_cpu_opts() depends on the order! */
enum {
  OCMD_NOP,
  OCMD_CPU,
  OCMD_FPU,
  OCMD_SDREG,
  OCMD_NOOPT,
  OCMD_OPTGEN,
  OCMD_OPTMOVEM,
  OCMD_OPTPEA,
  OCMD_OPTCLR,
  OCMD_OPTST,
  OCMD_OPTLSL,
  OCMD_OPTMUL,
  OCMD_OPTDIV,
  OCMD_OPTFCONST,
  OCMD_OPTBRAJMP,
  OCMD_OPTJBRA,
  OCMD_OPTPC,
  OCMD_OPTBRA,
  OCMD_OPTDISP,
  OCMD_OPTABS,
  OCMD_OPTMOVEQ,
  OCMD_OPTNMOVQ,
  OCMD_OPTQUICK,
  OCMD_OPTBRANOP,
  OCMD_OPTBDISP,
  OCMD_OPTODISP,
  OCMD_OPTLEA,
  OCMD_OPTLQUICK,
  OCMD_OPTIMMADDR,
  OCMD_OPTSPEED,
  OCMD_OPTSIZE,
  OCMD_SMALLCODE,
  OCMD_SMALLDATA,
  OCMD_OPTWARN,
  OCMD_CHKPIC,
  OCMD_CHKTYPE,
  OCMD_NOWARN
};

/* minimum instruction alignment */
#define INST_ALIGN 2

/* default alignment for n-bit data */
#define DATA_ALIGN(n) ((n<=8)?1:2)

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) m68k_data_operand(n)

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(n) m68k_available(n)

/* returns true when operand type is optional; may init default operand */
#define OPERAND_OPTIONAL(p,t) m68k_operand_optional(p,t)

/* parse cpu-specific directives with label */
#define PARSE_CPU_LABEL(l,s) parse_cpu_label(l,s)

/* we define one additional, but internal, unary operation, to count 1-bits */
int ext_unary_eval(int,taddr,taddr *,int);
int ext_find_base(symbol **,expr *,section *,taddr);
#define CNTONES (LAST_EXP_TYPE+1)
#define EXT_UNARY_EVAL(t,v,r,c) ext_unary_eval(t,v,r,c)
#define EXT_FIND_BASE(b,e,s,p) BASE_ILLEGAL

/* type to store each operand */
typedef struct {
  signed char mode;
  signed char reg;
  uint16_t format;            /* used for (d8,An/PC,Rn) and ext.addr.modes */
  unsigned char bf_offset;    /* bitfield offset, k-factor or MAC Upper Word */
  unsigned char bf_width;     /* bitfield width or MAC-MASK '&' */
  int8_t basetype[2];         /* BASE_OK=normal, BASE=PCREL=pc-relative base */
  uint32_t flags;
  expr *value[2];             /* immediate, abs. or displacem. expression */
  /* filled during instruction_size(): */
  taddr extval[2];            /* evaluated expression from value[0/1] */
  symbol *base[2];            /* symbol base for value[0/1], NULL otherwise */
} operand;

/* flags */
/* Note: FL_CheckMask bits are used together with optype.flags OTF-bits */
#define FL_ExtVal0          1   /* extval[0] is set */
#define FL_ExtVal1          2   /* extval[1] is set */
#define FL_UsesFormat       4   /* operand uses format word */
#define FL_020up            8   /* 020+ addressing mode */
#define FL_noCPU32       0x10   /* addressing mode not available for CPU32 */
#define FL_BFoffsetDyn   0x20   /* dynamic bitfield offset specified */
#define FL_BFwidthDyn    0x40   /* dynamic bitfield width specified */
#define FL_DoNotEval     0x80   /* do not evaluate, extval and base are ok */
/*#define FL_PossRegList   0x80    parser is not sure if operand is RegList */
#define FL_NoOptBase    0x100   /* never optimize base displacement */
#define FL_NoOptOuter   0x200   /* never optimize outer displacement */
#define FL_NoOpt        0x300   /* never optimize this whole operand */
#define FL_ZBase        0x400   /* ZAn base register specified */
#define FL_ZIndex       0x800   /* ZRn index register specified */
#define FL_BaseReg     0x1000   /* BASEREG expression in exp.value[0] */
#define FL_BnReg       0x4000   /* Apollo: Bn register instead of An */
#define FL_MAC         0x8000   /* ColdFire MAC specific extensions */
#define FL_Bitfield   0x10000   /* operand uses bf_offset/bf_width */
#define FL_DoubleReg  0x20000   /* Dm:Dn or (Rm):(Rn), where both registers
                                   are put into "reg": 0nnn0mmm */
#define FL_KFactor    0x40000   /* k-factor <ea>{#n} or <ea>{Dn}, which
                                   can be found in bf_offset */
#define FL_FPSpec     0x80000   /* special FPU reg. FPIAR/FPCR/FPSR only */

#define FL_CheckMask  0xfc000   /* bits to check, when comparing with
                                   flags from struct optype */

/* addressing modes */
#define MODE_Dn           0
#define MODE_An           1
#define MODE_AnIndir      2
#define MODE_AnPostInc    3
#define MODE_AnPreDec     4
#define MODE_An16Disp     5
#define MODE_An8Format    6   /* uses format word */
#define MODE_Extended     7   /* reg determines addressing mode */
#define MODE_FPn          8   /* FPU register */
#define MODE_SpecReg      9   /* reg determines index into SpecRegs */
/* reg encodings for MODE_Extended: */
#define REG_AbsShort      0
#define REG_AbsLong       1
#define REG_PC16Disp      2
#define REG_PC8Format     3   /* uses format word */
#define REG_Immediate     4
#define REG_RnList        5   /* An/Dn register list in value[0] */
#define REG_FPnList       6   /* FPn register list in value[0] */

/* format word */
#define FW_IndexAn        0x8000
#define FW_IndexReg_Shift 12
#define FW_IndexReg(n)    ((n)<<FW_IndexReg_Shift)
#define FW_LongIndex      0x0800
#define FW_Scale_Shift    9
#define FW_Scale(n)       ((n)<<FW_Scale_Shift)
#define FW_FullFormat     0x0100
#define FW_BaseSuppress   0x0080
#define FW_IndexSuppress  0x0040
#define FW_BDSize_Shift   4
#define FW_BDSize(n)      ((n)<<FW_BDSize_Shift)
#define FW_getBDSize(n)   (((n)>>FW_BDSize_Shift)&3)
#define FW_Postindexed    0x0004
#define FW_IndSize(n)     (n)
#define FW_getIndSize(n)  (n&3)
#define FW_None           0
#define FW_Null           1
#define FW_Word           2
#define FW_Long           3
#define FW_SizeMask       3

/* register macros */
#define REGAn             8
#define REGPC             0x10
#define REGBn             0x20          /* Apollo only */
#define REGZero           0x80
#define REGisAn(n)        ((n)&REGAn)
#define REGisDn(n)        (!((n)&REGAn))
#define REGisPC(n)        ((n)&REGPC)
#define REGisZero(n)      ((n)&REGZero)
#define REGisBn(n)        ((n)&REGBn)   /* Apollo only */
#define REGget(n)         ((n)&(REGAn-1))
#define REGgetA(n)        ((n)&(REGPC-1))
#define REGext_Shift      8
#define REGext(n)         (((n)&0x700)>>REGext_Shift)
#define REGscale_Shift    12
#define REGscale(n)       (((n)&0x3000)>>REGscale_Shift)

/* MAC scale-factor, stored as value[0] */
#define MACSF_None        0
#define MACSF_ShiftLeft   1
#define MACSF_ShiftRight  3

/* special CPU registers */
struct specreg {
  char *name;
  int code;                  /* -1 means no code, syntax-check only */
  uint32_t available;
};


/* extension codes */
#define EXT_BYTE          1
#define EXT_WORD          2
#define EXT_LONG          3
#define EXT_SINGLE        4
#define EXT_DOUBLE        5
#define EXT_EXTENDED      6
#define EXT_PACKED        7
#define EXT_UPPER         2  /* ColdFire MAC upper register word */
#define EXT_LOWER         3  /* ColdFire MAC lower register word */
#define EXT_MASK          7


struct addrmode {
  signed char mode;
  signed char reg;
};

#define AM_Dn 0
#define AM_An 1
#define AM_AnIndir 2
#define AM_AnPostInc 3
#define AM_AnPreDec 4
#define AM_An16Disp 5
#define AM_An8Format 6
#define AM_AbsShort 7
#define AM_AbsLong 8
#define AM_PC16Disp 9
#define AM_PC8Format 10
#define AM_Immediate 11
#define AM_RnList 12
#define AM_FPnList 13
#define AM_FPn 14
#define AM_SpecReg 15


/* operand types */
struct optype {
  uint16_t modes;         /* addressing modes allowed (0-15, see above) */
  uint32_t flags;
  unsigned char first;
  unsigned char last;
};

/* flags */
/* Note: Do not allocate bits from FL_CheckMask! */
#define OTF_NOSIZE      1 /* this addr. mode requires no additional bytes */
#define OTF_BRANCH      2 /* branch instruction */
#define OTF_DATA        4 /* data definition */
#define OTF_FLTIMM      8 /* base10 immediate values are floating point */
#define OTF_QUADIMM  0x10 /* immediate values are 64 bits */
#define OTF_SPECREG  0x20 /* check for special registers during parse */
#define OTF_SRRANGE  0x40 /* check range between first/last only */
#define OTF_REGLIST  0x80 /* register list required, even when single reg. */
#define OTF_CHKVAL  0x100 /* compare op. value against first/last */
#define OTF_CHKREG  0x200 /* compare op. register against first/last */
#define OTF_VXRNG2  0x400 /* Apollo AMMX Rn:Rn+1 vector register range */
#define OTF_VXRNG4  0x800 /* Apollo AMMX Rn-Rn+3 vector register range */
#define OTF_OPT    0x1000 /* optional operand */


/* additional mnemonic data */
typedef struct {
  uint16_t place[MAX_OPERANDS];
  uint16_t opcode[2];
  uint16_t size;
  uint32_t available;
} mnemonic_extension;

/* size qualifiers, lowest two bits specify opcode size in words! */
#define SIZE_UNSIZED 0
#define SIZE_BYTE 0x100
#define SIZE_WORD 0x200
#define SIZE_LONG 0x400
#define SIZE_SINGLE 0x800
#define SIZE_DOUBLE 0x1000
#define SIZE_EXTENDED 0x2000
#define SIZE_PACKED 0x4000
#define SIZE_MASK 0x7f00
#define SIZE_UNAMBIG 0x8000 /* only a single size allowed for this mnemonic */
#define S_CFCHECK 0x80      /* SIZE_LONG only, when mcf (Coldfire) set */
#define S_NONE 4
#define S_STD S_NONE+4      /* 1st word, bits 6-7 */
#define S_STD1 S_STD+4      /* 1st word, bits 6-7, b=1,w=2,l=3  */
#define S_HI S_STD1+4       /* 1st word, bits 9-10 */
#define S_CAS S_HI+4        /* 1st word, bits 9-10, b=1,w=2,l=3 */
#define S_MOVE S_CAS+4      /* move instruction, 1st word bits 12-13 */
#define S_WL8 S_MOVE+4      /* w/l flag in 1st word bit 8 */
#define S_LW7 S_WL8+4       /* l/w flag in 1st word bit 7 */
#define S_WL6 S_LW7+4       /* w/l flag in 1st word bit 6 */
#define S_TRAP S_WL6+4      /* 1st word, bits 1-0, w=2, l=3 */
#define S_EXT S_TRAP+4      /* 2nd word, bits 6-7 */
#define S_FP S_EXT+4        /* 2nd word, bits 12-10 (l=0,s,x,p,w,d,b) */
#define S_MAC S_FP+4        /* w/l flag in 2nd word bit 11 */
#define S_OPCODE_SIZE(n) (n&3)
#define S_SIZEMODE(n) (n&0x7c)

/* short cuts */
#define UNS SIZE_UNSIZED
#define B SIZE_BYTE
#define W SIZE_WORD
#define L SIZE_LONG
#define Q SIZE_DOUBLE
#define SBW (SIZE_BYTE|SIZE_WORD|SIZE_SINGLE)  /* .s = .b for branches */
#define SBWL (SIZE_BYTE|SIZE_WORD|SIZE_LONG|SIZE_SINGLE)
#define BW (SIZE_BYTE|SIZE_WORD)
#define WL (SIZE_WORD|SIZE_LONG)
#define BWL (SIZE_BYTE|SIZE_WORD|SIZE_LONG)
#define CFWL (SIZE_WORD|SIZE_LONG|S_CFCHECK)
#define CFBWL (SIZE_BYTE|SIZE_WORD|SIZE_LONG|S_CFCHECK)
#define ANY (SIZE_BYTE|SIZE_WORD|SIZE_LONG|SIZE_SINGLE|SIZE_DOUBLE| \
             SIZE_EXTENDED|SIZE_PACKED)
#define CFANY (SIZE_BYTE|SIZE_WORD|SIZE_LONG|SIZE_SINGLE|SIZE_DOUBLE)
#define FX SIZE_EXTENDED
#define FD SIZE_DOUBLE


/* operand insertion info */
struct oper_insert {
  unsigned char mode;         /* insert mode (see below) */
  unsigned char size;         /* number of bits to insert */
  unsigned char pos;          /* bit position for inserted value in stream */
  unsigned char flags;
  void (*insert)(unsigned char *,struct oper_insert *,operand *);
};

/* insert modes */
#define M_nop         0       /* do nothing for this operand */
#define M_noea        1       /* don't store ea, only extension words */
#define M_ea          2       /* insert mode/reg in lowest 6 bits */
#define M_high_ea     3       /* insert reg/mode in bits 11-6 (MOVE) */
#define M_bfea        4       /* insert std. ea and bitfield offset/width */
#define M_kfea        5       /* insert std. ea and k-factor/dest.format */
#define M_func        6       /* use insert() function */
#define M_branch      7       /* extval0 contains branch label */
#define M_val0        8       /* extval0 at specified position */
#define M_reg         9       /* insert reg at specified position */
/* flags */
#define IIF_MASK      1       /* value 2^size is represented by a 0 (M_val0)
                                 recognize MASK-flag for MAC instr. (M_ea) */
#define IIF_BCC       2       /* Bcc branch, opcode is modified */
#define IIF_REVERSE   4       /* store bits in reverse order (M_val0) */
#define IIF_NOMODE    8       /* don't store ea mode specifier in opcode */
#define IIF_SIGNED   16       /* value is signed (M_val0) */
#define IIF_3Q       64       /* MOV3Q: -1 is written as 0 (M_val0) */
#define IIF_ABSVAL  128       /* make sure first expr. is absolute (M_func) */
/* redefinition for AMMX special registers (M_func) */
#define IIF_A         8       /* RegBit 4 to A-bit (bit-position 8) */
#define IIF_B         7       /* RegBit 4 to B-bit (bit-position 7) */
#define IIF_D         6       /* RegBit 4 to D-bit (bit-position 6) */


/* CPU models and their type-flags */
struct cpu_models {
  char name[8];
  uint32_t type;
};

/* cpu types for availability check - warning: order is important */
#define CPUMASK  0x00ffffff
#define	m68000   0x00000001
#define	m68010   0x00000002
#define	m68020   0x00000004
#define	m68030   0x00000008
#define	m68040   0x00000010
#define m68060   0x00000020
#define	m68881   0x00000040
#define	m68882   m68881
#define	m68851   0x00000080
#define cpu32    0x00000100
#define mcfa     0x00000200
#define mcfaplus 0x00000400
#define mcfb     0x00000800
#define mcfc     0x00001000
#define mcfhwdiv 0x00002000
#define mcfmac   0x00004000
#define mcfemac  0x00008000
#define mcfusp   0x00010000
#define mcffpu   0x00020000
#define mcfmmu   0x00040000
#define ac68080  0x00100000
#define mbanked  0x10000000 /* Apollo 68080 Bank Prefix */
#define mgas     0x20000000 /* a GNU-as specific mnemonic */
#define malias   0x40000000 /* a bad alias which we should warn about */
#define mfpu     0x80000000 /* just to check if CP-ID needs to be inserted */

/* handy aliases */
#define m68k      (m68000|m68010|m68020|m68030|m68040|m68060)
#define apollo    (ac68080)
#define mcf       (mcfa|mcfaplus|mcfb|mcfc)
#define mcf_all   (mcfa|mcfaplus|mcfb|mcfc|mcfhwdiv|mcfmac|mcfemac|mcfusp|mcffpu|mcfmmu)
#define	mfloat    (mfpu|m68881|m68882|m68040|m68060|ac68080)
#define	mmmu      (m68851|m68030|m68040|m68060)
#define	m68040up  (m68040|m68060|apollo)
#define	m68030up  (m68030|m68040up)
#define	m68020up  (m68020|m68030up)
#define	m68010up  (m68010|cpu32|m68020up)
#define	m68000up  (m68000|m68010up)


/* register symbols */
#define HAVE_REGSYMS
#define REGSYMHTSIZE 256

#define RSTYPE_Dn   0
#define RSTYPE_An   1
#define RSTYPE_FPn  2
#define RSTYPE_Bn   3  /* Apollo only */


/* MID for a.out format */
extern int m68k_mid;
#define MID m68k_mid

/* exported functions */
int m68k_available(int);
int m68k_data_operand(int);
int m68k_operand_optional(operand *,int);
int parse_cpu_label(char *,char **);
