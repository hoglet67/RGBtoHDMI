/* cpu.c tr3200 cpu description file */
/* (c) in 2014,2019 by Luis Panadero Guardeno */

#include "vasm.h"

/*#define CPU_DEBUG (1)*/
#ifdef CPU_DEBUG
#define OPERAND_DEBUG (1)
#define INSTR_DEBUG (1)
#endif

char *cpu_copyright="vasm TR3200 cpu module v0.2 by Luis Panadero Guardeno";

char *cpuname="tr3200";
int bitsperbyte=8;
int bytespertaddr=4;

mnemonic mnemonics[]={
#include "opcodes.h"
};

int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);


static taddr opsize(operand *p, unsigned char num_operands, section *sec, taddr pc);

/* parse instruction */
char *parse_instruction(char *s, int *inst_len, char **ext, int *ext_len,
                        int *ext_cnt)
{
/*  char* inst = s;*/
#ifdef CPU_DEBUG
    fprintf(stderr, "parse_inst : \"%.*s\"\n", *inst_len, s);
#endif
/*
  while (*s && !isspace((unsigned char)*s))
    s++;
  *inst_len = s - inst; */
  return s;
}

/* Sets op if is a valid register */
static int parse_reg(char **p, int len, operand *op)
{
  char *rp = skip(*p);
  int reg = -1;

  if (len < 2) {
    return 0;
  }

  if (*rp != '%') {
    return 0;
  }
  rp++;

  if (tolower((unsigned char)rp[0]) != 'r') {
    /* Could be y, bp, sp, ia or flags */
    if (len == 2 && tolower((unsigned char)rp[0]) == 'y') {
      rp++;
      *p = skip(rp);
      op->type = OP_GPR;
      op->reg = 11;

      return 1;
    } else if ( len == 3 && (tolower((unsigned char)rp[0]) == 'b')
        && (tolower((unsigned char)rp[1]) == 'p')  ) {
      rp++; rp++;
      *p = skip(rp);
      op->type = OP_GPR;
      op->reg = 12;

      return 1;
    } else if ( len == 3 && (tolower((unsigned char)rp[0]) == 's')
        && (tolower((unsigned char)rp[1]) == 'p')  ) {
      rp++; rp++;
      *p = skip(rp);
      op->type = OP_GPR;
      op->reg = 13;

      return 1;
    } else if ( len == 3 && (tolower((unsigned char)rp[0]) == 'i')
        && (tolower((unsigned char)rp[1]) == 'a')  ) {
      rp++; rp++;
      *p = skip(rp);
      op->type = OP_GPR;
      op->reg = 14;

      return 1;
    } else if ( len == 6
        && (tolower((unsigned char)rp[0]) == 'f')
        && (tolower((unsigned char)rp[1]) == 'l')
        && (tolower((unsigned char)rp[2]) == 'a')
        && (tolower((unsigned char)rp[3]) == 'g')
        && (tolower((unsigned char)rp[4]) == 's')
        ) {
      rp += 5;
      *p = skip(rp);
      op->type = OP_GPR;
      op->reg = 14;

      return 1;
    }
    /* It's not a register */
    return 0;
  }

  rp++;
  /* Get number */
  if (len < 2 || sscanf(rp, "%u", &reg) != 1) {
    return 0;
  }

  /* "%r0 .. %r15" are valid */
  if (reg < 0 || reg > 15) {
    return 0;
  }

  /* skip digits and return new pointer together with register number */
  while ( isdigit((unsigned char)*rp) ) {
    rp++;
  }

  *p = skip(rp);
  op->type = OP_GPR;
  op->reg = reg;
  return 1;
}

/* Parses operands and reads expressions
 * *p string
 * len string length
 * *op operand
 * requires Type of operand expected
 */
