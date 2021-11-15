/*
** cpu.h PowerPC cpu-description header-file
** (c) in 2002,2006,2011-2016 by Frank Wille
*/

extern int ppc_endianess;
#define BIGENDIAN (ppc_endianess)
#define LITTLEENDIAN (!ppc_endianess)
#define VASM_CPU_PPC 1
#define MNEMOHTABSIZE 0x18000

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 5

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* data type to represent a target-address */
typedef int64_t taddr;
typedef uint64_t utaddr;

/* we support floating point constants */
#define FLOAT_PARSER 1

/* minimum instruction alignment */
#define INST_ALIGN 4

/* default alignment for n-bit data */
#define DATA_ALIGN(n) ppc_data_align(n)

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) ppc_data_operand(n)

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) ppc_available(i)

/* returns true when operand type is optional; may init default operand */
#define OPERAND_OPTIONAL(p,t) ppc_operand_optional(p,t)

/* special data operand types: */
#define OP_D8  0x1001
#define OP_D16 0x1002
#define OP_D32 0x1003
#define OP_D64 0x1004
#define OP_F32 0x1005
#define OP_F64 0x1006

#define OP_DATA(t) (t >= OP_D8)
#define OP_FLOAT(t) (t >= OP_F32)

/* PPC specific relocations */
#define REL_PPCEABI_SDA2 (LAST_STANDARD_RELOC+1)
#define REL_PPCEABI_SDA21 (LAST_STANDARD_RELOC+2)
#define REL_PPCEABI_SDAI16 (LAST_STANDARD_RELOC+3)
#define REL_PPCEABI_SDA2I16 (LAST_STANDARD_RELOC+4)
#define REL_MORPHOS_DREL (LAST_STANDARD_RELOC+5)
#define REL_AMIGAOS_BREL (LAST_STANDARD_RELOC+6)
#define LAST_PPC_RELOC (LAST_STANDARD_RELOC+6)


/* type to store each operand */
typedef struct {
  int16_t type;
  unsigned char attr;   /* reloc attribute != REL_NONE when present */
  unsigned char mode;   /* @l/h/ha */
  expr *value;
  expr *basereg;  /* only for d(Rn) load/store addressing mode */
} operand;

/* operand modifier */
#define OPM_NONE 0
#define OPM_LO 1  /* low 16 bits */
#define OPM_HI 2  /* high 16 bits */
#define OPM_HA 3  /* high 16 bits with addi compensation */


/* additional mnemonic data */
typedef struct {
  uint64_t available;
  uint32_t opcode;
} mnemonic_extension;

/* Values defined for the 'available' field of mnemonic_extension.  */
#define CPU_TYPE_PPC          (1ULL)
#define CPU_TYPE_POWER        (2ULL)
#define CPU_TYPE_POWER2       (4ULL)
#define CPU_TYPE_601          (8ULL)
#define CPU_TYPE_COMMON       (0x10ULL)
#define CPU_TYPE_ALTIVEC      (0x20ULL)
#define CPU_TYPE_403          (0x40ULL)
#define CPU_TYPE_405          (0x80ULL)
#define CPU_TYPE_440          (0x100ULL)
#define CPU_TYPE_476          (0x200ULL)
#define CPU_TYPE_BOOKE        (0x400ULL)
#define CPU_TYPE_E300         (0x800ULL)
#define CPU_TYPE_E500         (0x1000ULL)
#define CPU_TYPE_VLE          (0x2000ULL)
#define CPU_TYPE_E500MC       (0x4000ULL)
#define CPU_TYPE_750          (0x8000ULL)
#define CPU_TYPE_7450         (0x10000ULL)
#define CPU_TYPE_ISEL         (0x20000ULL)
#define CPU_TYPE_RFMCI        (0x40000ULL)
#define CPU_TYPE_PMR          (0x80000ULL)
#define CPU_TYPE_TMR          (0x100000ULL)
#define CPU_TYPE_SPE          (0x200000ULL)
#define CPU_TYPE_EFS          (0x400000ULL)
#define CPU_TYPE_860          (0x800000ULL)
#define CPU_TYPE_ANY          (0x1000000000000000ULL)
#define CPU_TYPE_64_BRIDGE    (0x2000000000000000ULL)
#define CPU_TYPE_32           (0x4000000000000000ULL)
#define CPU_TYPE_64           (0x8000000000000000ULL)

