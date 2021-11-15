/*
** cpu.c PowerPC cpu-description file
** (c) in 2002-2019 by Frank Wille
*/

#include "vasm.h"
#include "operands.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};

int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);

char *cpu_copyright="vasm PowerPC cpu backend 3.1 (c) 2002-2019 Frank Wille";
char *cpuname = "PowerPC";
int bitsperbyte = 8;
int bytespertaddr = 4;
int ppc_endianess = 1;

static uint64_t cpu_type = CPU_TYPE_PPC | CPU_TYPE_ALTIVEC | CPU_TYPE_32 | CPU_TYPE_ANY;
static int regnames = 1;
static taddr sdreg = 13;  /* this is default for V.4, PowerOpen = 2 */
static taddr sd2reg = 2;
static unsigned char opt_branch = 0;



int ppc_data_align(int n)
{
  if (n<=8) return 1;
  if (n<=16) return 2;
  if (n<=32) return 4;
  return 8;
}


int ppc_data_operand(int n)
{
  if (n&OPSZ_FLOAT) return OPSZ_BITS(n)>32?OP_F64:OP_F32;
  if (OPSZ_BITS(n)<=8) return OP_D8;
  if (OPSZ_BITS(n)<=16) return OP_D16;
  if (OPSZ_BITS(n)<=32) return OP_D32;
  return OP_D64;
}


int ppc_operand_optional(operand *op,int type)
{
  if (powerpc_operands[type].flags & OPER_OPTIONAL) {
    op->attr = REL_NONE;
    op->mode = OPM_NONE;
    op->basereg = NULL;
    op->value = number_expr(0);  /* default value 0 */

    if (powerpc_operands[type].flags & OPER_NEXT)
      op->type = NEXT;
    else
      op->type = type;
    return 1;
  }
  else if (powerpc_operands[type].flags & OPER_FAKE) {
    op->type = type;
    op->value = NULL;
    return 1;
  }

  return 0;
}


int ppc_available(int idx)
/* Check if mnemonic is available for selected cpu_type. */
{
  uint64_t avail = mnemonics[idx].ext.available;
  uint64_t datawidth = CPU_TYPE_32 | CPU_TYPE_64;

  if ((avail & cpu_type) != 0) {
    if ((avail & cpu_type & ~datawidth)!=0 || (cpu_type & CPU_TYPE_ANY)!=0) {
      if (avail & datawidth)
        return (avail & datawidth) == (cpu_type & datawidth)
               || (cpu_type & CPU_TYPE_64_BRIDGE) != 0;
      else
        return 1;
    }
  }
  return 0;
}


static char *parse_reloc_attr(char *p,operand *op)
{
  p = skip(p);
  while (*p == '@') {
    unsigned char chk;

    p++;
    chk = op->attr;
    if (!strncmp(p,"got",3)) {
      op->attr = REL_GOT;
      p += 3;
    }
    else if (!strncmp(p,"plt",3)) {
      op->attr = REL_PLT;
      p += 3;
    }
    else if (!strncmp(p,"sdax",4)) {
      op->attr = REL_SD;
      p += 4;
    }
    else if (!strncmp(p,"sdarx",5)) {
      op->attr = REL_SD;
      p += 5;
    }
    else if (!strncmp(p,"sdarel",6)) {
      op->attr = REL_SD;
      p += 6;
    }
    else if (!strncmp(p,"sectoff",7)) {
      op->attr = REL_SECOFF;
      p += 7;
    }
    else if (!strncmp(p,"local",5)) {
      op->attr = REL_LOCALPC;
      p += 5;
    }
    else if (!strncmp(p,"globdat",7)) {
      op->attr = REL_GLOBDAT;
      p += 7;
    }
    else if (!strncmp(p,"sda2rel",7)) {
      op->attr = REL_PPCEABI_SDA2;
      p += 7;
    }
    else if (!strncmp(p,"sda21",5)) {
      op->attr = REL_PPCEABI_SDA21;
      p += 5;
    }
    else if (!strncmp(p,"sdai16",6)) {
      op->attr = REL_PPCEABI_SDAI16;
      p += 6;
    }
    else if (!strncmp(p,"sda2i16",7)) {
      op->attr = REL_PPCEABI_SDA2I16;
      p += 7;
    }
    else if (!strncmp(p,"drel",4)) {
      op->attr = REL_MORPHOS_DREL;
      p += 4;
    }
    else if (!strncmp(p,"brel",4)) {
      op->attr = REL_AMIGAOS_BREL;
      p += 4;
    }
    if (chk!=REL_NONE && chk!=op->attr)
      cpu_error(7);  /* multiple relocation attributes */

    chk = op->mode;
    if (!strncmp(p,"ha",2)) {
      op->mode = OPM_HA;
      p += 2;
    }
    if (*p == 'h') {
      op->mode = OPM_HI;
      p++;
    }
    if (*p == 'l') {
      op->mode = OPM_LO;
      p++;
    }
    if (chk!=OPM_NONE && chk!=op->mode)
      cpu_error(8);  /* multiple hi/lo modifiers */
  }

  return p;
}


