/* cpu.h TR3200 cpu-description header-file */
/* (c) in 2014 by Luis Panadero Guardeno */

#define LITTLEENDIAN 1
#define BIGENDIAN 0
#define VASM_CPU_TR3200 1

/* maximum number of operands in one mnemonic */
#define MAX_OPERANDS 3

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* maximum number of additional command-line-flags for this cpu */

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* minimum instruction alignment */
#define INST_ALIGN 4

/* default alignment for n-bit data */
#define DATA_ALIGN(n) ((n)<=8 ? 1 : ((n)<=16 ? 2 : 4))


/* type to store each operand */
typedef struct {
  int16_t type;               /* type of operand from mnemonic.operand_type */
  uint8_t reg;                /* Register number if there is one */
  expr *value;                /* single register, immed. val. or branch loc.*/
} operand;

/* operand types */
#define  NO_OP    (0)
#define  OP_GPR   (1) /* %r0 to %r15 or %y, %bp %sp, %ia %flags */
#define  OP_IMM   (2)

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) OP_IMM

#define CPU_ALL  (-1)

typedef struct {
  unsigned char opcode;    /* OpCode */
  unsigned char rn_pos;    /* Position of Rn */
  unsigned char available; /* CPU variants were is available */
} mnemonic_extension;