/* Shortcuts for PPC instruction sets */
#undef  PPC
#define PPC     CPU_TYPE_PPC
#define PPCCOM  (CPU_TYPE_PPC | CPU_TYPE_COMMON)
#define PPC32   (CPU_TYPE_PPC | CPU_TYPE_32)
#define PPC64   (CPU_TYPE_PPC | CPU_TYPE_64)
#define PPCONLY CPU_TYPE_PPC
#define PPC403  CPU_TYPE_403
#define PPC405  CPU_TYPE_405
#define PPC440  CPU_TYPE_440
#define PPC750  PPC
#define PPC860  CPU_TYPE_860
#define AVEC    CPU_TYPE_ALTIVEC
#define BOOKE   CPU_TYPE_BOOKE
#define E300    CPU_TYPE_E300
#define E500    CPU_TYPE_E500
#define E500MC  CPU_TYPE_E500MC
#define RFMCI   CPU_TYPE_RFMCI
#define ISEL    (CPU_TYPE_ISEL | CPU_TYPE_VLE)
#define SPE     (CPU_TYPE_SPE | CPU_TYPE_VLE)
#define EFS     (CPU_TYPE_EFS | CPU_TYPE_VLE)
#define PPCPMR  CPU_TYPE_PMR
#define PPC43   (CPU_TYPE_403 | CPU_TYPE_440)
#define PPC45   (CPU_TYPE_405 | CPU_TYPE_440)
#define BE3403  (CPU_TYPE_403 | CPU_TYPE_476 | CPU_TYPE_E300 | CPU_TYPE_BOOKE)
#define BE403   (CPU_TYPE_403 | CPU_TYPE_476 | CPU_TYPE_BOOKE)
#define BE476   (CPU_TYPE_476 | CPU_TYPE_BOOKE)
#define VLCOM   (CPU_TYPE_PPC | CPU_TYPE_COMMON | CPU_TYPE_VLE)
#define RFMC476 (CPU_TYPE_RFMCI | CPU_TYPE_476)
#define VLRFMCI (CPU_TYPE_RFMCI | CPU_TYPE_VLE)
#define VLBE403 (CPU_TYPE_403 | CPU_TYPE_476 | CPU_TYPE_BOOKE | CPU_TYPE_VLE)
#define VLBE405 (CPU_TYPE_405 | CPU_TYPE_BOOKE | CPU_TYPE_VLE)
#define VL43    (CPU_TYPE_403 | CPU_TYPE_440 | CPU_TYPE_VLE)
#define VL45    (CPU_TYPE_405 | CPU_TYPE_440 | CPU_TYPE_VLE)
#define VL4376  (CPU_TYPE_403 | CPU_TYPE_440 | CPU_TYPE_476 | CPU_TYPE_VLE)
#define VLBE    (CPU_TYPE_BOOKE | CPU_TYPE_VLE)
#define VLBE476 (CPU_TYPE_476 | CPU_TYPE_BOOKE | CPU_TYPE_VLE)
#define VLBE3   (CPU_TYPE_476 | CPU_TYPE_E300 | CPU_TYPE_BOOKE | CPU_TYPE_VLE)
#define VL7450  (CPU_TYPE_405 | CPU_TYPE_476 | CPU_TYPE_BOOKE | CPU_TYPE_7450 | CPU_TYPE_VLE)
#define VLBEPMR (CPU_TYPE_PMR | CPU_TYPE_BOOKE | CPU_TYPE_VLE)
#define POWER   CPU_TYPE_POWER
#define POWER2  (CPU_TYPE_POWER | CPU_TYPE_POWER2)
#define PPCPWR2 (CPU_TYPE_PPC | CPU_TYPE_POWER | CPU_TYPE_POWER2)
#define POWER32 (CPU_TYPE_POWER | CPU_TYPE_32)
#define COM     (CPU_TYPE_POWER | CPU_TYPE_PPC | CPU_TYPE_COMMON)
#define COM32   (CPU_TYPE_POWER | CPU_TYPE_PPC | CPU_TYPE_COMMON | CPU_TYPE_32)
#define M601    (CPU_TYPE_POWER | CPU_TYPE_601)
#define PWRCOM  (CPU_TYPE_POWER | CPU_TYPE_601 | CPU_TYPE_COMMON)
#define MFDEC1  CPU_TYPE_POWER
#define MFDEC2  (CPU_TYPE_PPC | CPU_TYPE_601)


/* Macros used to form opcodes */
#define OP(x) ((((uint32_t)(x)) & 0x3f) << 26)
#define OPTO(x,to) (OP (x) | ((((uint32_t)(to)) & 0x1f) << 21))
#define OPL(x,l) (OP (x) | ((((uint32_t)(l)) & 1) << 21))
#define A(op, xop, rc) \
  (OP (op) | ((((uint32_t)(xop)) & 0x1f) << 1) | (((uint32_t)(rc)) & 1))
#define B(op, aa, lk) (OP (op) | ((((uint32_t)(aa)) & 1) << 1) | ((lk) & 1))
#define BBO(op, bo, aa, lk) (B ((op), (aa), (lk)) | ((((uint32_t)(bo)) & 0x1f) << 21))
#define BBOCB(op, bo, cb, aa, lk) \
  (BBO ((op), (bo), (aa), (lk)) | ((((uint32_t)(cb)) & 0x3) << 16))
#define DSO(op, xop) (OP (op) | ((xop) & 0x3))
#define M(op, rc) (OP (op) | ((rc) & 1))
#define MME(op, me, rc) (M ((op), (rc)) | ((((uint32_t)(me)) & 0x1f) << 1))
#define MD(op, xop, rc) \
  (OP (op) | ((((uint32_t)(xop)) & 0x7) << 2) | ((rc) & 1))