int parse_operand(char *p, int len, operand *op, int requires)
{
  op->type = NO_OP;
#ifdef OPERAND_DEBUG
    fprintf(stderr, "parse_operand (reqs=%02x): \"%.*s\"\t",
           (unsigned)requires, len, p);
#endif

  /* Try to grab the register */
  if (1 != parse_reg(&p, len, op) ) {
#ifdef OPERAND_DEBUG
    fprintf(stderr, "imm\t");
#endif
    /* Its not a register, should be a immediate value or a expression */
    if(p[0]=='#') { /* Immediate value */
      expr *tree;
#ifdef OPERAND_DEBUG
    fprintf(stderr, "# ");
#endif
      op->type = OP_IMM;
      p=skip(p+1);
      tree = parse_expr(&p);
      if (!tree) { /* It's not a valid expresion */
        return PO_NOMATCH;
      }
      op->value = tree;
    } else { /* expresion that would be a immediate value */
#ifdef OPERAND_DEBUG
    fprintf(stderr, "expr\t");
#endif
      op->type = OP_IMM;

      /*int parent=0;*/
      expr *tree;

      /*
      if (*p=='(') {
        parent=1;
        p=skip(p+1);
      }
      */

      tree = parse_expr(&p);
      if (!tree) { /* It's not a valid expresion */
        return PO_NOMATCH ;
      }

      /* Inside of a ( ) */
      /*
      p=skip(p);
      if(parent) {
        if(*p!=')'){
          cpu_error(0);
          return 0;
        } else
        p=skip(p+1);
      }
      */

      op->value=tree;
    }
  }

#ifdef OPERAND_DEBUG
    fprintf(stderr, "(type=%02x)\n", (unsigned) op->type);
#endif

  if(requires == op->type) { /* Matched type */
    return PO_MATCH;
  }

  return PO_NOMATCH; /* Ops! Not match */
}

/* Convert an instruction into a DATA atom including relocations,
   if necessary. */
