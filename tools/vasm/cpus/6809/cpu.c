/*
 * cpu.c 6809/6309/68HC12 cpu description file
 */

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};
int mnemonic_cnt = sizeof(mnemonics) / sizeof(mnemonics[0]);

char *cpu_copyright = "vasm 6809/6309/68hc12 cpu backend 0.4a (c)2020-2021 by Frank Wille";
char *cpuname = "6809";
int bitsperbyte = 8;
int bytespertaddr = 2;

static uint8_t cpu_type = M6809;
static int modifier;      /* set by find_base() */
static uint8_t dpage = 0; /* default direct page - set with SETDP */

static int opt_off;       /* constant offset optimization 0,R to ,R */
static int opt_bra;       /* relative branch optimization/translation */
static int opt_pc;        /* optimize all EXT addressing to PC-relative */

static int OC_BRA,OC_BSR,OC_LBRA,OC_LBSR;
static int RIDX_PC;

static struct CPUReg registers[] = {
#include "registers.h"
};
static int reg_cnt = sizeof(registers) / sizeof(registers[0]);

static int psh_postbyte_map[] = {
  /* D, X, Y, U, S, PC, W, V, A, B, CC, DP, 0, 0, E, F */
  3<<1,1<<4,1<<5,1<<6,1<<6,1<<7,-1,-1,1<<1,1<<2,1<<0,1<<3,-1,-1,-1,-1
};
static int ir_postbyte_map09[] = {
  /* D, X, Y, U, S, PC, W, V, A, B, CC, DP, 0, 0, E, F */
  0,1,2,3,4,5,6,7,8,9,10,11,-1,13,14,15
};
static int ir_postbyte_map12[] = {
  /* D, X, Y, U, S, PC, W, V, A, B, CC, DP, 0, 0, E, F */
  4,5,6,7,7,-1,-1,-1,0,1,2,-1,-1,-1,-1,-1
};
static int idx_postbyte_map09[] = {
  /* D, X, Y, U, S, PC, W, V, A, B, CC, DP, 0, 0, E, F */
  -1,0x00,0x20,0x40,0x60,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};
static int idx_postbyte_map12[] = {
  /* D, X, Y, U, S, PC, W, V, A, B, CC, DP, 0, 0, E, F */
  -1,0,1,2,2,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};
static int offs_postbyte_map09[] = {
  /* D, X, Y, U, S, PC, W, V, A, B, CC, DP, 0, 0, E, F */
  11,-1,-1,-1,-1,-1,14,-1,6,5,-1,-1,-1,-1,7,10
};
static int offs_postbyte_map12[] = {
  /* D, X, Y, U, S, PC, W, V, A, B, CC, DP, 0, 0, E, F */
  2,-1,-1,-1,-1,-1,-1,-1,0,1,-1,-1,-1,-1,-1,-1
};
static int bitm_postbyte_map[] = {
  /* D, X, Y, U, S, PC, W, V, A, B, CC, DP, 0, 0, E, F */
  -1,-1,-1,-1,-1,-1,-1,-1,1,2,0,-1,-1,-1,-1,-1
};
static int dbr_postbyte_map[] = {
  /* D, X, Y, U, S, PC, W, V, A, B, CC, DP, 0, 0, E, F */
  4,5,6,7,7,-1,-1,-1,0,1,-1,-1,-1,-1,-1,-1
};


int ext_unary_type(char *s)
{
  return *s=='<' ? LOBYTE : HIBYTE;
}


int ext_unary_eval(int type,taddr val,taddr *result,int cnst)
{
  switch (type) {
    case LOBYTE:
      *result = cnst ? (val & 0xff) : val;
      return 1;
    case HIBYTE:
      *result = cnst ? ((val >> 8) & 0xff) : val;
      return 1;
    default:
      break;
  }
  return 0;  /* unknown type */
}


int ext_find_base(symbol **base,expr *p,section *sec,taddr pc)
{
  /* addr/256 equals >addr, addr%256 and addr&255 equal <addr */
  if (p->type==DIV || p->type==MOD) {
    if (p->right->type==NUM && p->right->c.val==256)
      p->type = p->type == DIV ? HIBYTE : LOBYTE;
  }
  else if (p->type==BAND && p->right->type==NUM && p->right->c.val==255)
    p->type = LOBYTE;

  if (p->type==LOBYTE || p->type==HIBYTE) {
    modifier = p->type;
    return find_base(p->left,base,sec,pc);
  }
  return BASE_ILLEGAL;
}


void init_instruction_ext(instruction_ext *ext)
{
  ext->ocidx = OCSTD;
  ext->dp = dpage;  /* current DP defined by SETDP directive */
}


operand *new_operand()
{
  operand *new = mymalloc(sizeof(*new));
  new->mode = 0;
  return new;
}


static int parse_reg(char **start,uint32_t avail)
{
  char *s,*p;
  int i,len;

  p = s = *start;
  while (isalnum((unsigned char)*p))
    p++;

  if (!(len = p - s))
    return -1;

  for (i=0; i<reg_cnt; i++) {
    if ((registers[i].cpu & cpu_type) && (registers[i].avail & avail) &&
        registers[i].len==len && !strnicmp(registers[i].name,s,len)) {
      *start = p;
      return i;
    }
  }
  return -1;
}