int parse_operand(char *p,int len,operand *op,int optype)
/* Parses operands, reads expressions and assigns relocation types. */
{
  char *start = p;
  int rc = PO_MATCH;

  op->attr = REL_NONE;
  op->mode = OPM_NONE;
  op->basereg = NULL;

  p = skip(p);
  op->value = OP_FLOAT(optype) ? parse_expr_float(&p) : parse_expr(&p);

  if (!OP_DATA(optype)) {
    p = parse_reloc_attr(p,op);
    p = skip(p);

    if (p-start < len && *p=='(') {
      /* parse d(Rn) load/store addressing mode */
      if (powerpc_operands[optype].flags & OPER_PARENS) {
        p++;
        op->basereg = parse_expr(&p);
        p = skip(p);
        if (*p == ')') {
          p = skip(p+1);
          rc = PO_SKIP;
        }
        else {
          cpu_error(5);  /* missing closing parenthesis */
          rc = PO_CORRUPT;
          goto leave;
        }
      }
      else {
        cpu_error(4);  /* illegal operand type */
        rc = PO_CORRUPT;
        goto leave;
      }
    }
  }

  if (p-start < len)
    cpu_error(3);  /* trailing garbage in operand */
leave:
  op->type = optype;
  return rc;
}


static taddr read_sdreg(char **s,taddr def)
{
  expr *tree;
  taddr val = def;

  *s = skip(*s);
  tree = parse_expr(s);
  simplify_expr(tree);
  if (tree->type==NUM && tree->c.val>=0 && tree->c.val<=31)
    val = tree->c.val;
  else
    cpu_error(13);  /* not a valid register */
  free_expr(tree);
  return val;
}


char *parse_cpu_special(char *start)
/* parse cpu-specific directives; return pointer to end of
   cpu-specific text */
{
  char *name=start,*s=start;

  if (ISIDSTART(*s)) {
    s++;
    while (ISIDCHAR(*s))
      s++;
    if (dotdirs && *name=='.')
      name++;
    if (s-name==5 && !strncmp(name,"sdreg",5)) {
      sdreg = read_sdreg(&s,sdreg);
      return s;
    }
    else if (s-name==6 && !strncmp(name,"sd2reg",6)) {
      sd2reg = read_sdreg(&s,sd2reg);
      return s;
    }
  }
  return start;
}


static int get_reloc_type(operand *op)
{
  int rtype = REL_NONE;

  if (OP_DATA(op->type)) {  /* data relocs */
    return REL_ABS;
  }

  else {  /* handle instruction relocs */
    const struct powerpc_operand *ppcop = &powerpc_operands[op->type];

    if (ppcop->shift == 0) {
      if (ppcop->bits == 16 || ppcop->bits == 26) {

        if (ppcop->flags & OPER_RELATIVE) {  /* a relative branch */
          switch (op->attr) {
            case REL_NONE:
              rtype = REL_PC;
              break;
            case REL_PLT:
              rtype = REL_PLTPC;
              break;
            case REL_LOCALPC:
              rtype = REL_LOCALPC;
              break;
            default:
              cpu_error(11); /* reloc attribute not supported by operand */
              break;
          }
        }

        else if (ppcop->flags & OPER_ABSOLUTE) { /* absolute branch */
          switch (op->attr) {
            case REL_NONE:
              rtype = REL_ABS;
              break;
            case REL_PLT:
            case REL_GLOBDAT:
            case REL_SECOFF:
              rtype = op->attr;
              break;
            default:
              cpu_error(11); /* reloc attribute not supported by operand */
              break;
          }
        }

        else {  /* immediate 16 bit or load/store d16(Rn) instruction */
          switch (op->attr) {
            case REL_NONE:
              rtype = REL_ABS;
              break;
            case REL_GOT:
            case REL_PLT:
            case REL_SD:
            case REL_PPCEABI_SDA2:
            case REL_PPCEABI_SDA21:
            case REL_PPCEABI_SDAI16:
            case REL_PPCEABI_SDA2I16:
            case REL_MORPHOS_DREL:
            case REL_AMIGAOS_BREL:
              rtype = op->attr;
              break;
            default:
              cpu_error(11); /* reloc attribute not supported by operand */
              break;
          }
        }
      }
    }
  }

  return rtype;
}


