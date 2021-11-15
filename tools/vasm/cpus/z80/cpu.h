/*
 * Copyright (c) 2007-2009 Dominic Morris <dom /at/ suborbital.org.uk>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* z80 backend for vasm */

#include <stdint.h>

#define BIGENDIAN 0
#define LITTLEENDIAN 1
#define VASM_CPU_Z80 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 2

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* maximum number of additional command-line-flags for this cpu */

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

#define HAVE_INSTRUCTION_EXTENSION 1
typedef struct {
    int  altd;
    int  ioi;
    int  ioe;
} instruction_ext;

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) OP_DATA

/* we define two additional unary operations, '<' and '>' */
int ext_unary_eval(int,taddr,taddr *,int);
int ext_find_base(symbol **,expr *,section *,taddr);
#define LOBYTE (LAST_EXP_TYPE+1)   
#define HIBYTE (LAST_EXP_TYPE+2)
#define EXT_UNARY_NAME(s) (*s=='<'||*s=='>')
#define EXT_UNARY_TYPE(s) (*s=='<'?LOBYTE:HIBYTE)
#define EXT_UNARY_EVAL(t,v,r,c) ext_unary_eval(t,v,r,c)
#define EXT_FIND_BASE(b,e,s,p) ext_find_base(b,e,s,p)

/* Z80 allows ':' as a statement delimiter in oldstyle-syntax */
#define STATEMENT_DELIMITER ':'


/* type to store each operand */
typedef struct {
    int            reg;
    int            type;
    int            flags;
    int            bit;
    expr          *value;
} operand;


#define CPU_8080    1
#define CPU_Z80     2
#define CPU_RCM2000 4
#define CPU_RCM3000 8
#define CPU_RCM4000 16
#define CPU_Z180    32
#define CPU_EZ80    64
#define CPU_GB80    128
#define CPU_80OS    256

#define CPU_RABBIT (CPU_RCM2000|CPU_RCM3000|CPU_RCM4000)
#define CPU_ZILOG (CPU_Z80|CPU_Z180|CPU_EZ80)
#define CPU_ALL ( CPU_ZILOG | CPU_8080 | CPU_RABBIT | CPU_GB80 )
#define CPU_NOT8080 ( CPU_ALL ^ CPU_8080 )
#define CPU_NOTGB80 ( CPU_ALL ^ CPU_GB80 )

#define RCM_EMU_NONE         0
#define RCM_EMU_INCREMENT    1  /* Move to next instruction */
#define RCM_EMU_LIBRARY      2  /* Call a library routine */

/* Flags for altd */
#define F_IO    1    /* ioe/ioi valid */
#define F_ALTD  2    /* altd valid */
#define F_ALTDW  4   /* warn about altw use */
#define F_ALTDWHL 8  /* Warn about HLREF  use with altd */
#define F_80OS_CHECK 16 /* When both 8080 and 80OS are specified, jp = "jump on plus" else jp = jump (jmp) */
#define F_ALL ( F_IO|F_ALTD)

/* additional mnemonic data */
typedef struct {
    int           mode;        /* Opcode mode */
    uint32_t      opcode;      /* Opcode */
    unsigned short cpus;
    int           modifier_flags;    /* Modifier flags for altd/ioi etc */
    int           rabbit2000_action; /* Action to take on a rabbit */
    int           rabbit3000_action; /* Action to take on a rabbit */
    int           rabbit4000_action; /* Action to take on a rabbit */
    int           gb80_action;    /* Action to take for a GBz80 */
} mnemonic_extension;

/* These values are significant - opcodes derived using them */
#define FLAGS_NZ 0
#define FLAGS_Z  1
#define FLAGS_NC 2
#define FLAGS_C  3
#define FLAGS_PO 4
#define FLAGS_PE 5
#define FLAGS_P 6
#define FLAGS_M 7

/* These values are significant - RCM4000 flags */
#define FLAGS_GT  0
#define FLAGS_GTU 1
#define FLAGS_LT  2
#define FLAGS_V   3

/* Theses values are significant */
#define REG_BC  0x00
#define REG_DE  0x01
#define REG_HL  0x02
#define REG_SP  0x03
#define REG_AF  0x04


/* These values are signifcant - opcodes are derived using them */
#define REG_B      0x00
#define REG_C      0x01
#define REG_D      0x02
#define REG_E      0x03
#define REG_H      0x04
#define REG_L      0x05
#define REG_HLREF  0x06
#define REG_M      0x06
#define REG_A      0x07
/* Extra ones - values from here onwards aren't significant */
#define REG_I      0x08
#define REG_R      0x09
#define REG_F      0x0a
#define REG_PORT   0x0b
/* Rabbit extras */
#define REG_IP     0x0c
#define REG_XPC    0x0d
/* Rabbit 4000 extras */
#define REG_JK     0x0d
#define REG_BCDE   0x0e
#define REG_JKHL   0x0f
/* PW...PZ should be defined in this order */
#define REG_PW     0x10
#define REG_PX     0x11
#define REG_PY     0x12
#define REG_PZ     0x13
#define REG_SU     0x14
#define REG_HTR    0x15

