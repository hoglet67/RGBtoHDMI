/*
** cpu.c 650x/65C02/6510/6280/45gs02 cpu-description file
** (c) in 2002,2006,2008-2012,2014-2021 by Frank Wille
*/

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};

int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);

char *cpu_copyright="vasm 6502 cpu backend 0.9c (c) 2002,2006,2008-2012,2014-2021 Frank Wille";
char *cpuname = "6502";
int bitsperbyte = 8;
int bytespertaddr = 2;

uint16_t cpu_type = M6502;
static int branchopt = 0;
static int modifier;      /* set by find_base() */
static utaddr dpage = 0;  /* default zero/direct page - set with SETDP */
static char lo_c = '<';
static char hi_c = '>';
static int OC_JMPABS,OC_BRA;


int ext_unary_type(char *s)
{
  return *s==lo_c ? LOBYTE : HIBYTE;
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
  ext->dp = dpage;  /* current DP defined by SETDP directive */
}


int parse_operand(char *p,int len,operand *op,int required)
{
  char *start = p;
  int indir = 0;

  op->flags = 0;

  p = skip(p);
  if (len>0 && required!=DATAOP && check_indir(p,start+len)) {
    indir = 1;
    p = skip(p+1);
  }

  switch (required) {
    case IMMED:
      if (*p++ != '#')
        return PO_NOMATCH;
      p = skip(p);
      break;
    case INDIR:
    case INDIRX:
    case INDX:
    case INDY:
    case INDZ:
    case INDZ32:
    case IND32:
    case DPINDIR:
      if (!indir)
        return PO_NOMATCH;
      break;
    case WBIT:
      if (*p == '#')  /* # is optional */
        p = skip(++p);
      if (indir)
        return PO_NOMATCH;
      break;
    case ABS:
      if (*p==hi_c || *p=='!' || *p=='|') {
        p = skip(++p);
        op->flags |= OF_HI;  /* force absolute addressing mode */
      }
      else if (*p == lo_c) {
        p = skip(++p);
        op->flags |= OF_LO;  /* force zero/direct page addressing mode */
      }
    default:
      if (indir)
        return PO_NOMATCH;
      break;
  }

  if (required < ACCU)
    op->value = parse_expr(&p);
  else
    op->value = NULL;

  switch (required) {
    case INDX:
    case INDIRX:
      if (*p++ == ',') {
        p = skip(p);
        if (toupper((unsigned char)*p++) != 'X')
          return PO_NOMATCH;
      }
      else
        return PO_NOMATCH;
      break;
    case ACCU:
      if (len != 0) {
        if (len!=1 || toupper((unsigned char)*p++) != 'A')
          return PO_NOMATCH;
      }
      break;
    case DUMX:
      if (toupper((unsigned char)*p++) != 'X')
        return PO_NOMATCH;
      break;
    case DUMY:
      if (toupper((unsigned char)*p++) != 'Y')
        return PO_NOMATCH;
      break;
    case DUMZ:
      if (toupper((unsigned char)*p++) != 'Z')
        return PO_NOMATCH;
      break;
  }

  if (required>=FIRST_INDIR && required<=LAST_INDIR) {
    p = skip(p);
    if (*p++ != ')') {
      cpu_error(2);  /* missing closing parenthesis */
      return PO_CORRUPT;
    }
  }

  if (required==INDZ32 || required==IND32 ) {
    p = skip(p);
    if (*p++ != ']') {
      cpu_error(13);  /* missing closing square bracket */
      return PO_CORRUPT;
    }
  }

  p = skip(p);
  if (p-start < len)
    cpu_error(1);  /* trailing garbage in operand */
  op->type = required;
  return PO_MATCH;
}


char *parse_cpu_special(char *start)
{
  const char zeroname[] = ".zero";
  char *name=start,*s=start;

  if (dotdirs && *s=='.') {
    s++;
    name++;
  }
  if (ISIDSTART(*s)) {
    s++;
    while (ISIDCHAR(*s))
      s++;

    if (s-name==5 && !strnicmp(name,"setdp",5)) {
      s = skip(s);
      dpage = (utaddr)parse_constexpr(&s);
      eol(s);
      return skip_line(s);
    }
    else if (s-name==5 && !strnicmp(name,"zpage",5)) {
      char *name;
      int done;
      do{
	s = skip(s);
	if (name = parse_identifier(&s)) {
	  symbol *sym = new_import(name);
	  myfree(name);
	  sym->flags |= ZPAGESYM;
	}
	else
	  cpu_error(8);  /* identifier expected */
	s = skip(s);
	if(*s==','){
	  s++;
	  done = 0;
	}else
	  done = 1;
      }while(!done);
      eol(s);
      return skip_line(s);
    }
    else if (s-name==4 && !strnicmp(name,zeroname+1,4)) {  /* zero */
      section *sec = new_section(dotdirs ?
                                 (char *)zeroname : (char *)zeroname+1,
                                 "aurw",1);
      sec->flags |= NEAR_ADDRESSING;  /* meaning of zero-page addressing */
      set_section(sec);
      eol(s);
      return skip_line(s);
    }
  }
  return start;
}


