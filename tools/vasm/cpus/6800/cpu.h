/*
 * cpu.h 6800 cpu description file
 * (c) in 2013-2014 by Esben Norby and Frank Wille
*/

#define BIGENDIAN 1
#define LITTLEENDIAN 0
#define VASM_CPU_6800 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 4

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* data type to represent a target-address */
typedef int16_t taddr;
typedef uint16_t utaddr;

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) DATAOP

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) cpu_available(i)

/* allow commas and blanks at the same time to separate instruction operands */
#define OPERSEP_COMMA 1
#define OPERSEP_BLANK 1

/* we define two additional unary operations, '<' and '>' */
int	ext_unary_eval(int,taddr,taddr *,int);
int	ext_find_base(symbol **,expr *,section *,taddr);
#define LOBYTE (LAST_EXP_TYPE+1)
#define HIBYTE (LAST_EXP_TYPE+2)
#define EXT_UNARY_NAME(s) (*s=='<'||*s=='>')
#define EXT_UNARY_TYPE(s) (*s=='<'?LOBYTE:HIBYTE)
#define EXT_UNARY_EVAL(t,v,r,c) ext_unary_eval(t,v,r,c)
#define EXT_FIND_BASE(b,e,s,p) ext_find_base(b,e,s,p)

/* type to store each operand */
typedef struct {
    uint16_t type;
    uint8_t code[2];
    expr *value;
} operand;


/* additional mnemonic data */
typedef struct {
    unsigned char prebyte;
    unsigned char opcode;
    unsigned char dir_opcode;  /* !=0 means optimization to DIR allowed */
    uint8_t available;
} mnemonic_extension;

/* available */
#define M6800   1
#define M6801   2       /* 6801/6803: Adds D register and some extras */
#define M68HC11 4       /* standard 68HC11 instruction set */

/* adressing modes */
#define INH     0
#define IMM	1	/* #$12 */		/* IMM ii */
#define IMM16	2	/* #$1234 */		/* IMM jj kk */
#define ADDR	3
#define EXT	4				/* EXT hh */
#define DIR	5				/* DIR dd */
#define REL	6				/* REL rr */
#define REGX	7
#define REGY	8
#define DATAOP	9	/* data operand */

/* exported by cpu.c */
int cpu_available(int);