#define REG_PLAIN  0x1f  /* Mask to get the register out */

/* Modifiers */
#define REG_IX     0x20  /* h,l modified by ix */
#define REG_IY     0x40  /* h,l modified by iy */
#define REG_ALT    0x80  /* alternate registers */
#define REG_INDEX  0x100  /* (ix + ) */






/* Operand types */
#define OP_NONE   0x00
#define OP_ABS    0x01
#define OP_ABS16  0x02
#define OP_REG16  0x03
#define OP_REG8   0x04
#define OP_AF     0x05  /* af */
#define OP_SP     0x06  /* sp */
#define OP_HL     0x07  /* hl */
#define OP_DE     0x08  /* de */
#define OP_BC     0x09  /* bc */
#define OP_JK     0x0a
#define OP_BCDE   0x0b
#define OP_JKHL   0x0c
#define OP_A      0x0d      /* a register */
#define OP_R      0x0e      /* r register */
#define OP_I      0x0f     /* i register */
#define OP_F      0x10      /* f - for in f,(c) */
#define OP_PORT   0x11
#define OP_FLAGS  0x12    /* nz, z, nc, c, (po, pe) */
#define OP_FLAGS_RCM  0x13  /* gt, lt, ge, le */
#define OP_ADDR   0x14    /* (xx) - an address */
#define OP_NUMBER 0x15    /* for bit, im, res, set, rst */
#define OP_ADDR8  0x16
#define OP_IP     0x17    /* ip (RCM) */
#define OP_XPC    0x18
#define OP_ABS24  0x19
#define OP_ABS32  0x20
#define OP_ADDR24 0x21
#define OP_ADDR32 0x22
#define OP_IX     0x23
#define OP_IY     0x24
#define OP_IDX32  0x25
#define OP_SU     0x26
#define OP_HTR    0x27
#define OP_DATA   0x3e

#define OP_MASK     0x3f     /* Gets the basic operation out */

/* Modifiers */
#define OP_INDIR  0x40     /* ( x ) */
#define OP_INDEX  0x80     /* Modifier for 8 bit operations with ixh, ixl */
#define OP_ALT    0x100
#define OP_OFFSET 0x200   /* Has an offset as well (always optional) - maybe from ix,iy or sp,hl (rabbit) */
#define OP_ARITH16 0x400
#define OP_PUSHABLE 0x800
#define OP_STD16BIT 0x1000
#define OP_RALT     0x2000  /* Accept an alt register on a rabbit (only used mnemonics) - except (hl) */
#define OP_RALTHL   0x4000  /* Accept an alt (hl) register on a rabbit (only used mnemonics)  */
#define OP_OFFSA    0x8000   /* Have +a as offset */
#define OP_OFFSHL   0x10000  /* Have +hl as offset */
#define OP_OFFSBC   0x20000  /* Have +bc as offset */
#define OP_OFFSDE   0x40000  /* Have +de as offset */
#define OP_OFFSIX   0x80000  /* Have +ix as offset */
#define OP_OFFSIY   0x100000  /* Have +iy as offset */

#define OP_RP          0x200000 /*  Reg Pair (BC, DE, HL, and SP) */
#define OP_RP_PUSHABLE 0x400000 /*  Pushable reg pair (BC, DE, HL, and PSW) */
#define OP_B           0x800000 /*  Reg Pair B (for ldax and stax) */
#define OP_D           0x1000000 /*  Reg Pair D (for ldax and stax) */

/* Opcode types - we can group them together to calculate offsets etc */
#define TYPE_NONE     0x00    /* No modifiers */
#define TYPE_ARITH8   0x01    /* + reg1 index or + reg2 index*/
#define TYPE_MISC8    0x02    /* + reg1 * 8 or + reg 2 * 8*/
#define TYPE_LD8      0x03    /* + reg1 * 8 + reg 2 */
#define TYPE_ARITH16  0x04    /* reg * 16 */

#define TYPE_FLAGS    0x05    /* flags * 8 */
#define TYPE_RELJUMP  0x06    /* jr/djnz/jre (flags *8 if necessary */

#define TYPE_BIT      0x07    /* Bit setting etc */
#define TYPE_RST      0x08    /* rst XX */
#define TYPE_IM       0x09    /* im */
#define TYPE_EDPREF   0x0a    /* Has a 0xed prefix by default (RCM opcodes) */
#define TYPE_IPSET    0x0b    /* ipset X (RCM) */
#define TYPE_IDX32    0x0c    /* p? * 16 */
#define TYPE_NOPREFIX 0x0d    /* Don't add on an indexing prefix (RCM) */
#define TYPE_IDX32R   0x0e    /* p? * 16 + p? * 64*/
#define TYPE_OUT_C_0  0x0f    /* for out (c), number, number can only be 0 */
#define TYPE_RP       0x10    /* Register pair instructions */