int parse_cpu_label(char *labname,char **start)
/* parse cpu-specific directives following a label field,
   return zero when no valid directive was recognized */
{
  char *dir=*start,*s=*start;

  if (ISIDSTART(*s)) {
    s++;
    while (ISIDCHAR(*s))
      s++;
    if (dotdirs && *dir=='.')
      dir++;

    if (s-dir==3 && !strnicmp(dir,"ezp",3)) {
      /* label EZP <expression> */
      symbol *sym;

      s = skip(s);
      sym = new_equate(labname,parse_expr_tmplab(&s));
      sym->flags |= ZPAGESYM;
      eol(s);
      *start = skip_line(s);
      return 1;
    }
  }
  return 0;
}


static void optimize_instruction(instruction *ip,section *sec,
                                 taddr pc,int final)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  symbol *base;
  operand *op;
  taddr val;
  int i;

  for (i=0; i<MAX_OPERANDS; i++) {
    if ((op = ip->op[i]) != NULL) {
      if (op->value != NULL) {
        if (eval_expr(op->value,&val,sec,pc))
          base = NULL;  /* val is constant/absolute */
        else
          find_base(op->value,&base,sec,pc);  /* get base-symbol */

        if (IS_ABS(op->type)) {
          if (!mnemo->ext.zp_opcode && (op->flags & OF_LO))
            cpu_error(10);  /* zp/dp not available */
          if (mnemo->ext.zp_opcode && ((op->flags & OF_LO) ||
                                       (!(op->flags & OF_HI) &&
              ((base==NULL && ((val>=0 && val<=0xff) ||
                               ((utaddr)val>=ip->ext.dp &&
                                (utaddr)val<=ip->ext.dp+0xff))) ||
               (base!=NULL && ((base->flags & ZPAGESYM) || (LOCREF(base) &&
                               (base->sec->flags & NEAR_ADDRESSING)))))
             ))) {
            /* we can use a zero page addressing mode for absolute 16-bit */
            op->type += ZPAGE-ABS;
          }
        }
        else if (op->type==REL8 && (base==NULL || !is_pc_reloc(base,sec))) {
          taddr bd = val - (pc + 2);

          if ((bd<-0x80 || bd>0x7f) && branchopt) {
            if (mnemo->ext.opcode==0x80 || mnemo->ext.opcode==0x12) {
              /* translate out of range 65C02/DTV BRA to JMP */
              ip->code = OC_JMPABS;
              op->type = ABS;
            }
            else  /* branch dest. out of range: use a B!cc/JMP combination */
              op->type = RELJMP;
          }
        }
        else if (ip->code==OC_JMPABS && (cpu_type&(DTV|M65C02))!=0 &&
                 branchopt && !(base!=NULL && is_pc_reloc(base,sec))) {
          taddr bd = val - (pc + 2);

          if (bd>=-128 && bd<=128) {
            /* JMP may be optimized to a BRA */
            ip->code = OC_BRA;
            op->type = REL8;
          }
        }
      }
    }
  }
}


static size_t get_inst_size(instruction *ip)
{
  size_t sz = 1;
  int i;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] != NULL) {
      switch (ip->op[i]->type) {
        case REL8:
        case INDX:
        case INDY:
        case INDZ:
        case IND32:
        case INDZ32:
        case DPINDIR:
        case IMMED:
        case ZPAGE:
        case ZPAGEX:
        case ZPAGEY:
        case ZPAGEZ:
          sz += 1;
          break;
        case REL16:
        case ABS:
        case ABSX:
        case ABSY:
        case ABSZ:
        case INDIR:
        case INDIRX:
          sz += 2;
          break;
        case RELJMP:
          sz += 4;
          break;
      }
    }
  }
  return sz;
}