dblock *eval_instruction (instruction *p, section *sec, taddr pc)
{
  /* Calc instruction size */
  size_t size = instruction_size(p, sec, pc);
  dblock *db = new_dblock();
  mnemonic m = mnemonics[p->code];
  unsigned char *d; /* Data */
  taddr val;
  unsigned char ml_bits = 0;
  unsigned char num_operands = 0;
  unsigned char srn = 0; /* Size of Rn */
  operand* rn = NULL;
  symbol *base = NULL;
  int btype;

#ifdef INSTR_DEBUG
  fprintf(stderr, "eval_instruction code \"%s\" ", m.name);
#endif

  num_operands += (m.operand_type[0] != NO_OP)? 1 : 0;
  num_operands += (m.operand_type[1] != NO_OP)? 1 : 0;
  num_operands += (m.operand_type[2] != NO_OP)? 1 : 0;
  if (num_operands > 0) {
    rn = p->op[m.ext.rn_pos];
  }

#ifdef INSTR_DEBUG
  fprintf(stderr, "P%d ", num_operands);
  if (num_operands > 0)
    fprintf(stderr, "rn type=%02x ", (unsigned) rn->type);
#endif

  /* See if Rn is an immediate to set ML bits*/
  if (rn != NULL && rn->type == OP_IMM) {
    srn = opsize(rn, num_operands, sec, pc);
    ml_bits = 2 | (srn == 4);
  }
  ml_bits = ml_bits << 6; /* Emplace ML bits */
#ifdef INSTR_DEBUG
  fprintf(stderr, "IMM=%d ", srn);
  fprintf(stderr, "ml %02x ", ml_bits);
#endif

  db->size=size;
  db->data = mymalloc(size); /* allocate for the data block */
  memset(db->data, 0, db->size);
  /* Here to write data ! */
  d = db->data;
  d[3] = m.ext.opcode; /* Common part */
  d[2] = ml_bits;

  switch (num_operands) {
    case 3: /* format P3 */
      /* Rn is always at the LSBytes side */
      if( rn->type == OP_GPR) { /* Is a register */
        d[0] = (rn->reg) & 0xF;
      } else if (ml_bits == 0x80 ) { /* ML are 10 -> immediate */
        eval_expr(rn->value, &val, sec, pc);
#ifdef INSTR_DEBUG
    fprintf(stderr, "val %04x ", val);
#endif
        d[1] = (val >>8)  & 0x3F;
        d[0] = (val )     & 0xFF;
      } else { /* 32 bit immediate */
        eval_expr(rn->value, &val, sec, pc);
#ifdef INSTR_DEBUG
    fprintf(stderr, "val %08x ", val);
#endif
        d[7] = (val >>24) & 0xFF;
        d[6] = (val >>16) & 0xFF;
        d[5] = (val >>8 ) & 0xFF;
        d[4] = (val )     & 0xFF;
      }

      if (m.ext.rn_pos == 1) { /* Special case of STORE */
        d[1] |= (p->op[0]->reg & 0xF) << 6;
        d[2] |= (p->op[0]->reg & 0xF) >> 2;
        d[2] |= (p->op[2]->reg) << 2; /* rd */
      } else {
        d[1] |= (p->op[1]->reg & 0xF) << 6;
        d[2] |= (p->op[1]->reg & 0xF) >> 2;
        d[2] |= (p->op[0]->reg) << 2; /* rd */
      }

      break;

    case 2: /* format P2 */
      if( rn->type == OP_GPR) { /* Is a register */
        d[0] = (rn->reg) & 0xF;
      } else if (ml_bits == 0x80 ) { /* ML are 10 -> immediate */
        eval_expr(rn->value, &val, sec, pc);
        /* CALL/JUMP stuff */
        if (m.ext.opcode == 0x4B || m.ext.opcode == 0x4C ) {
          val = val >> 2; /* CALL/JMP does a left shift of two bits */
        }
#ifdef INSTR_DEBUG
    fprintf(stderr, "val %06x ", val);
#endif
        d[2] |= (val >>16)  & 0x03;
        d[1] =  (val >>8)  & 0xFF;
        d[0] =  (val )     & 0xFF;

      } else { /* 32 bit immediate */
        eval_expr(rn->value, &val, sec, pc);
#ifdef INSTR_DEBUG
    fprintf(stderr, "val %08x ", val);
#endif
        d[7] = (val >>24) & 0xFF;
        d[6] = (val >>16) & 0xFF;
        d[5] = (val >>8 ) & 0xFF;
        d[4] = (val )     & 0xFF;
      }

      if (m.ext.rn_pos == 0) { /* Special case of STORE */
        d[2] |= (p->op[1]->reg) << 2;
      } else {
        d[2] |= (p->op[0]->reg) << 2;
      }
      break;

    case 1: /* format P1 */
      if( rn->type == OP_GPR) { /* Is a register */
        d[0] = (rn->reg) & 0xF;
      } else if (ml_bits == 0x80 ) { /* ML are 10 -> immediate */
        if (!eval_expr(rn->value, &val, sec, pc))
          btype = find_base(rn->value, &base, sec, pc);
        /* CALL/JUMP stuff */
        if (m.ext.opcode >= 0x27 && m.ext.opcode <= 0x28 ) {
          if ((base != NULL && btype == BASE_OK && !is_pc_reloc(base, sec)) ||
              base == NULL)
            val = val - pc - 4; /* Relative jump/call (%pc has been increased) */
          else if (btype == BASE_OK)
            add_extnreloc_masked(&db->relocs, base, val-4, REL_PC,
                                 0, 22, 0, 0xfffffc);
          else
            general_error(38);  /* @@@ illegal relocation */
          base = NULL;
          val = val >> 2; /* CALL/JMP does a left shift of two bits */
        } else if (m.ext.opcode >= 0x25 && m.ext.opcode <= 0x26 ) {
          if (base != NULL && btype != BASE_ILLEGAL) {
            add_extnreloc_masked(&db->relocs, base, val,
                                 btype == BASE_PCREL ? REL_PC : REL_ABS,
                                 0, 22, 0, 0xfffffc);
            base = NULL;
          }
        }

#ifdef INSTR_DEBUG
    fprintf(stderr, "val %06x ", val);
#endif
        d[2] |= (val >>16)  & 0x3F;
        d[1] =  (val >>8)  & 0xFF;
        d[0] =  (val )     & 0xFF;

      } else { /* 32 bit immediate */
        eval_expr(rn->value, &val, sec, pc);
        /* CALL/JUMP stuff */
        if (m.ext.opcode >= 0x27 && m.ext.opcode <= 0x28 ) {
          val = val - pc - 4; /* Relative jump/call */
        }
#ifdef INSTR_DEBUG
    fprintf(stderr, "val %08x ", val);
#endif
        d[7] = (val >>24) & 0xFF;
        d[6] = (val >>16) & 0xFF;
        d[5] = (val >>8 ) & 0xFF;
        d[4] = (val )     & 0xFF;
      }

      break;

    case 0: /* format NP */
#ifdef INSTR_DEBUG
    fprintf(stderr, "NP ");
#endif
    default:
    ;
  }
#ifdef INSTR_DEBUG
  {
    int i;
    for(i= db->size -1; i >= 0; i-- ) {
      fprintf(stderr, "%02X ", d[i]);
    }
  }
  fprintf(stderr, "\n");
#endif

  return db;
}

