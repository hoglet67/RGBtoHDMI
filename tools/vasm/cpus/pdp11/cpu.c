/*
 * cpu.c PDP-11 cpu description file
 */

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};
int mnemonic_cnt = sizeof(mnemonics) / sizeof(mnemonics[0]);

char *cpu_copyright = "vasm pdp-11 cpu backend 0.2 (c)2020 by Frank Wille";
char *cpuname = "pdp11";
int bitsperbyte = 8;
int bytespertaddr = 2;

static uint8_t cpu_type = STD|PSW;
static int opt_bra;       /* relative branch optimization/translation */
static int OC_BR,OC_JMP;


operand *new_operand()
{
  operand *new = mymalloc(sizeof(*new));
  return new;
}


static int parse_reg(char **start)
{
  char *p = *start;
  regsym *sym;

  if (*p == '%') {
    p++;
    if (isdigit((unsigned char)*p)) {
      int r;

      p++;
      r = (int)parse_constexpr(&p);  /* get register number: %N */
      if (r>=0 && r<=7) {
        *start = p;
        return r;
      }
    }
    return -1;
  }

  if (ISIDSTART(*p)) {
    p++;
    while (ISIDCHAR(*p))
      p++;
    if (sym = find_regsym_nc(*start,p-*start)) {
      *start = p;
      return sym->reg_num;
    }
  }

  return -1;
}


static int parse_regindir(char **start)
{
  char *p = *start;
  int reg;

  if (*p++ == '(') {
    p = skip(p);
    if ((reg = parse_reg(&p)) >= 0) {
      p = skip(p);
      if (*p++ = ')') {
        *start = p;
        return reg;
      }
    }
  }
  return -1;
}


int parse_operand(char *p,int len,operand *op,int required)
{
  int deferred = 0;

  op->value = NULL;
  p = skip(p);

  switch (required) {
    case ANY:
      /* parse any addressing mode */
      if (*p == '@') {
        deferred = 1;
        p++;
      }

      if (*p == '#') {
        /* immediate or absolute addressing */
        p = skip(p+1);
        op->value = parse_expr(&p);
        op->mode = deferred ? MABS : MIMM;
        op->reg = 7;
        return PO_MATCH;
      }

      if ((op->reg = parse_reg(&p)) >= 0) {
        /* register direct or deferred */
        op->mode = deferred ? MREGD : MREG;
        return PO_MATCH;
      }

      if (*p=='(' || (*p=='-' && *(p+1)=='(')) {
        char *oldp = p;

        if (*p == '-') {
          op->mode = MADEC;
          p++;
        }
        else
          op->mode = MREGD;
        if ((op->reg = parse_regindir(&p)) >= 0) {
          if (*p == '+') {
            p++;
            if (op->mode == MREGD)
              op->mode = MAINC;
            else
              return PO_CORRUPT;  /* -(Rn)+ */
          }
          if (deferred) {
            if (op->mode == MREGD)
              return PO_CORRUPT;  /* @(Rn) */
            op->mode++;
          }
          return PO_MATCH;
        }
        p = oldp;
      }

      /* must be an index or an address at this point */
      op->value = parse_expr(&p);
      p = skip(p);

      if ((op->reg = parse_regindir(&p)) >= 0) {
        /* register indexed or register indexed deferred */
        op->mode = deferred ? MIDXD : MIDX;
        return PO_MATCH;
      }
      else {
        /* pc-relative address */
        op->mode = deferred ? MRELD : MREL;
        op->reg = 7;
        return PO_MATCH;
      }
      break;

    case REG:
      if ((op->reg = parse_reg(&p)) >= 0) {
        op->mode = MREG;
        return PO_MATCH;
      }
      break;

    case EXP:
      op->value = parse_expr(&p);
      op->mode = MEXP;
      return PO_MATCH;
  }

  return PO_NOMATCH;
}


char *parse_cpu_special(char *s)
{
  return s;  /* nothing special */
}


static int is_section_relative(struct MyOpVal *opval,section *sec)
{
  return (sec->flags & ABSOLUTE) ? opval->base==NULL :
         opval->base!=NULL && LOCREF(opval->base) && opval->base->sec==sec;
}


static size_t process_instruction(instruction *ip,section *sec,taddr pc,
                                  struct MyOpVal *values,int final)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  size_t size = 2;
  operand *op;
  int i;

  for (i=0; i<MAX_OPERANDS; i++) {
    if ((op = ip->op[i]) == NULL)
      break;

    if (op->value != NULL) {
      if (!eval_expr(op->value,&values[i].value,sec,pc)) {
        values[i].btype = find_base(op->value,&values[i].base,sec,pc);
        if (final && values[i].btype == BASE_ILLEGAL)
          general_error(38);  /* illegal relocation */
      }
      else {
        values[i].base = NULL;
        values[i].btype = BASE_NONE;
      }
    }

    switch (op->mode) {
      case MIDX:
      case MABS:
      case MREL:
        if (opt_bra && ip->code==OC_JMP && op->reg==7
            && is_section_relative(&values[i],sec)) {
          int d = values[i].value - (pc + size);

          if (d>=-256 && d>256) {
            /* JMP a -> BR a */
            ip->code = OC_BR;
            op->mode = MEXP;
            break;
          }
        }
      case MIDXD:
      case MIMM:
      case MRELD:
        size += 2;
        break;

      case MEXP:
        if (opt_bra && mnemo->ext.format==BR) {
          int secrel = is_section_relative(&values[i],sec);
          int d = values[i].value - (pc + size);

          if ((secrel && (d<-256 || d>=256)) || !secrel) {
            if (ip->code == OC_BR) {
              /* BR a -> JMP a */
              ip->code = OC_JMP;
              op->mode = MREL;
              op->reg = 7;
              size += 2;
              break;
            }
            else {
              /* Bcc a -> B!cc .+6, JMP a */
              op->mode = MBCCJMP;
              size += 4;
              break;
            }
          }
        }
        break;
    }
  }
  return size;
}