static int read_index(char **start,operand *op)
{
  int indir = (op->flags & AF_INDIR) != 0;
  int preinc=0,postinc=0;
  char *p = *start;
  char c = *p;
  uint32_t a;
  int reg;

  switch (op->mode) {
    case AM_NOFFS:
      a = indir ? R_ICIX0 : R_DCIX0;
      break;
    case AM_ROFFS:
      a = indir ? R_IAIX : R_DAIX;
      break;
    case AM_COFFS:
      a = indir ? R_ICIDX : R_DCIDX;
      break;
    default:  /* mode still unknown - allow all index variants */
      a = indir ? (R_ICIDX|R_IAIX) : (R_DCIDX|R_DAIX);
      break;
  }

  /* optional index pre-increment/decrement */
  if (c=='+' || c=='-') {
    while (*p == c) {
      if (c == '+')
        preinc++;
      else
        preinc--;
      p++;
    }
  }

  if ((reg = parse_reg(&p,indir?R_IIDX:R_DIDX)) < 0)
    return 0;

  /* index register ok, now look for post-increment/decrement */
  c = *p;
  if (preinc==0 && (c=='+' || c=='-')) {  /* both would be strange */
    while (*p == c) {
      if (c == '+')
        postinc++;
      else
        postinc--;
      p++;
    }
  }

  /* check if pre/post increment/decrements are valid for this register */
  switch (preinc) {
    case -2:
      a = indir ? R_IPDIX : R_PD2IX;
      break;
    case -1:
      a = indir ? 0 : R_PD1IX;
      break;
    case 1:
      a = indir ? 0 : R_PI1IX;
    case 0:
      break;      
    default:
      a = 0;
      break;
  }
  switch (postinc) {
    case -1:
      a = indir ? 0 : R_QD1IX;
      break;
    case 1:
      a = indir ? 0 : R_QI1IX;
      break;
    case 2:
      a = indir ? R_IQIIX : R_QI2IX;
    case 0:
      break;
    default:
      a = 0;
      break;
  }
  if (!(registers[reg].avail & a))
    return 0;  /* register does not support increment/decrement */

  op->opreg = reg;
  op->preinc = preinc;
  op->postinc = postinc;
  *start = p;
  return 1;
}