static int valid_hiloreloc(int type)
/* checks if this relocation type allows a @l/@h/@ha modifier */
{
  switch (type) {
    case REL_ABS:
    case REL_GOT:
    case REL_PLT:
    case REL_MORPHOS_DREL:
    case REL_AMIGAOS_BREL:
      return 1;
  }
  cpu_error(6);  /* relocation does not allow hi/lo modifier */
  return 0;
}


static taddr make_reloc(int reloctype,operand *op,section *sec,
                        taddr pc,rlist **reloclist)
/* create a reloc-entry when operand contains a non-constant expression */
{
  taddr val;

  if (!eval_expr(op->value,&val,sec,pc)) {
    /* non-constant expression requires a relocation entry */
    symbol *base;
    int btype,pos,size,offset;
    taddr addend,mask;

    btype = find_base(op->value,&base,sec,pc);
    pos = offset = 0;

    if (btype > BASE_ILLEGAL) {
      if (btype == BASE_PCREL) {
        if (reloctype == REL_ABS)
          reloctype = REL_PC;
        else
          goto illreloc;
      }

      if (op->mode != OPM_NONE) {
        /* check if reloc allows @ha/@h/@l */
        if (!valid_hiloreloc(reloctype))
          op->mode = OPM_NONE;
      }

      if (reloctype == REL_PC && !is_pc_reloc(base,sec)) {
        /* a relative branch - reloc is only needed for external reference */
        return val-pc;
      }

      /* determine reloc size, offset and mask */
      if (OP_DATA(op->type)) {  /* data operand */
        switch (op->type) {
          case OP_D8:
            size = 8;
            break;
          case OP_D16:
            size = 16;
            break;
          case OP_D32:
          case OP_F32:
            size = 32;
            break;
          case OP_D64:
          case OP_F64:
            size = 64;
            break;
          default:
            ierror(0);
            break;
        }
        addend = val;
        mask = -1;
      }
      else {  /* instruction operand */
        const struct powerpc_operand *ppcop = &powerpc_operands[op->type];

        if (ppcop->flags & (OPER_RELATIVE|OPER_ABSOLUTE)) {
          /* branch instruction */
          if (ppcop->bits == 26) {
            size = 24;
            pos = 6;
            mask = 0x3fffffc;
          }
          else {
            size = 14;
            offset = 2;
            mask = 0xfffc;
          }
          addend = (btype == BASE_PCREL) ? val + offset : val;
        }
        else {
          /* load/store or immediate */
          size = 16;
          offset = 2;
          addend = (btype == BASE_PCREL) ? val + offset : val;
          switch (op->mode) {
            case OPM_LO:
              mask = 0xffff;
              break;
            case OPM_HI:
              mask = 0xffff0000;
              break;
            case OPM_HA:
              add_extnreloc_masked(reloclist,base,addend,reloctype,
                                   pos,size,offset,0x8000);
              mask = 0xffff0000;
              break;
            default:
              mask = -1;
              break;
          }
        }
      }

      add_extnreloc_masked(reloclist,base,addend,reloctype,
                           pos,size,offset,mask);
    }
    else if (btype != BASE_NONE) {
illreloc:
      general_error(38);  /* illegal relocation */
    }
  }
  else {
     if (reloctype == REL_PC) {
       /* a relative reference to an absolute label */
       return val-pc;
     }
  }

  return val;
}


