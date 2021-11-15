/* cpu.h c16x/st10 cpu-description header-file */
/* (c) in 2002 by Volker Barthelmann */


/* maximum number of operands in one mnemonic */
#define MAX_OPERANDS 3

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* maximum number of additional command-line-flags for this cpu */

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

#define LITTLEENDIAN 1
#define BIGENDIAN 0
#define VASM_CPU_C16X 1

/* minimum instruction alignment */
#define INST_ALIGN 2

/* default alignment for n-bit data */
#define DATA_ALIGN(n) ((n)<=8?1:2)

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) OP_ABS

#define cc reg

/* type to store each operand */
typedef struct {
  int type;
  int mod;
  int reg,regsfr; /* also cc and boff */
  expr *offset,*boffset;
} operand;

/* operand-types */
#define OP_GPR        1
#define OP_BGPR       2
#define OP_SFR        3
#define OP_BSFR       4
#define OP_ABS        5
#define OP_SEG OP_ABS
#define OP_BABS       6
#define OP_REGDISP    7
#define OP_REGIND     8
#define OP_REG03IND   9
#define OP_BWORD     10
#define OP_BADDR     11
#define OP_IMM2      12
#define OP_IMM3      13
#define OP_IMM4      14
#define OP_IMM7      15
#define OP_IMM8      16
#define OP_IMM16     17
#define OP_CC        18
#define OP_REL       19
#define OP_JADDR     20
#define OP_POSTINC03 21
#define OP_PREDEC03  22
#define OP_POSTINC   23
#define OP_PREDEC    24
#define OP_PROTECTED 0

/* mod types */
#define MOD_SOF 1
#define MOD_SEG 2
#define MOD_DPP0 3
#define MOD_DPP1 4
#define MOD_DPP2 5
#define MOD_DPP3 6
#define MOD_DPPX 7

#define CPU_C166 1
#define CPU_C167 2
#define CPU_ALL  (-1)

typedef struct {
  unsigned int len;
  unsigned int opcode;
  unsigned int match;
  unsigned int lose;
  unsigned int encoding;
  unsigned int available;
} mnemonic_extension;