#define MDS(op, xop, rc) \
  (OP (op) | ((((uint32_t)(xop)) & 0xf) << 1) | ((rc) & 1))
#define SC(op, sa, lk) (OP (op) | ((((uint32_t)(sa)) & 1) << 1) | ((lk) & 1))
#define VX(op, xop) (OP (op) | (((uint32_t)(xop)) & 0x7ff))
#define VXA(op, xop) (OP (op) | (((uint32_t)(xop)) & 0x07f))
#define VXR(op, xop, rc) \
  (OP (op) | (((rc) & 1) << 10) | (((uint32_t)(xop)) & 0x3ff))
#define X(op, xop) (OP (op) | ((((uint32_t)(xop)) & 0x3ff) << 1))
#define XRC(op, xop, rc) (X ((op), (xop)) | ((rc) & 1))
#define XCMPL(op, xop, l) (X ((op), (xop)) | ((((uint32_t)(l)) & 1) << 21))
#define XTO(op, xop, to) (X ((op), (xop)) | ((((uint32_t)(to)) & 0x1f) << 21))
#define XTLB(op, xop, sh) (X ((op), (xop)) | ((((uint32_t)(sh)) & 0x1f) << 11))
#define XFL(op, xop, rc) \
  (OP (op) | ((((uint32_t)(xop)) & 0x3ff) << 1) | (((uint32_t)(rc)) & 1))
#define XL(op, xop) (OP (op) | ((((uint32_t)(xop)) & 0x3ff) << 1))
#define XLLK(op, xop, lk) (XL ((op), (xop)) | ((lk) & 1))
#define XLO(op, bo, xop, lk) \
  (XLLK ((op), (xop), (lk)) | ((((uint32_t)(bo)) & 0x1f) << 21))
#define XLYLK(op, xop, y, lk) \
  (XLLK ((op), (xop), (lk)) | ((((uint32_t)(y)) & 1) << 21))
#define XLOCB(op, bo, cb, xop, lk) \
  (XLO ((op), (bo), (xop), (lk)) | ((((uint32_t)(cb)) & 3) << 16))
#define XO(op, xop, oe, rc) \
  (OP (op) | ((((uint32_t)(xop)) & 0x1ff) << 1) | \
   ((((uint32_t)(oe)) & 1) << 10) | (((uint32_t)(rc)) & 1))
#define XS(op, xop, rc) \
  (OP (op) | ((((uint32_t)(xop)) & 0x1ff) << 2) | (((uint32_t)(rc)) & 1))
#define XFXM(op, xop, fxm) \
  (X ((op), (xop)) | ((((uint32_t)(fxm)) & 0xff) << 12))
#define XSPR(op, xop, spr) \
  (X ((op), (xop)) | ((((uint32_t)(spr)) & 0x1f) << 16) | \
   ((((uint32_t)(spr)) & 0x3e0) << 6))
#define XDS(op, xop, at) \
  (X ((op), (xop)) | ((((uint32_t)(at)) & 1) << 25))
#define XISEL(op, xop) (OP (op) | ((((uint32_t)(xop)) & 0x1f) << 1))
#define XSYNC(op, xop, l) (X ((op), (xop)) | ((((uint32_t)(l)) & 3) << 21))
#define EVSEL(op, xop) (OP (op) | (((uint32_t)(xop)) & 0xff) << 3)


/* The BO encodings used in extended conditional branch mnemonics.  */
#define BODNZF  (0x0)
#define BODNZFP (0x1)
#define BODZF   (0x2)
#define BODZFP  (0x3)
#define BOF     (0x4)
#define BOFP    (0x5)
#define BODNZT  (0x8)
#define BODNZTP (0x9)
#define BODZT   (0xa)
#define BODZTP  (0xb)
#define BOT     (0xc)
#define BOTP    (0xd)
#define BODNZ   (0x10)
#define BODNZP  (0x11)
#define BODZ    (0x12)
#define BODZP   (0x13)
#define BOU     (0x14)

/* The BI condition bit encodings used in extended conditional branch
   mnemonics.  */
#define CBLT    (0)
#define CBGT    (1)
#define CBEQ    (2)
#define CBSO    (3)

/* The TO encodings used in extended trap mnemonics.  */
#define TOLGT   (0x1)
#define TOLLT   (0x2)
#define TOEQ    (0x4)
#define TOLGE   (0x5)
#define TOLNL   (0x5)
#define TOLLE   (0x6)
#define TOLNG   (0x6)
#define TOGT    (0x8)
#define TOGE    (0xc)
#define TONL    (0xc)
#define TOLT    (0x10)
#define TOLE    (0x14)
#define TONG    (0x14)
#define TONE    (0x18)
#define TOU     (0x1f)


/* Prototypes */
int ppc_data_align(int);
int ppc_data_operand(int);
int ppc_available(int);
int ppc_operand_optional(operand *,int);