static void fix_reloctype(dblock *db,int rtype)
{
  rlist *rl;

  for (rl=db->relocs; rl!=NULL; rl=rl->next)
    rl->type = rtype;
}


static void range_check(taddr val,const struct powerpc_operand *o,dblock *db)
/* checks if a value fits the allowed range for this operand field */
{
  int32_t v = (int32_t)val;
  int32_t minv = 0;
  int32_t maxv = (1L << o->bits) - 1;
  int force_signopt = 0;

  if (db) {
    if (db->relocs) {
      switch (db->relocs->type) {
        case REL_SD:
        case REL_PPCEABI_SDA2:
        case REL_PPCEABI_SDA21:
          force_signopt = 1;  /* relocation allows full positive range */
          break;
      }
    }
  }

  if (o->flags & OPER_SIGNED) {
    minv = ~(maxv >> 1);

    /* @@@ Only recognize this flag in 32-bit mode! Don't care for now */
    if (!(o->flags & OPER_SIGNOPT) && !force_signopt)
      maxv >>= 1;
  }
  if (o->flags & OPER_NEGATIVE)
    v = -v;

  if (v<minv || v>maxv)
    cpu_error(12,v,minv,maxv);  /* operand out of range */
}


static void negate_bo_cond(uint32_t *p)
/* negates all conditions in a branch instruction's BO field */
{
  if (!(*p & 0x02000000))
    *p ^= 0x01000000;
  if (!(*p & 0x00800000))
    *p ^= 0x00400000;
}


static uint32_t insertcode(uint32_t i,taddr val,
                           const struct powerpc_operand *o)
{
  if (o->insert) {
    const char *errmsg = NULL;

    i = (o->insert)(i,(int32_t)val,&errmsg);
    if (errmsg)
      cpu_error(0,errmsg);
  }
  else
    i |= ((int32_t)val & ((1<<o->bits)-1)) << o->shift;

  return i;
}


size_t eval_operands(instruction *ip,section *sec,taddr pc,
                     uint32_t *insn,dblock *db)
/* evaluate expressions and try to optimize instruction,
   return size of instruction */
{
  mnemonic *mnemo = &mnemonics[ip->code];
  size_t isize = 4;
  int i;
  operand op;

  if (insn != NULL)
    *insn = mnemo->ext.opcode;

  for (i=0; i<MAX_OPERANDS && ip->op[i]!=NULL; i++) {
    const struct powerpc_operand *ppcop;
    int reloctype;
    taddr val;

    op = *(ip->op[i]);

    if (op.type == NEXT) {
      /* special case: operand omitted and use this operand's type + 1
         for the next operand */
      op = *(ip->op[++i]);
      op.type = mnemo->operand_type[i-1] + 1;
    }

    ppcop = &powerpc_operands[op.type];

    if (ppcop->flags & OPER_FAKE) {
      if (insn != NULL) {
        if (op.value != NULL)
          cpu_error(16);  /* ignoring fake operand */
        *insn = insertcode(*insn,0,ppcop);
      }
      continue;
    }

    if ((reloctype = get_reloc_type(&op)) != REL_NONE) {
      if (db != NULL) {
        val = make_reloc(reloctype,&op,sec,pc,&db->relocs);
      }
      else {
        if (!eval_expr(op.value,&val,sec,pc)) {
          if (reloctype == REL_PC)
            val -= pc;
        }
      }
    }
    else {
      if (!eval_expr(op.value,&val,sec,pc))
        if (insn != NULL)
          cpu_error(2);  /* constant integer expression required */
    }

    /* execute modifier on val */
    if (op.mode) {
      switch (op.mode) {
        case OPM_LO:
          val &= 0xffff;
          break;
        case OPM_HI:
          val = (val>>16) & 0xffff;
          break;
        case OPM_HA:
          val = ((val>>16) + ((val & 0x8000) ? 1 : 0) & 0xffff);
          break;
      }
      if ((ppcop->flags & OPER_SIGNED) && (val & 0x8000))
        val -= 0x10000;
    }

    /* do optimizations here: */

    if (opt_branch) {
      if (reloctype==REL_PC &&
          (op.type==BD || op.type==BDM || op.type==BDP)) {
        if (val<-0x8000 || val>0x7fff) {
          /* "B<cc>" branch destination out of range, convert into
             a "B<!cc> ; B" combination */
          if (insn != NULL) {
            negate_bo_cond(insn);
            *insn = insertcode(*insn,8,ppcop);  /* B<!cc> $+8 */
            insn++;
            *insn = B(18,0,0);  /* set B instruction opcode */
            val -= 4;
          }
          ppcop = &powerpc_operands[LI];  /* set oper. for B instruction */
          isize = 8;
        }
      }
    }

    if (ppcop->flags & OPER_PARENS) {
      if (op.basereg) {
        /* a load/store instruction d(Rn) carries basereg in current op */
        taddr reg;

        if (db!=NULL && op.mode==OPM_NONE && op.attr==REL_NONE) {
          if (eval_expr(op.basereg,&reg,sec,pc)) {
            if (reg == sdreg)  /* is it a small data reference? */
              fix_reloctype(db,REL_SD);
            else if (reg == sd2reg)  /* EABI small data 2 */
              fix_reloctype(db,REL_PPCEABI_SDA2);
          }
        }

        /* write displacement */
        if (insn != NULL) {
          range_check(val,ppcop,db);
          *insn = insertcode(*insn,val,ppcop);
        }

        /* move to next operand type to handle base register */
        op.type = mnemo->operand_type[++i];
        ppcop = &powerpc_operands[op.type];
        op.attr = REL_NONE;
        op.mode = OPM_NONE;
        op.value = op.basereg;
        if (!eval_expr(op.value,&val,sec,pc))
          if (insn != NULL)
            cpu_error(2);  /* constant integer expression required */
      }
      else if (insn != NULL)
        cpu_error(14);  /* missing base register */
    }

    /* write val (register, immediate, etc.) */
    if (insn != NULL) {
      range_check(val,ppcop,db);
      *insn = insertcode(*insn,val,ppcop);
    }
  }

  return isize;
}


