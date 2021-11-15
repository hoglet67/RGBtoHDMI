/* cpu.h VideoCore IV cpu-description header-file */
/* (c) in 2013 by Volker Barthelmann */


/* maximum number of operands in one mnemonic */
#define MAX_OPERANDS 4

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 2

/* maximum number of additional command-line-flags for this cpu */

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

#define LITTLEENDIAN 1
#define BIGENDIAN 0
#define VASM_CPU_VC4 1

/* minimum instruction alignment */
#define INST_ALIGN 2

/* default alignment for n-bit data */
#define DATA_ALIGN(n) ((n)<=8?1:(n<=16?2:4))

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) OP_ABS

#define cc reg

/* type to store each operand */
typedef struct {
  int type;
  int reg;
  int dreg;
  long vecmod;
  expr *offset;
} operand;

/* vecmod:
   0-15: f_i or add-reg for vload/vstore
   16-18: rep
   20: setf
   
   for 80-bit 'A' registers, 'dreg' is formatted as
   0-5: flags
   6-9: ra_x
*/

/* operand-types */
enum {
  OP_REG=1,
  OP_SREG,
  OP_PC,
  OP_LR,
  OP_MREG,
  OP_VREG,
  OP_VREGA80,
  OP_VREGM,
  OP_VREGMM,
  OP_ABS,
  OP_REL,
  OP_IMM4,
  OP_IMM5,
  OP_IMM6,
  OP_IMM32M,
  OP_IMM32,
  OP_REGIND,
  OP_IMMINDPC,
  OP_IMMINDSP,
  OP_IMMINDSD,
  OP_IMMINDR0,
  OP_IMMINDS,
  OP_IMMIND,
  OP_PREDEC,
  OP_POSTINC,
  OP_IND,
  OP_VIND,
  OP_DISP5,
  OP_DISP12,
  OP_DISP16,
  OP_DISP27,
};


#define CPU_VC4 1
#define CPU_ALL  (-1)

enum {
  EN_ARITHR16,
  EN_ARITHI16,
  EN_FIX16,
  EN_LEA16,
  EN_IBRANCH16,
  EN_RBRANCH16,
  EN_TABLE16,
  EN_MREG16,
  EN_MEMSTACK16,
  EN_MEMREG16,
  EN_MEMDISP16,
  EN_CONDR32,
  EN_CONDI32,
  EN_RBRANCH32,
  EN_ADDCMPB32,
  EN_MEMREG32,
  EN_MEMDISP32,
  EN_MEMPREDEC,
  EN_MEMPOSTINC,
  EN_MEM12DISP32,
  EN_MEM16DISP32,
  EN_FUNARY32,
  EN_ARITHR32,
  EN_ARITHI32,
  EN_LEA48,
  EN_MEM48,
  EN_ARITHI48,
  EN_ADD48,
  EN_VLOAD48,
  EN_VSTORE48,
  EN_VREAD48,
  EN_VWRITE48,
  EN_VREADI48,
  EN_VWRITEI48,
  EN_VARITHR48,
  EN_VARITHI48,
  EN_ADDCMPB64,
  EN_VLOAD80,
  EN_VSTORE80,
  EN_VREAD80,
  EN_VWRITE80,
  EN_VREADI80,
  EN_VWRITEI80,
  EN_VARITHR80,
  EN_VARITHI80,
};

typedef struct {
  unsigned int encoding;
  unsigned int code;
  unsigned int available;
} mnemonic_extension;
