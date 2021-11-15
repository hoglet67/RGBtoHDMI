/* cpu.h qnice cpu-description header-file */
/* (c) in 2016 by Volker Barthelmann */


/* maximum number of operands in one mnemonic */
#define MAX_OPERANDS 2

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* maximum number of additional command-line-flags for this cpu */

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

#define LITTLEENDIAN 1
#define BIGENDIAN 0
#define VASM_CPU_QNICE 1

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
  int reg; /* also cc */
  expr *offset;
} operand;

/* operand-types */

#define OP_REG        1
#define OP_REGIND     2
#define OP_POSTINC    3
#define OP_PREDEC     4
#define OP_ABS        5
#define OP_CC         6
#define OP_REL        7
#define OP_GEN        8
#define OP_DGEN       9
#define OP_PROTECTED  0

/* no derivates yet */
#define CPU_ALL  (-1)

typedef struct {
  unsigned int opcode;
  unsigned int encoding;
} mnemonic_extension;