size_t instruction_size(instruction *ip,section *sec,taddr pc)
/* Calculate the size of the current instruction; must be identical
   to the data created by eval_instruction. */
{
  /* determine optimized size, when needed */
  if (opt_branch)
    return eval_operands(ip,sec,pc,NULL,NULL);

  /* otherwise an instruction is always 4 bytes */
  return 4;
}


dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
/* Convert an instruction into a DATA atom including relocations,
   when necessary. */
{
  dblock *db = new_dblock();
  uint32_t insn[2];

  if (db->size = eval_operands(ip,sec,pc,insn,db)) {
    unsigned char *d = db->data = mymalloc(db->size);
    int i;

    for (i=0; i<db->size/4; i++)
      d = setval(1,d,4,insn[i]);
  }

  return db;
}


dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
/* Create a dblock (with relocs, if necessary) for size bits of data. */
{
  dblock *db = new_dblock();
  taddr val;
  tfloat flt;

  if ((bitsize & 7) || bitsize > 64)
    cpu_error(9,bitsize);  /* data size not supported */
  if (!OP_DATA(op->type))
    ierror(0);

  db->size = bitsize >> 3;
  db->data = mymalloc(db->size);

  if (type_of_expr(op->value) == FLT) {
    if (!eval_expr_float(op->value,&flt))
      general_error(60);  /* cannot evaluate floating point */

    switch (bitsize) {
      case 32:
        conv2ieee32(1,db->data,flt);
        break;
      case 64:
        conv2ieee64(1,db->data,flt);
        break;
      default:
        cpu_error(10);  /* data has illegal type */
        break;
    }
  }
  else {
    val = make_reloc(get_reloc_type(op),op,sec,pc,&db->relocs);

    switch (db->size) {
      case 1:
        db->data[0] = val & 0xff;
        break;
      case 2:
      case 4:
      case 8:
        setval(ppc_endianess,db->data,db->size,val);
        break;
      default:
        ierror(0);
        break;
    }
  }

  return db;
}


operand *new_operand()
{
  operand *new = mymalloc(sizeof(*new));
  new->type = -1;
  new->mode = OPM_NONE;
  return new;
}