size_t instruction_size(instruction *ip,section *sec,taddr pc)
{
  instruction *ipcopy;
  int i;

  for (i=0; i<MAX_OPERANDS-1; i++) {
    /* convert DUMX/DUMY/DUMZ operands into real addressing modes first */
    if (ip->op[i]!=NULL && ip->op[i+1]!=NULL) {
      if (ip->op[i]->type == ABS) {
        if (ip->op[i+1]->type == DUMX) {
          ip->op[i]->type = ABSX;
          break;
        }
        else if (ip->op[i+1]->type == DUMY) {
          ip->op[i]->type = ABSY;
          break;
        }
        else if (ip->op[i+1]->type == DUMZ) {
          ip->op[i]->type = ABSZ;
          break;
        }
      }
      else if (ip->op[i]->type == INDIR) {
        if (ip->op[i+1]->type == DUMY) {
          ip->op[i]->type = INDY;
          break;
        }
        else if (ip->op[i+1]->type == DUMZ) {
          ip->op[i]->type = INDZ;
          break;
        }
      }
    }
  }

  if (++i < MAX_OPERANDS) {
    /* we removed a DUMX/DUMY/DUMZ operand at the end */
    myfree(ip->op[i]);
    ip->op[i] = NULL;
  }

  ipcopy = copy_inst(ip);
  optimize_instruction(ipcopy,sec,pc,0);
  return get_inst_size(ipcopy);
}


static void rangecheck(instruction *ip,symbol *base,taddr val,operand *op)
{
  switch (op->type) {
    case ZPAGE:
    case ZPAGEX:
    case ZPAGEY:
    case ZPAGEZ:
    case INDX:
    case INDY:
    case INDZ:
    case IND32:
    case INDZ32:
    case DPINDIR:
      if (base!=NULL && base->type==IMPORT) {
        if (val<-0x80 || val>0x7f)
          cpu_error(12,8); /* addend doesn't fit into 8 bits */
      }
      else {
        if ((val<0 || val>0xff) &&
            ((utaddr)val<ip->ext.dp || (utaddr)val>ip->ext.dp+0xff))
          /*cpu_error(11)*/;   /* operand not in zero/direct page */
      }
      break;
    case IMMED:
      if (val<-0x80 || val>0xff)
        cpu_error(5,8); /* operand doesn't fit into 8 bits */
      break;
    case REL8:
      if (val<-0x80 || val>0x7f)
        cpu_error(6);   /* branch destination out of range */
      break;
    case WBIT:
      if (val<0 || val>7)
        cpu_error(7);   /* illegal bit number */
      break;
  }
}


dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  unsigned char *d,oc;
  int optype,i;
  taddr val;

  optimize_instruction(ip,sec,pc,1);  /* really execute optimizations now */

  db->size = get_inst_size(ip);
  d = db->data = mymalloc(db->size);

  /* write opcode */
  oc = mnemonics[ip->code].ext.opcode;
  for (i=0; i<MAX_OPERANDS; i++) {
    optype = ip->op[i]!=NULL ? ip->op[i]->type : IMPLIED;
    switch (optype) {
      case ZPAGE:
      case ZPAGEX:
      case ZPAGEY:
      case ZPAGEZ:
        oc = mnemonics[ip->code].ext.zp_opcode;
        break;
      case RELJMP:
        oc ^= 0x20;  /* B!cc branch */
        break;
    }
  }
  *d++ = oc;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] != NULL){
      operand *op = ip->op[i];
      int offs = d - db->data;
      symbol *base;

      optype = (int)op->type;
      if (op->value != NULL) {
        if (!eval_expr(op->value,&val,sec,pc)) {
          taddr add = 0;
          int btype;

          modifier = 0;
          btype = find_base(op->value,&base,sec,pc);
          if (btype==BASE_PCREL && optype==IMMED)
            op->flags |= OF_PC;  /* immediate value with pc-rel. relocation */

          if (optype==WBIT || btype==BASE_ILLEGAL ||
              (btype==BASE_PCREL && !(op->flags & OF_PC))) {
            general_error(38);  /* illegal relocation */
          }
          else {
            if (modifier) {
              if (op->flags & (OF_LO|OF_HI))
                cpu_error(9);  /* multiple hi/lo modifiers */
              switch (modifier) {
                case LOBYTE: op->flags |= OF_LO; break;
                case HIBYTE: op->flags |= OF_HI; break;
              }
            }

            if ((optype==REL8 || optype==REL16) && !is_pc_reloc(base,sec)) {
              /* relative branch requires no relocation */
              val = val - (pc + offs + 1);
            }
            else {
              int type = REL_ABS;
              int size;
              rlist *rl;

              switch (optype) {
                case ABS:
                case ABSX:
                case ABSY:
                case ABSZ:
                  op->flags &= ~(OF_LO|OF_HI);
                case INDIR:
                case INDIRX:
                  size = 16;
                  break;
                case ZPAGE:
                case ZPAGEX:
                case ZPAGEY:
                case ZPAGEZ:
                  op->flags &= ~(OF_LO|OF_HI);
                case INDX:
                case INDY:
                case INDZ:
                case INDZ32:
                case IND32:
                case DPINDIR:
                  size = 8;
                  break;
                case IMMED:
                  if (op->flags & OF_PC) {
                    type = REL_PC;
                    val += offs;
                  }
                  size = 8;
                  break;
                case RELJMP:
                  size = 16;
                  offs = 3;
                  break;
                case REL8:
                  type = REL_PC;
                  size = 8;
                  add = -1;  /* 6502 addend correction */
                  break;
                case REL16:
                  type = REL_PC;
                  size = 16;
                  add = -1;  /* 6502 addend correction */
                  break;
                default:
                  ierror(0);
                  break;
              }

              rl = add_extnreloc(&db->relocs,base,val+add,type,0,size,offs);
              if (op->flags & OF_LO) {
                if (rl)
                  ((nreloc *)rl->reloc)->mask = 0xff;
                val = val & 0xff;
              }
              else if (op->flags & OF_HI) {
                if (rl)
                  ((nreloc *)rl->reloc)->mask = 0xff00;
                val = (val >> 8) & 0xff;
              }
            }
          }
        }
        else {
          /* constant/absolute value */
          base = NULL;
          if (optype==REL8 || optype==REL16)
            val = val - (pc + offs + 1);
        }

        rangecheck(ip,base,val,op);

        /* write operand data */
        switch (optype) {
          case ABSX:
          case ABSY:
          case ABSZ:
            if (!*(db->data)) /* STX/STY allow only ZeroPage addressing mode */
              cpu_error(5,8); /* operand doesn't fit into 8 bits */
          case ABS:
          case INDIR:
          case INDIRX:
          case REL16:
            *d++ = val & 0xff;
            *d++ = (val>>8) & 0xff;
            break;
          case DPINDIR:
          case INDX:
          case INDY:
          case INDZ:
          case INDZ32:
          case IND32:
          case ZPAGE:
          case ZPAGEX:
          case ZPAGEY:
            if ((utaddr)val>=ip->ext.dp && (utaddr)val<=ip->ext.dp+0xff)
              val -= ip->ext.dp;
          case IMMED:
          case REL8:
            *d++ = val & 0xff;
            break;
          case RELJMP:
            if (d - db->data > 1)
              ierror(0);
            *d++ = 3;     /* B!cc *+3 */
            *d++ = 0x4c;  /* JMP */
            *d++ = val & 0xff;
            *d++ = (val>>8) & 0xff;
            break;
          case WBIT:
            *(db->data) |= (val&7) << 4;  /* set bit number in opcode */
            break;
        }
      }
    }
  }

  return db;
}


dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  taddr val;

  if (bitsize!=8 && bitsize!=16 && bitsize!=32)
    cpu_error(3,bitsize);  /* data size not supported */

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
      cpu_error(5,8);  /* operand doesn't fit into 8-bits */
  } else if (bitsize < 32) {
    if (val<-0x8000 || val>0xffff)
      cpu_error(5,16);  /* operand doesn't fit into 16-bits */
  }

  setval(0,db->data,db->size,val);
  return db;
}


operand *new_operand()
{
  operand *new = mymalloc(sizeof(*new));
  new->type = -1;
  new->flags = 0;
  return new;
}


int cpu_available(int idx)
{
  return (mnemonics[idx].ext.available & cpu_type) != 0;
}


int init_cpu()
{
  int i;

  for (i=0; i<mnemonic_cnt; i++) {
    if (mnemonics[i].ext.available & cpu_type) {
      if (!strcmp(mnemonics[i].name,"jmp") && mnemonics[i].operand_type[0]==ABS)
        OC_JMPABS = i;
      else if (!strcmp(mnemonics[i].name,"bra"))
        OC_BRA = i;
    }
  }
  return 1;
}


int cpu_args(char *p)
{
  if (!strcmp(p,"-bbcade")) {
    /* GMGM - BBC ADE assembler swaps meaning of < and > */
    lo_c = '>';
    hi_c = '<';
  }
  else if (!strcmp(p,"-opt-branch"))
    branchopt = 1;
  else if (!strcmp(p,"-illegal"))
    cpu_type |= ILL;
  else if (!strcmp(p,"-dtv"))
    cpu_type |= DTV;
  else if (!strcmp(p,"-c02"))
    cpu_type = M6502 | M65C02;
  else if (!strcmp(p,"-wdc02"))
    cpu_type = M6502 | M65C02 | WDC02 | WDC02ALL;
  else if (!strcmp(p,"-ce02"))
    cpu_type = M6502 | M65C02 | WDC02 | WDC02ALL | CSGCE02;
  else if (!strcmp(p,"-6280"))
    cpu_type = M6502 | M65C02 | WDC02 | WDC02ALL | HU6280;
  else if (!strcmp(p,"-mega65"))
    cpu_type = M6502 | M65C02 | WDC02 | M45GS02;
  else
    return 0;

  return 1;
}