int parse_operand(char *p,int len,operand *op,int required)
{
  static int rel_mode_map[] = { AM_REL8, AM_REL9, AM_REL16 };
  char *start = p;
  int ret = PO_MATCH;
  int indir = 0;
  int mode,reg;

  p = skip(p);

  if (mode = required & OTMASK) {
    /* instruction allows just a single operand type */
    char *s = p;

    if (p-start >= len)
      return PO_NOMATCH;  /* empty operand */

    switch (mode) {
      case DT1:
        if (*p == '#')  /* # is optional here */
          p = skip(++p);
        op->mode = AM_IMM8;
        op->value = parse_expr(&p);
        break;

      case RLS:
      case RLD:
      case RLL:
        /* read an expression, symbol or label */
        op->mode = rel_mode_map[mode-RLS];
      case DTA:
        op->value = parse_expr(&p);
        break;

      case TFR:
        /* transfer/exchange registers */
        if ((reg = parse_reg(&p,R_IRP)) < 0)
          return PO_NOMATCH;
        op->mode = AM_TFR;
        op->opreg = reg;
        break;

      case TFM:
        /* transfer memory pointer-register, with increment or decrement */
        if ((reg = parse_reg(&p,R_IRP)) < 0)
          return PO_NOMATCH;
        if (required & (TFM_PLUS|TFM_MINUS)) {
          if (((required & TFM_PLUS) && *p=='+') ||
              ((required & TFM_MINUS) && *p=='-'))
            p++;
          else
            return PO_NOMATCH;
        }
        op->mode = AM_TFR;
        op->opreg = reg;
        break;

      case PPL:
        /* read next register for a comma-separated register list */
        if ((reg = parse_reg(&p,R_STK)) < 0)
          return PO_NOMATCH;
        if ((reg = psh_postbyte_map[registers[reg].value]) < 0)
          ierror(0);
        op->mode = AM_REGXB;
        op->curval |= reg;      /* encode for PSH/PUL postbyte */
        ret = PO_AGAIN;         /* do it again until out of operands */
        break;

      case BMR:
        /* read register for bit manipulation instruction */
        if ((reg = parse_reg(&p,R_BMP)) < 0)
          return PO_NOMATCH;
        op->mode = AM_BITR;
        op->opreg = reg;
        break;

      case BIT:
        /* bit number */
        op->mode = AM_BITN;
        op->value = parse_expr(&p);
        break;

      case DBR:
        /* read register for bit DBcc/IBcc/TBcc instructions */
        if ((reg = parse_reg(&p,R_DBR)) < 0)
          return PO_NOMATCH;
        op->mode = AM_DBR;
        op->opreg = reg;
        break;

      default:
        ierror(0);
        break;
    }

    if (p-s == 0)
      return PO_NOMATCH;  /* nothing read */

    p = skip(p);
    if (p-start < len) {
      cpu_error(0);  /* trailing garbage */
      return PO_CORRUPT;
    }
    return ret;
  }

  else {
    /* immediate, direct, extended and indexed modes may be combined */
    mode = AM_NONE;
    op->opreg = -1;
  
    if (p-start >= len) {
      /* empty operand */
      if (!op->mode && (required & IDX0)) {
        op->mode = AM_NOFFS;
        return PO_AGAIN;  /* may be a ,R addressing mode without offset */
      }
      return PO_NOMATCH;
    }

    if (*p == '#') {
      if (required & IMM) {
        /* immediate addressing mode */
        p = skip(++p);
        op->value = parse_expr(&p);
        mode = (required & IM1) ?
               AM_IMM8 : ((required & IM2) ? AM_IMM16 : AM_IMM32);
      }
      else
        return PO_NOMATCH;
    }
    else if (*p == '[') {
      if (required & IND) {
        /* indirect indexed addressing mode */
        p = skip(++p);
        op->flags |= AF_INDIR;
        indir = 1;
      }
      else
        return PO_NOMATCH;
    }

    if (!op->mode && (required & (IDX0|IIXR))) {
      /* check for direct/indirect offset register */
      if ((reg = parse_reg(&p,indir?R_IOFF:R_DOFF)) >= 0) {
        op->offs_reg = reg;
        mode = AM_ROFFS;
        p = skip(p);
      }
    }

    if (!op->mode && !mode && p-start<len && *p!=',' && *p!='\0') {
      /* try to read any expression as an offset or address */
      if ((required & DIR) && *p=='<') {
        op->flags |= AF_LO;  /* remember '<' hint */
        p = skip(++p);
        mode = AM_DIR;
      }
      else if ((required & DIR) && !(required & EXT))
        mode = AM_DIR;
      else if ((required & EXT) && *p=='>') {
        op->flags |= AF_HI;  /* remember '>' hint */
        p = skip(++p);
        mode = AM_EXT;
      }
      else if ((required & EXT) && !(required & DIR))
        mode = AM_EXT;
      else if (required & MEM) {
        if (*p == '<') {
          op->flags |= AF_LO;  /* remember '<' hint */
          p = skip(++p);
        }
        else if (*p == '>') {
          op->flags |= AF_HI;  /* remember '>' hint */
          p = skip(++p);
        }
        mode = AM_ADDR;  /* size unknown, will become AM_DIR or AM_EXT */
      }
      else
        return PO_NOMATCH;

      op->value = parse_expr(&p);
    }

    /* handle the index register part right of the comma */
    if (indir) {
      if (*p == ',') {
        /* indirect addressing mode has an index register */
        p = skip(++p);
        if (!mode)
          mode = AM_NOFFS;
        else if (mode != AM_ROFFS)
          mode = AM_COFFS;
        if (!read_index(&p,op)) {
          cpu_error(2);  /* missing valid index reg. */
          return PO_CORRUPT;
        }
        p = skip(p);
      }
      else if (!(cpu_type & HC12) &&
               (mode==AM_DIR || mode==AM_ADDR || mode==AM_EXT))
        mode = AM_EXT;
      else
        ret = PO_NOMATCH;

      if (*p++ != ']') {
        cpu_error(1);  /* ] expected */
        return PO_CORRUPT;
      }
    }
    else if (op->mode) {
      /* continue parsing the same operand with a new argument */

      switch (op->mode) {
        case AM_NOFFS:
        case AM_ROFFS:
        case AM_COFFS:
        case AM_DIR:
        case AM_EXT:
        case AM_ADDR:
          /* read second part of indirect addressing mode */
          if (!(read_index(&p,op)))
            return PO_NEXT;  /* no index found - continue with next operand */
          mode = (op->mode >= AM_NOFFS) ? op->mode : AM_COFFS;
          break;

        default:
          ierror(0);
          return PO_CORRUPT;
      }
    }
    else if (!op->mode && (required & (IDX0|IDX1|IDX2))) {
      /* certain indexed addressing modes might need a second pass */

      switch (mode) {
        case AM_NOFFS:
        case AM_ROFFS:
        case AM_COFFS:
        case AM_DIR:
        case AM_EXT:
        case AM_ADDR:
          ret = PO_AGAIN;
          break;
      }
    }
  }

  p = skip(p);
  if (p-start < len) {
    cpu_error(0);  /* trailing garbage */
    return PO_CORRUPT;
  }

  op->mode = mode;
  return ret;
}


char *parse_cpu_special(char *start)
{
  char *name=start,*s=start;

  if (ISIDSTART(*s)) {
    s++;
    while (ISIDCHAR(*s))
      s++;

    if (dotdirs && *name=='.')
      ++name;

    if (s-name==5 && !strnicmp(name,"setdp",5)) {
      utaddr dp;
      s = skip(s);
      dp = (utaddr)parse_constexpr(&s);
      if (dp > 0xff)
        dp >>= 8;
      if (!(cpu_type & HC12))
        dpage = dp;
      else
        cpu_error(16);  /* setdp ignored on HC12 */
      eol(s);
      return skip_line(s);
    }
    else if (s-name==6 && !strnicmp(name,"direct",6)) {
      char *name;
      s = skip(s);
      if (name = parse_identifier(&s)) {
        symbol *sym = new_import(name);
        myfree(name);
        if (!(cpu_type & HC12))
          sym->flags |= DPAGESYM;
        else
          cpu_error(16);  /* dpage ignored on HC12 */
        eol(s);
      }
      else
        cpu_error(8);  /* identifier expected */
      return skip_line(s);
    }
  }
  return start;
}


static void check_opreg(operand *op,int final)
{
  if (op->opreg < 0) {
    if (final)
      cpu_error(2);  /* missing valid index register */
    op->opreg = 0;
  }
}


