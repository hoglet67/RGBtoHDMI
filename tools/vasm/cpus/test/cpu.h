/* cpu.h example cpu-description header-file */
/* (c) in 2002 by Volker Barthelmann */

#define LITTLEENDIAN 1
#define BIGENDIAN 0
#define VASM_CPU_TEST 1

/* maximum number of operands in one mnemonic */
#define MAX_OPERANDS 2

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 1

/* maximum number of additional command-line-flags for this cpu */

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* minimum instruction alignment */
#define INST_ALIGN 2

/* default alignment for n-bit data */
#define DATA_ALIGN(n) ((n)<=8?1:2)

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) OP_ABS

/* type to store each operand */
typedef struct {
  int type;
  int basereg;
  expr *offset;
} operand;

/* operand-types (stupid example) */
#define OP_IMM32           1
#define OP_REG             2
#define OP_ABS             3
#define OP_REGIND          4
/* supersets of other operands */
#define OP_MEM      100
#define OP_ALL      200
#define OP_ALL_DEST 300

#define OP_ISMEM(x) ((x)==OP_ABS||(x)==OP_REGIND)

#define CPU_SMALL 1
#define CPU_LARGE 2
#define CPU_ALL  (-1)

typedef struct {
  unsigned int available;
  unsigned int opcode;
} mnemonic_extension;