/* Create a dblock (with relocs, if necessary) for size bits of data. */
dblock *eval_data(operand *op, size_t bitsize, section *sec, taddr pc)
{
  dblock *new=new_dblock();
  taddr val;
  new->size = bitsize >> 3;
  new->data = mymalloc(new->size);
#ifdef CPU_DEBUG
    fprintf(stderr, "eval_data ");
#endif
  if(op->type != OP_IMM) { /* ??? */
#ifdef CPU_DEBUG
    fprintf(stderr, "!= OP_IMM\n");
#endif
    ierror(0);
  }

  if(bitsize!=8 && bitsize!=16 && bitsize!=32) {
#ifdef CPU_DEBUG
    fprintf(stderr, "bad data size\n");
#endif
    cpu_error(2); /* Invalid data size */
  }

  if(!eval_expr(op->value, &val, sec, pc) ) {
    symbol *base;
    int btype;

    btype = find_base(op->value, &base, sec, pc);
    if (base)
      add_extnreloc(&new->relocs, base, val,
                    btype==BASE_PCREL ? REL_PC : REL_ABS, 0, bitsize, 0);
    else if (btype != BASE_NONE)
      general_error(38);  /* illegal relocation */
  }

  if(bitsize == 8){
    new->data[0] = val & 0xFF;

  } else if (bitsize == 16){
    new->data[1] = (val>>8)  & 0xFF;
    new->data[0] = val       & 0xFF;

  } else if (bitsize == 32){
    new->data[3] = (val>>24) & 0xFF;
    new->data[2] = (val>>16) & 0xFF;
    new->data[1] = (val>>8)  & 0xFF;
    new->data[0] = val       & 0xFF;
  }
  return new;
}

/* Size of a operand
 * *p operand
 * num_operand Total number of operands
 * */
static taddr opsize(operand *p, unsigned char num_operands, section *sec, taddr pc)
{
  taddr val = 0;
  if(!p) {
    return 0;

  } else if (p->type == OP_IMM ) {
    eval_expr(p->value, &val, sec, pc);
    if (num_operands == 3
        && (val < -8192 || val > 8191) ) {          /* 14 bits */
      return 4; /* 32 bit immediate */
    } else if (num_operands == 2
        && (val < -131072 || val > 131071) ) {  /* 18 bits */
      return 4; /* 32 bit immediate */
    } else if (num_operands == 1
        && (val < -2097152 || val > 2097151) ) { /* 22 bits */
      return 4; /* 32 bit immediate */
    }
  }
  return 0;
}

/* Calculate the size of the current instruction; must be identical
   to the data created by eval_instruction. */
size_t instruction_size (instruction *p, section *sec, taddr pc)
{
  size_t size = 4; /* four bytes */
  unsigned char num_operands = 0;
  num_operands += (p->op[0] != 0)? 1 : 0;
  num_operands += (p->op[1] != 0)? 1 : 0;
  num_operands += (p->op[2] != 0)? 1 : 0;

  size += opsize(p->op[0], num_operands, sec, pc);
  size += opsize(p->op[1], num_operands, sec, pc);
  size += opsize(p->op[2], num_operands, sec, pc);

  return size;
}

operand *new_operand()
{
  operand *new = mymalloc(sizeof(*new));
  new->type=-1;
  return new;
}

/* return true, if initialization was successfull */
int init_cpu()
{
  return 1;
}

/* return true, if the passed argument is understood */
int cpu_args(char *p)
{
  /* no args */
  return 0;
}

/* parse cpu-specific directives; return pointer to end of
   cpu-specific text */
char *parse_cpu_special(char *s)
{
  /* no specials */
  return s;
}