static size_t process_instruction(instruction *ip,section *sec,
                                  taddr orig_pc,int final)
{
  static int coffs_size[4] = { 0,1,1,2 };  /* OFF5,OFF8,OFF9,OFF16 */
  static int rel_size[3] = { 1,1,2 };      /* REL8,REL9,REL16 */
  static int mov_val_offs[4] = { -1,-2,1,2 }; /* IMMIDX,EXTIDX,IDXIDX,IDXEXT */
  static int mov_pc_offs[4] = { 1,2,1,2 };    /* IMMIDX,EXTIDX,IDXIDX,IDXEXT */
  taddr pc = orig_pc;
  taddr pcd;
  operand *op;
  int ocidx = -1;
  int mcvoff = 0;  /* curval offset for mov pc-addressing mode */
  int mpcoff = 0;  /* pc offset for mov pc-addressing mode */
  int i,j,btype;

  /* evaluate all expressions */
  for (i=0; i<MAX_OPERANDS; i++) {
    if ((op = ip->op[i]) == NULL)
      break;
    op->base = NULL;

    if (op->value != NULL) {
      if (!eval_expr(op->value,&op->curval,sec,pc)) {
        modifier = 0;
        btype = find_base(op->value,&op->base,sec,pc);
        if (btype==BASE_PCREL && modifier==0) {
          switch (op->mode) {
            case AM_IMM8:
            case AM_IMM16:
            case AM_IMM32:
              op->flags |= AF_PC;  /* create a PC-relative relocation */
              break;
          }
        }
        if (final) {
          if (btype==BASE_ILLEGAL ||
              (btype==BASE_PCREL && !(op->flags & AF_PC)))
            general_error(38);  /* illegal relocation */
        }
        if (modifier) {
          if (op->flags & (AF_LO|AF_HI))
            cpu_error(17);  /* double size modifier ignored */
          switch (modifier) {
            case LOBYTE: op->flags |= AF_LO; break;
            case HIBYTE: op->flags |= AF_HI; break;
          }
        }
      }
    }
  }

  if (mnemonics[ip->code].ext.flags & MOVE) {
    for (i=0,j=0; i<MAX_OPERANDS; i++) {
      if (ip->op[i] == NULL) {
        if (i != 2)
          ierror(0);  /* movb/movw must have two operands */
        break;
      }
      switch (ip->op[i]->mode) {
        case AM_COFFS:
        case AM_NOFFS:
        case AM_ROFFS:
          mpcoff = 1;  /* found an indexed addressing mode */
          j += i ? 0 : 2;
          break;
        case AM_EXT:
          j++;
          break;
      }
    }
    if (mpcoff) {
      /* corrective offsets for mov PC-relative addressing */
      mcvoff = mov_val_offs[j];
      mpcoff = mov_pc_offs[j];
      if (j < 2) {
        /* swap operands to make sure indexed addressing is processed first */
        op = ip->op[0];
        ip->op[0] = ip->op[1];
        ip->op[1] = op;
        if (j==0 && ip->op[0]->mode==AM_IMM16) {
          mcvoff <<= 1;  /* double offsets for 16-bit immediate */
          mpcoff <<= 1;
        }
      }
    }
  }
  else {
    /* determine opcode type from operands */
    for (i=0; i<MAX_OPERANDS; i++) {
      if (ip->op[i] == NULL)
        break;
      switch (ip->op[i]->mode) {
        case AM_DIR:
          if (ocidx >= 0) ierror(0);
          ocidx = OCDIR;
          break;
        case AM_ADDR:  /* treat as EXT, while unknown */
        case AM_EXT:
          if (ocidx >= 0) ierror(0);
          ocidx = (ip->op[i]->flags & AF_INDIR) ? OCIDX : OCEXT;
          break;
        case AM_COFFS:
        case AM_NOFFS:
        case AM_ROFFS:
          if (ocidx >= 0) ierror(0);
          ocidx = OCIDX;
          break;
      }
    }
  }
  if (ocidx < 0)
    ocidx = OCSTD;
  pc += (mnemonics[ip->code].ext.opcode[ocidx] > 0xff ? 2 : 1);

  /* optimize and get operand sizes */
  for (i=0; i<MAX_OPERANDS; i++) {
    if ((op = ip->op[i]) == NULL)
      break;

    switch (op->mode) {
      case AM_ADDR:
        /* convert to either AM_DIR or AM_EXT, depending on address size */
        if ((!op->base && (utaddr)op->curval>=(ip->ext.dp<<8) &&
            (utaddr)op->curval<=(ip->ext.dp<<8)+0xff) ||
            (op->base && (op->base->flags&DPAGESYM))) {
          op->curval &= 0xff;
          op->mode = AM_DIR;
          ocidx = OCDIR;
          pc++;
          break;
        }
        else
          op->mode = AM_EXT;  /* always EXT, when exact address is unknown */
      case AM_EXT:
        if (op->flags & AF_INDIR) {
          if (opt_pc && (mnemonics[ip->code].operand_type[i] & IND)) {
            /* [ext] -> [rel,PC] */
            ocidx = OCIDX;
            op->mode = AM_COFFS;
            op->opreg = RIDX_PC;
            goto do_coffs;
          }
          pc++;  /* postbyte for indirect */
        }
        else {
          if ((opt_bra || opt_pc) && *mnemonics[ip->code].name=='j') {
            /* jmp/jsr optimizations */
            if (!(op->base && is_pc_reloc(op->base,sec))) {
              /* try optimizing jmp/jsr EXT to bra/bsr REL8 */
              pcd = op->curval - (pc + 2);
              if (pcd>=-129 && pcd<=127) {
                ip->code = mnemonics[ip->code].name[1]=='s' ? OC_BSR : OC_BRA;
                ocidx = OCSTD;
                op->mode = AM_REL8;
                op->flags |= AF_PC | AF_PCREL;
                pcd++;    /* adjust for EXT -> REL8 */
                goto rel_final;
              }
            }
            if (opt_pc) {
              /* jmp/jsr -> lbra/lbsr, even when slower/larger */
              int oc;

              if (oc = mnemonics[ip->code].name[1]=='s' ? OC_LBSR : OC_LBRA) {
                ip->code = oc;
                ocidx = OCSTD;
                op->mode = AM_REL16;
                if (cpu_type & HC12)
                  pc++;  /* LBRA opcode needs additional byte on HC12 */
                goto do_rel;
              }
            }
          }
          if (opt_pc && op->base!=NULL &&
              (mnemonics[ip->code].operand_type[i] & DIX)) {
            /* ext -> rel,PC */
            ocidx = OCIDX;
            op->mode = AM_COFFS;
            op->opreg = RIDX_PC;
            goto do_coffs;
          }
        }
        pc++;
      case AM_DIR:
        pc++;
        break;

      case AM_TFR:
        if (i != 0)  /* only the first TFR operand generates a byte */
          break;
      case AM_NOFFS:
        if ((cpu_type & HC12) && (op->flags & AF_INDIR)) {
          /* [,r] -> [0,r] is always 16 bit on the HC12 */
          op->flags |= AF_OFF16;
          pc += 2;
        }
      case AM_ROFFS:
        check_opreg(op,final);
      case AM_IMM8:
      case AM_REGXB:
      case AM_BITR:
        pc++;
      case AM_BITN:
      case AM_DBR:
        break;

      case AM_IMM16:
        pc += 2;
        break;

      case AM_IMM32:
        pc += 4;
        break;

      case AM_COFFS:
        /* indexed with constant offset - determine optimal offset size */
        do_coffs:
        op->flags &= ~AF_COSIZ;
        pc++;  /* the postbyte is always there */

        if (i) {
          mcvoff = -mcvoff;  /* a second postbyte would negate this offset */
          mpcoff = 0;        /* a second postbyte invalidates this offset */
        }

        check_opreg(op,final);

        if (registers[op->opreg].value == REG_PC) {
          /* PC-relative */
          int secrel = (sec->flags & ABSOLUTE) ? op->base==NULL :
              op->base!=NULL && LOCREF(op->base) && op->base->sec==sec;

          op->flags |= AF_PC;
          if (secrel) {
            /* calculate PC-relative distance to known label/address,
               assume worst case of 16-bit distance at first */
            pcd = (op->curval + mcvoff) - (pc + 2 + mpcoff);
            op->flags |= AF_PCREL;
          }
          else
            pcd = op->curval + mcvoff;

          if (final && (pcd>0xffff || pcd<-0x8000))
            cpu_error(3,(long)pcd);  /* pc-relative offset out of range */

          if (op->base && is_pc_reloc(op->base,sec)) {
            /* external symbol or label from different section usually
               needs 16 bits, except there was a '<' size selector */
            if (cpu_type & HC12)
              op->flags |= (op->flags&AF_LO) ? AF_OFF9 : AF_OFF16;  /* @@@ */
            else
              op->flags |= (op->flags&AF_LO) ? AF_OFF8 : AF_OFF16;
          }
          else {
            if (cpu_type & HC12) {
              if (!(op->flags & AF_INDIR) && pcd>=-18 && pcd<=15) {
                if (secrel)
                  pcd = (op->curval + mcvoff) - (pc + mpcoff);
                op->flags |= AF_OFF5;
              }
              else if (!(op->flags & AF_INDIR) && pcd>=-257 && pcd<=255) {
                if (secrel)
                  pcd = op->curval - (pc + 1);
                op->flags |= AF_OFF9;
              }
              else
                op->flags |= AF_OFF16;
            }
            else {  /* 6809/6309 */
              if (op->flags & AF_LO) {
                if (secrel)
                  pcd = op->curval - (pc + 1);
                op->flags |= AF_OFF8;
                if (final && (pcd<-128 || pcd>127))
                  cpu_error(3,(long)pcd);  /* pc-rel. offset out of range */
              }
              else if (!(op->flags&AF_HI) && pcd>=-129 && pcd<=127) {
                if (secrel)
                  pcd = op->curval - (pc + 1);
                op->flags |= AF_OFF8;
              }
              else
                op->flags |= AF_OFF16;
            }
          }
          op->curval = pcd;  /* update curval */
        }

        else {
          /* constant offset */
          taddr val = bf_sign_extend(op->curval,16);

          if (final && (val>0xffff || val<-0x8000))
            cpu_error(4,(long)val);  /* constant offset out of range */

          if (cpu_type & HC12) {
            if (op->preinc || op->postinc) {
              if (final && (val<1 || val>8))
                cpu_error(5,(int)val);  /* bad auto decr./incr. value */
              if (final && (op->flags & AF_INDIR)) {
                cpu_error(6);  /* indirect addressing not allowed */
                op->flags &= ~AF_INDIR;
              }
            }
            else if (!(op->flags & AF_INDIR) && val>=-16 && val<=15)
              op->flags |= AF_OFF5;
            else if (!(op->flags & AF_INDIR) && val>=-256 && val<=255)
              op->flags |= AF_OFF9;
            else
              op->flags |= AF_OFF16;
          }
          else {  /* 6809/6309 */
            int wreg = registers[op->opreg].value == REG_W;
            int abs = op->base == NULL;

            if (op->preinc || op->postinc) {
              if (final)
                cpu_error(7);  /* auto increment/decrement not allowed */
              op->preinc = op->postinc = 0;
            }

            if (opt_off && val==0 && abs) {
              /* 0,R -> ,R */
              op->mode = AM_NOFFS;
              if (final)
                free_expr(op->value);
              op->value = NULL;
            }
            else if ((op->flags & AF_LO) && !wreg) {
              op->flags |= AF_OFF8;
              if (final && (val<-128 || val>127))
                cpu_error(4,(long)val);  /* constant offset out of range */
            }
            else if (op->flags & AF_HI)
              op->flags |= AF_OFF16;
            else if (!(op->flags & AF_INDIR) &&
                     abs && val>=-16 && val<=15 && !wreg)
              op->flags |= AF_OFF5;
            else if (abs && val>=-128 && val<=127 && !wreg)
              op->flags |= AF_OFF8;
            else
              op->flags |= AF_OFF16;
          }
        }

        pc += coffs_size[op->flags & AF_COSIZ];
        break;

      case AM_REL8:
      case AM_REL9:
      case AM_REL16:
      do_rel:
        if (op->base && is_pc_reloc(op->base,sec)) {
          pcd = op->curval;  /* reloc addend */
          op->flags |= AF_PC;
        }
        else {
          pcd = op->curval - (pc + rel_size[op->mode-AM_REL8]);
          op->flags |= AF_PC | AF_PCREL;
        }

        /* optimize/translate Bcc and LBcc instructions */
        if (opt_bra && i==0 && (op->flags & AF_PCREL)) {
          int ocsz_diff = pc - orig_pc;  /* init with current opcode size */

          switch (op->mode) {
            case AM_REL8:
              if ((pcd<-128 || pcd>127) &&
                  (mnemonics[ip->code+2].operand_type[0] & OTMASK) == RLL) {
                op->mode = AM_REL16;
                ip->code += 2;
                ocsz_diff = (mnemonics[ip->code].ext.opcode[OCSTD]>0xff? 2 : 1)
                            - ocsz_diff;
                pc += ocsz_diff;  /* LBcc opcode may be larger */
                pcd -= 1 + ocsz_diff;  /* adjust for LBcc and 16-bit branch */
              }
              break;
            case AM_REL16:
              if ((pcd>=-129 && pcd<=127) &&
                  (mnemonics[ip->code-2].operand_type[0] & OTMASK) == RLS) {
                op->mode = AM_REL8;
                ip->code -= 2;
                ocsz_diff = 1 - ocsz_diff;
                pc += ocsz_diff;  /* Bcc opcode may be smaller */
                pcd += 1 - ocsz_diff;  /* adjust for Bcc and 8-bit branch */
              }
              break;
          }
        }

        rel_final:
        if (final) {
          switch (op->mode) {
            case AM_REL8:
              if (pcd<-128 || pcd>127)
                cpu_error(8,(long)pcd);  /* short branch out of range */
              break;
            case AM_REL9:
              if (pcd<-256 || pcd>255)
                cpu_error(9,(long)pcd);  /* decrement branch out of range */
              break;
            case AM_REL16:
              if (pcd<-0x8000 || pcd>0xffff)
                cpu_error(10,(long)pcd);  /* long branch out of range */
              break;
          }
        }
        op->curval = pcd;
        pc += rel_size[op->mode-AM_REL8];
        break;

      default:
        ierror(0);
        break;
    }
  }

  ip->ext.ocidx = ocidx;
  return pc - orig_pc;
}