static void define_regnames(void)
{
  char r[4];
  int i;

  for (i=0; i<32; i++) {
    sprintf(r,"r%d",i);
    set_internal_abs(r,(taddr)i);
    r[0] = 'f';
    set_internal_abs(r,(taddr)i);
    r[0] = 'v';
    set_internal_abs(r,(taddr)i);
  }
  for (i=0; i<8; i++) {
    sprintf(r,"cr%d",i);
    set_internal_abs(r,(taddr)i);
  }
  set_internal_abs("vrsave",256);
  set_internal_abs("lt",0);
  set_internal_abs("gt",1);
  set_internal_abs("eq",2);
  set_internal_abs("so",3);
  set_internal_abs("un",3);
  set_internal_abs("sp",1);
  set_internal_abs("rtoc",2);
  set_internal_abs("fp",31);
  set_internal_abs("fpscr",0);
  set_internal_abs("xer",1);
  set_internal_abs("lr",8);
  set_internal_abs("ctr",9);
}


int init_cpu()
{
  if (regnames)
    define_regnames();
  return 1;
}


int cpu_args(char *p)
{
  int i;

  if (!strncmp(p,"-m",2)) {
    p += 2;
    if (!strcmp(p,"pwrx") || !strcmp(p,"pwr2"))
      cpu_type = CPU_TYPE_POWER | CPU_TYPE_POWER2 | CPU_TYPE_32;
    else if (!strcmp(p,"pwr"))
      cpu_type = CPU_TYPE_POWER | CPU_TYPE_32;
    else if (!strcmp(p,"601"))
      cpu_type = CPU_TYPE_601 | CPU_TYPE_PPC | CPU_TYPE_32;
    else if (!strcmp(p,"ppc") || !strcmp(p,"ppc32") || !strncmp(p,"60",2) ||
             !strncmp(p,"75",2) || !strncmp(p,"85",2))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_32;
    else if (!strcmp(p,"ppc64") || !strcmp(p,"620"))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_64;
    else if (!strcmp(p,"7450"))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_7450 | CPU_TYPE_32 | CPU_TYPE_ALTIVEC;
    else if (!strncmp(p,"74",2) || !strcmp(p,"avec") || !strcmp(p,"altivec"))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_32 | CPU_TYPE_ALTIVEC;
    else if (!strcmp(p,"403"))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_403 | CPU_TYPE_32;
    else if (!strcmp(p,"405"))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_403 | CPU_TYPE_405 | CPU_TYPE_32;
    else if (!strncmp(p,"44",2) || !strncmp(p,"46",2))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_440 | CPU_TYPE_BOOKE | CPU_TYPE_ISEL
                 | CPU_TYPE_RFMCI | CPU_TYPE_32;
    else if (!strcmp(p,"821") || !strcmp(p,"850") || !strcmp(p,"860"))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_860 | CPU_TYPE_32;
    else if (!strcmp(p,"e300"))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_E300 | CPU_TYPE_32;
    else if (!strcmp(p,"e500"))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_E500 | CPU_TYPE_BOOKE | CPU_TYPE_ISEL
                 | CPU_TYPE_SPE | CPU_TYPE_EFS | CPU_TYPE_PMR | CPU_TYPE_RFMCI
                 | CPU_TYPE_32;
    else if (!strcmp(p,"booke"))
      cpu_type = CPU_TYPE_PPC | CPU_TYPE_BOOKE;
    else if (!strcmp(p,"com"))
      cpu_type = CPU_TYPE_COMMON | CPU_TYPE_32;
    else if (!strcmp(p,"any"))
      cpu_type |= CPU_TYPE_ANY;
    else
      return 0;
  }
  else if (!strcmp(p,"-no-regnames"))
    regnames = 0;
  else if (!strcmp(p,"-little"))
    ppc_endianess = 0;
  else if (!strcmp(p,"-big"))
    ppc_endianess = 1;
  else if (!strncmp(p,"-sdreg=",7)) {
    i = atoi(p+7);
    if (i>=0 && i<=31)
      sdreg = i;
    else
      cpu_error(13);  /* not a valid register */
  }
  else if (!strncmp(p,"-sd2reg=",8)) {
    i = atoi(p+8);
    if (i>=0 && i<=31)
      sd2reg = i;
    else
      cpu_error(13);  /* not a valid register */
  }
  else if (!strcmp(p,"-opt-branch"))
    opt_branch = 1;
  else
    return 0;

  return 1;
}