size_t instruction_size(instruction *ip,section *sec,taddr pc)
{
  struct MyOpVal values[MAX_OPERANDS];

  return process_instruction(copy_inst(ip),sec,pc,values,0);
}


static uint16_t modereg(operand *op)
{
  if (op->mode > MRELD)
    cpu_error(0);  /* bad addressing mode */
  if (op->reg > 7)
    cpu_error(1);  /* bad register */
  return ((op->mode & MMASK)<<3) | (op->reg & 7);
}


static uint16_t stdreg(operand *op)
{
  if (op->reg > 7)
    cpu_error(1);  /* bad register */
  return (op->reg & 7);
}


static uint16_t broffset(struct MyOpVal *opval,section *sec,taddr pc,
                         rlist **relocs)
{
  if (is_section_relative(opval,sec)) {
    taddr d = opval->value - (pc + 2);

    if (d<-256 || d>=256)
      cpu_error(2,(long)d,-256,254);  /* pc-relative out of range */
    return (d>>1) & 0xff;
  }
 
  /* destination is in another section or externally defined */
  if (opval->base!=NULL && opval->btype==BASE_OK)
    add_extnreloc(relocs,opval->base,opval->value-2,REL_PC,0,8,1);
  else
    general_error(38);  /* illegal relocation */
  return (opval->value-2) & 0xff;
}


static uint16_t soboffset(struct MyOpVal *opval,section *sec,taddr pc)
{
  if (is_section_relative(opval,sec)) {
    taddr d = (pc + 2) - opval->value;

    if (d<0 || d>=128)
      cpu_error(2,(long)-d,-126,0);  /* pc-relative out of range */
    return (d>>1) & 0x3f;
  }
  general_error(38);  /* illegal relocation */
  return opval->value & 0x3f;
}


static uint16_t trap(struct MyOpVal *opval,rlist **relocs)
{
  taddr val = opval->value;

  if (opval->base) {
    if (opval->btype == BASE_OK)
      add_extnreloc(relocs,opval->base,val,REL_ABS,0,8,1);
    else
      general_error(38);  /* illegal relocation */
  }
  if (val<0 || val>255)
    cpu_error(3,(long)val,0,255);  /* bad trap code */
  return val & 0xff;
}


static uint8_t *absword(uint8_t *d,struct MyOpVal *opval,rlist **relocs,
                        size_t offs)
{
  if (opval->base) {
    if (opval->btype != BASE_ILLEGAL)
      add_extnreloc(relocs,opval->base,opval->value,
                    opval->btype==BASE_PCREL?REL_PC:REL_ABS,0,16,offs);
    else
      general_error(38);  /* illegal relocation */
  }
  return setval(0,d,2,opval->value);
}


static uint8_t *pcrelword(uint8_t *d,struct MyOpVal *opval,rlist **relocs,
                          section *sec,taddr pc,size_t offs)
{
  if (is_section_relative(opval,sec)) {
    taddr pcd = opval->value - (pc + offs + 2);

    if (pcd<-32768 || pcd>=32767)
      cpu_error(2,(long)pcd,-32768,32767);  /* pc-relative out of range */
    return setval(0,d,2,pcd);
  }
 
  /* destination is in another section or externally defined */
  if (opval->base!=NULL && opval->btype==BASE_OK)
    add_extnreloc(relocs,opval->base,opval->value-2,REL_PC,0,16,offs);
  else
    general_error(38);  /* illegal relocation */
  return setval(0,d,2,opval->value-2);
}


dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
{
  struct MyOpVal values[MAX_OPERANDS];
  mnemonic *mnemo = &mnemonics[ip->code];
  dblock *db = new_dblock();
  uint8_t *d;
  uint16_t oc;
  size_t offs;
  int i;

  /* execute all optimizations for real and determine final size */
  db->size = process_instruction(ip,sec,pc,values,1);
  d = db->data = mymalloc(db->size);

  /* assemble the opcode */
  oc = mnemo->ext.opcode;
  switch (mnemo->ext.format) {
    case NO:  /* no operand */
      break;
    case DO:  /* double operand */
      oc |= (modereg(ip->op[0])<<6) | modereg(ip->op[1]);
      break;
    case SO:  /* single operand */
      oc |= modereg(ip->op[0]);
      break;
    case RO:  /* register and operand */
      oc |= (stdreg(ip->op[0])<<6) | modereg(ip->op[1]);
      break;
    case RG:  /* register only */
      oc |= stdreg(ip->op[0]);
      break;
    case BR:  /* branches */
      if (ip->op[0]->mode == MBCCJMP) {
        oc ^= 0x100;  /* negate the condition */
        oc |= 2;      /* and skip the following JMP-instruction, when true */
      }
      else if (ip->op[0]->mode == MEXP)
        oc |= broffset(&values[0],sec,pc,&db->relocs);
      else
        cpu_error(0);  /* bad addressing mode */
      break;
    case RB:  /* register and branch */
      if (ip->op[1]->mode == MEXP)
        oc |= (stdreg(ip->op[0])<<6) | soboffset(&values[1],sec,pc);
      else
        cpu_error(0);  /* bad addressing mode */
      break;
    case TP:  /* traps */
      if (ip->op[0]->mode == MEXP)
        oc |= trap(&values[0],&db->relocs);
      else
        cpu_error(0);  /* bad addressing mode */
      break;
    default:
      ierror(0);
      break;
  }

  /* write opcode in little-endian */
  *d++ = oc & 0xff;
  *d++ = oc >> 8;
  offs = 2;

  /* handle operands from addressing modes and write extension words */
  for (i=0; i<MAX_OPERANDS; i++) {
    operand *op = ip->op[i];
    taddr val;

    if (op == NULL)
      break;
    val = values[i].value;

    switch (op->mode) {
      case MIDX:
      case MIDXD:
        if (val<-32768 || val>32767)
          cpu_error(4,(long)val);  /* displacement out of range */
        d = absword(d,&values[i],&db->relocs,offs);
        offs += 2;
        break;
      case MIMM:
        if (val<-32768 || val>65535)
          cpu_error(5,(long)val);  /* immediate out of range */
        d = absword(d,&values[i],&db->relocs,offs);
        offs += 2;
        break;
      case MABS:
        if (val<-32768 || val>65535)
          cpu_error(6,(long)val);  /* absolute out of range */
        d = absword(d,&values[i],&db->relocs,offs);
        offs += 2;
        break;
      case MREL:
      case MRELD:
        d = pcrelword(d,&values[i],&db->relocs,sec,pc,offs);
        offs += 2;
        break;
      case MBCCJMP:
        d = setval(0,d,2,0000167);  /* JMP a / JMP d(PC) */
        offs += 2;
        d = pcrelword(d,&values[i],&db->relocs,sec,pc,offs);
        offs += 2;
        break;
    }
  }

  if (offs!=db->size || d!=(db->data+db->size))
    ierror(0);

  return db;
}


dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  taddr val;

  if (bitsize!=8 && bitsize!=16 && bitsize!=32)
    cpu_error(7,bitsize);  /* data size not supported */

  db->size = bitsize >> 3;
  db->data = mymalloc(db->size);

  if (!eval_expr(op->value,&val,sec,pc)) {
    symbol *base;
    int btype;
    
    btype = find_base(op->value,&base,sec,pc);
    if (btype==BASE_OK || btype==BASE_PCREL) {
      add_extnreloc(&db->relocs,base,val,
                    btype==BASE_PCREL?REL_PC:REL_ABS,0,bitsize,0);
    }
    else if (btype != BASE_NONE)
      general_error(38);  /* illegal relocation */
  }

  if (bitsize < 16) {
    if (val<-0x80 || val>0xff)
      cpu_error(8,8);   /* data doesn't fit into 8-bits */
  } else if (bitsize < 32) {
    if (val<-0x8000 || val>0xffff)
      cpu_error(8,16);  /* data doesn't fit into 16-bits */
  }

  setval(0,db->data,db->size,val);
  return db;
}


int cpu_available(int idx)
{
  return (mnemonics[idx].ext.flags & cpu_type) != 0;
}


int init_cpu()
{
  char r[4];
  int i;

  for (i=0; i<mnemonic_cnt; i++) {
    if (mnemonics[i].ext.flags & cpu_type) {
      if (!strcmp(mnemonics[i].name,"br"))
        OC_BR = i;
      if (!strcmp(mnemonics[i].name,"jmp"))
        OC_JMP = i;
    }
  }

  /* define register symbols */
  for (i=0; i<8; i++) {
    sprintf(r,"r%d",i);
    new_regsym(0,1,r,RTYPE_R,0,i);
  }
  new_regsym(0,1,"sp",RTYPE_R,0,6);
  new_regsym(0,1,"pc",RTYPE_R,0,7);

  return 1;
}


int cpu_args(char *p)
{
  if (!strcmp(p,"-eis"))
    cpu_type |= EIS;
  else if (!strcmp(p,"-fis"))
    cpu_type |= FIS;
  else if (!strcmp(p,"-msp"))
    cpu_type |= MSP;
  else if (!strcmp(p,"-opt-branch"))
    opt_bra = 1;
  else
    return 0;

  return 1;
}