size_t instruction_size(instruction *ip,section *sec,taddr pc)
{
  return process_instruction(copy_inst(ip),sec,pc,0);
}


dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  operand *op;
  int offs = 1;
  rlist *rl;
  int16_t oc;
  uint8_t *d;
  taddr val;
  int i,ocsz;

  /* execute all optimizations for real and determine final size */
  db->size = process_instruction(ip,sec,pc,1);
  d = db->data = mymalloc(db->size);

  /* write one or two opcode bytes */
  oc = mnemonics[ip->code].ext.opcode[ip->ext.ocidx];
  if (oc == NA) {
    cpu_error(18);  /* addressing mode not supported */
    return db;
  }
  if (oc > 0xff) {
    *d++ = oc >> 8;
    offs++;
  }
  *d++ = oc;
  ocsz = offs;

  for (i=0; i<MAX_OPERANDS; i++) {
    if ((op = ip->op[i]) == NULL)
      break;
    val = op->curval;

    if (op->base!=NULL && (op->flags & AF_PC)) {
      switch (op->mode) {
        case AM_IMM8:
        case AM_IMM16:
        case AM_IMM32:
          /* late fix for BASE_PCREL, taking reloc-position into account */
          val += offs;
          break;
      }
    }

    switch (op->mode) {
      case AM_DIR:
      case AM_IMM8:
        if (op->base) {
          rl = add_extnreloc(&db->relocs,op->base,val,
                             (op->flags&AF_PC)?REL_PC:REL_ABS,0,8,offs);
          if (op->flags & AF_LO) {
            ((nreloc *)rl->reloc)->mask = 0xff;
            val &= 0xff;
          }
          else if (op->flags & AF_HI) {
            ((nreloc *)rl->reloc)->mask = 0xff00;
            val = (val >> 8) & 0xff;
          }
        }
        if (val<-0x80 || val>0xff)
          cpu_error(15,8);  /* immediate expression doesn't fit */
        *d++ = val;
        offs++;
        break;

      case AM_REL9:
        if (op->base && is_pc_reloc(op->base,sec)) {
          val -= 2;  /* reloc addend adjustment */
          add_extnreloc_masked(&db->relocs,op->base,val,REL_PC,
                               3,1,offs-1,0x100);
          add_extnreloc_masked(&db->relocs,op->base,val,REL_PC,
                               8,8,offs-1,0xff);
        }
        if (val < 0)
          *(db->data+1) |= 0x10;  /* set sign-bit in extbyte */
        *d++ = val;
        offs++;
        break;

      case AM_REL8:
        if (op->base && is_pc_reloc(op->base,sec)) {
          val--;  /* reloc addend adjustment */
          add_extnreloc(&db->relocs,op->base,val,REL_PC,0,8,offs);
        }
        *d++ = val;
        offs++;
        break;

      case AM_REL16:
        if (op->base && is_pc_reloc(op->base,sec)) {
          val -= 2;  /* reloc addend adjustment */
          add_extnreloc(&db->relocs,op->base,val,REL_PC,0,16,offs);
        }
        *d++ = val >> 8;
        *d++ = val;
        offs += 2;
        break;

      case AM_REGXB:
        *d++ = op->curval;
        offs++;
        break;

      case AM_TFR:
        if (offs == ocsz) {
          *d++ = (cpu_type & HC12) ?
                 ir_postbyte_map12[registers[op->opreg].value]<<4 :
                 ir_postbyte_map09[registers[op->opreg].value]<<4;
          offs++;
        }
        else {
          *(d-1) |= (cpu_type & HC12) ?
                    ir_postbyte_map12[registers[op->opreg].value] :
                    ir_postbyte_map09[registers[op->opreg].value];
        }
        break;

      case AM_BITR:
        *d++ = bitm_postbyte_map[registers[op->opreg].value] << 6;
        offs++;
        break;

      case AM_BITN:
        if (val<0 || val>7)
          cpu_error(13,(int)val);  /* illegal bit number */

        if (offs==ocsz+1 && ip->op[0]->mode==AM_BITR) {
          if (op->base) {
            if (op->flags & (AF_HI|AF_LO))
              general_error(38);  /* illegal relocation */
            else
              add_extnreloc(&db->relocs,op->base,val,REL_ABS,
                            i==1?2:5,3,offs-1);
          }
          switch (i) {
            case 1:
              *(d-1) |= (val&7) << 3;
              break;
            case 2:
              *(d-1) |= (val&7);
              break;
            default:
              ierror(0);
              break;
          }
        }
        else
          ierror(0);
        break;

      case AM_DBR:
        *(db->data+1) |= dbr_postbyte_map[registers[op->opreg].value];
        break;

      case AM_EXT:
        if (op->flags & AF_INDIR) {
          *d++ = 0x9f;  /* [ext] needs postbyte */
          offs++;
        }
      case AM_IMM16:
        if (op->base)
          add_extnreloc(&db->relocs,op->base,val,
                        (op->flags&AF_PC)?REL_PC:REL_ABS,0,16,offs);
        if (val<-0x8000 || val>0xffff)
          cpu_error(15,16);  /*immediate expression doesn't fit */
        *d++ = val >> 8;
        *d++ = val;
        offs += 2;
        break;

      case AM_IMM32:
        if (op->base)
          add_extnreloc(&db->relocs,op->base,val,
                        (op->flags&AF_PC)?REL_PC:REL_ABS,0,32,offs);
        *d++ = val >> 24;
        *d++ = val >> 16;
        *d++ = val >> 8;
        *d++ = val;
        offs += 4;
        break;

      case AM_NOFFS:
        if (cpu_type & HC12) {
          /* this addressing mode does not exist on the HC12 */
          cpu_error(14);  /* omitted offset taken as 5-bit zero offset */
          op->mode = AM_COFFS;
          op->base = NULL;
          val = 0;
          goto gen_coffs;
        }
        else {  /* 6809/6309 */
          if (registers[op->opreg].value == REG_W) {
            *d = (op->flags & AF_INDIR) ? 0x90 : 0x8f;
            if (op->postinc)
              *d |= 0x40;
            else if (op->preinc)
              *d |= 0x60;
          }
          else {
            *d = idx_postbyte_map09[registers[op->opreg].value] |
                 ((op->flags & AF_INDIR) ? 0x90 : 0x80);
            if (op->preinc < 0)
              *d |= (-op->preinc) + 1;
            else if (op->postinc > 0)
              *d |= op->postinc - 1;
            else
              *d |= 4;
          }
        }
        d++;
        offs++;
        break;

      case AM_ROFFS:
        if (cpu_type & HC12) {
          *d++ = 0xe4 | (idx_postbyte_map12[registers[op->opreg].value]<<3) |
                 ((op->flags & AF_INDIR) ? 0x03 :
                  offs_postbyte_map12[registers[op->offs_reg].value]);
        }
        else {   /* 6809/6309 */
          *d++ = idx_postbyte_map09[registers[op->opreg].value] |
                 offs_postbyte_map09[registers[op->offs_reg].value] |
                 ((op->flags & AF_INDIR) ? 0x90 : 0x80);
        }
        offs++;
        break;

      case AM_COFFS:
      gen_coffs:
        {
          static int boff[4] = { 3,0,7,0 };
          static int bsiz[4] = { 5,8,9,16 };
          static int offa[4] = { 0,1,0,1 };
          int cosz = op->flags & AF_COSIZ;

          if (cpu_type & HC12) {
            if (!(op->flags & AF_INDIR)) {
              if (op->preinc || op->postinc) {
                /* n,-r n,+r n,r- n,r+ */
                uint8_t xb = 0x20;
                int p;

                if (op->base) {
                  general_error(38);  /* illegal relocation */
                  op->base = NULL;
                }
                if (p = op->postinc)
                  xb = 0x30;
                else
                  p = op->preinc;
                if (p < 0)
                  val = -val & 15;
                else
                  val--;
                *d = xb | (idx_postbyte_map12[registers[op->opreg].value]<<6);
              }
              else {
                /* n,r -n,r (5, 9, 16 bits) */
                if (cosz == AF_OFF5)
                  *d = idx_postbyte_map12[registers[op->opreg].value] << 6;
                else
                  *d = (cosz==AF_OFF16 ? 0xe2 : 0xe0) |
                       (idx_postbyte_map12[registers[op->opreg].value] << 3);
              }
            }
            else
              *d = 0xe3 | (idx_postbyte_map12[registers[op->opreg].value]<<3);
          }
          else {  /* 6809/6309 */
            if (registers[op->opreg].value == REG_W) {
              *d = (op->flags & AF_INDIR) ? 0xb0 : 0xaf;
            }
            else {
              if (registers[op->opreg].value == REG_PC) {
                *d = (op->flags & AF_INDIR) ? 0x9c : 0x8c;
              }
              else {
                *d = idx_postbyte_map09[registers[op->opreg].value];              
                if (cosz != AF_OFF5)
                  *d |= (op->flags & AF_INDIR) ? 0x98 : 0x88;
              }
              *d |= cosz == AF_OFF16;
            }
          }

          if (op->base!=NULL &&
              (!(op->flags & AF_PC) || is_pc_reloc(op->base,sec))) {
            int rtyp = (op->flags & AF_PC) ? REL_PC : REL_ABS;

            if (rtyp == REL_PC)
              val -= cosz==AF_OFF16 ? 2 : 1;  /* fix addend for reloc */
            add_extnreloc(&db->relocs,op->base,val,rtyp,
                          boff[cosz],bsiz[cosz],offs+offa[cosz]);
          }
          switch (cosz) {
            case AF_OFF5:
              *d |= val & 0x1f;
              d++;
              offs++;
              break;
            case AF_OFF8:
              d++;
              *d++ = val;
              offs += 2;
              break;
            case AF_OFF9:
              *d++ |= (val >> 8) & 1;
              *d++ = val;
              offs += 2;
              break;
            case AF_OFF16:
              d++;
              *d++ = val >> 8;
              *d++ = val;
              offs += 3;
              break;
          }
        }
        break;

      default:
        ierror(0);
        break;
    }
  }
  if (offs != db->size)
    ierror(0);

  return db;
}


dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  taddr val;

  if (bitsize!=8 && bitsize!=16 && bitsize!=32)
    cpu_error(11,bitsize);  /* data size not supported */

  db->size = bitsize >> 3;
  db->data = mymalloc(db->size);

  if (!eval_expr(op->value,&val,sec,pc)) {
    symbol *base;
    int btype;
    rlist *rl;
    
    modifier = 0;
    btype = find_base(op->value,&base,sec,pc);
    if (btype==BASE_OK || (btype==BASE_PCREL && modifier==0)) {
      rl = add_extnreloc(&db->relocs,base,val,
                         btype==BASE_PCREL?REL_PC:REL_ABS,0,bitsize,0);
      switch (modifier) {
        case LOBYTE:
          if (rl)
            ((nreloc *)rl->reloc)->mask = 0xff;
          val = val & 0xff;
          break;
        case HIBYTE:
          if (rl)
            ((nreloc *)rl->reloc)->mask = 0xff00;
          val = (val >> 8) & 0xff;
          break;
      }
    }
    else if (btype != BASE_NONE)
      general_error(38);  /* illegal relocation */
  }

  if (bitsize < 16) {
    if (val<-0x80 || val>0xff)
      cpu_error(12,8);   /* data doesn't fit into 8-bits */
  } else if (bitsize < 32) {
    if (val<-0x8000 || val>0xffff)
      cpu_error(12,16);  /* data doesn't fit into 16-bits */
  }

  setval(1,db->data,db->size,val);
  return db;
}


int cpu_available(int idx)
{
  return (mnemonics[idx].ext.flags & cpu_type) != 0;
}


int init_cpu()
{
  int i;

  for (i=0; i<mnemonic_cnt; i++) {
    if (mnemonics[i].ext.flags & cpu_type) {
      if (!strcmp(mnemonics[i].name,"bra"))
        OC_BRA = i;
      else if (!strcmp(mnemonics[i].name,"bsr"))
        OC_BSR = i;
      if (!strcmp(mnemonics[i].name,"lbra"))
        OC_LBRA = i;
      else if (!strcmp(mnemonics[i].name,"lbsr"))
        OC_LBSR = i;
    }
  }

  for (i=0; i<reg_cnt; i++) {
    if (registers[i].cpu & cpu_type) {
      if (!strcmp(registers[i].name,"PC"))
        RIDX_PC = i;
    }
  }

  return 1;
}


int cpu_args(char *p)
{
  if (!strcmp(p,"-6809"))
    cpu_type = M6809;
  else if (!strcmp(p,"-6309"))
    cpu_type = HD6309;
  else if (!stricmp(p,"-68hc12"))
    cpu_type = HC12;
  else if (!strcmp(p,"-opt-offset"))
    opt_off = 1;
  else if (!strcmp(p,"-opt-branch"))
    opt_bra = 1;
  else if (!strcmp(p,"-opt-pc"))
    opt_pc = 1;
  else
    return 0;

  return 1;
}
