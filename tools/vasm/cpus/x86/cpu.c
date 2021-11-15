/*
** cpu.c x86 cpu-description file
** (c) in 2005-2006,2011,2015-2019 by Frank Wille
*/

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};
int mnemonic_cnt = sizeof(mnemonics)/sizeof(mnemonics[0]);

char *cpu_copyright = "vasm x86 cpu backend 0.7a (c) 2005-2006,2011,2015-2019 Frank Wille";
char *cpuname = "x86";
int bitsperbyte = 8;
int bytespertaddr = 4;

/* cpu options */
uint32_t cpu_type = CPUAny;
static long cpudebug = 0;

static regsym x86_regsyms[] = {
#include "registers.h"
};

/* opcode-prefixes recognized during parse_instruction() */
static int prefix_cnt;
static unsigned char prefix[MAX_PREFIXES];

/* opcodes */
static unsigned char OC_ADDR_PREFIX;
static unsigned char OC_DATA_PREFIX;
static unsigned char OC_WAIT_PREFIX;
static unsigned char OC_LOCK_PREFIX;
static unsigned char OC_CSEG_PREFIX;
static unsigned char OC_DSEG_PREFIX;
static unsigned char OC_ESEG_PREFIX;
static unsigned char OC_FSEG_PREFIX;
static unsigned char OC_GSEG_PREFIX;
static unsigned char OC_SSEG_PREFIX;
static unsigned char OC_REPE_PREFIX;
static unsigned char OC_REPNE_PREFIX;
static unsigned char OC_POP_SREG2;
static unsigned char OC_MOV_ACC_DISP;
static unsigned char OC_JMP_DISP;

/* opcode suffixes */
static char *b_str = "b";
static char *w_str = "w";
static char *l_str = "l";
/*static char *s_str = "s";*/
static char *q_str = "q";
/*static char *x_str = "x";*/

/* assembler mode: 16, 32 or 64 bit */
enum codetype {
  CODE_32BIT,
  CODE_16BIT,
  CODE_64BIT
};

static enum codetype mode_flag = CODE_32BIT;  /* 32 bit is default */

/* operand types for printing */
static const char *operand_type_str[] = {
  "Reg8","Reg16","Reg32","Reg64","Imm8","Imm8S","Imm16","Imm32","Imm32S",
  "Imm64","Imm1","Disp8","Disp16","Disp32","Disp32S","Disp64",
  "BaseIndex","ShiftCntReg","IOPortReg","CtrlReg","DebugReg",
  "TestReg","Acc","SegReg2","SegReg3","MMXReg","XMMReg",
  "FloatReg","FloatAcc","EsSeg","JmpAbs","InvMem"
};

/* scale factors log2(scale) -> original scale factor */
static const int scale_factor_tab[4] = { 1, 2, 4, 8 };



operand *new_operand(void)
{
  return mycalloc(sizeof(operand));
}


int x86_data_operand(int n)
/* return data operand type for these number of bits */
{
  if (n&OPSZ_FLOAT) return OPSZ_BITS(n)>32?Float64:Float32;
  if (OPSZ_BITS(n)<=8) return Data8;
  if (OPSZ_BITS(n)<=16) return Data16;
  if (OPSZ_BITS(n)<=32) return Data32;
  if (OPSZ_BITS(n)<=64) return Data64;
  cpu_error(20,n);  /* data objects with n bits size are not supported */
  return 0;
}


static void print_operand_type(int ot,char c)
{
  int i,b;

  for (i=0,b=0; i<32; i++,ot>>=1) {
    if (ot & 1) {
      if (b)
        putchar(c);
      printf("%s",operand_type_str[i]);
      b = 1;
    }
  }
}


static void print_operands(instruction *ip,taddr pc)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  int i;

  if (pc == -1)
    printf("  \"%s\".\"%s\"\n",mnemo->name,
           ip->qualifiers[0] ? ip->qualifiers[0] : emptystr);
  else
    printf("  %08lx: \"%s\".\"%s\"\n",(unsigned long)pc,mnemo->name,
           ip->qualifiers[0] ? ip->qualifiers[0] : emptystr);

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] == NULL)
      break;
    printf("operand %d: ",i+1);
    print_operand_type(ip->op[i]->type,' ');
    putchar('\n');
  }
}


static int prefix_index(unsigned char oc)
{
  int i = -1;

  if (oc == OC_WAIT_PREFIX) {
    i = WAIT_PREFIX;
  }
  else if (oc==OC_LOCK_PREFIX || oc==OC_REPE_PREFIX ||
           oc==OC_REPNE_PREFIX) {
    i = LOCKREP_PREFIX;
  }
  else if (oc == OC_ADDR_PREFIX) {
    i = ADDR_PREFIX;
  }
  else if (oc == OC_DATA_PREFIX) {
    i = DATA_PREFIX;
  }
  else if (oc==OC_CSEG_PREFIX || oc==OC_DSEG_PREFIX ||
           oc==OC_ESEG_PREFIX || oc==OC_FSEG_PREFIX ||
           oc==OC_GSEG_PREFIX || oc==OC_SSEG_PREFIX) {
    i = SEG_PREFIX;
  }
  else
    ierror(0);

  return i;
}


#if 0 /* @@@ not now */
static int add_prefix(unsigned char oc)
{
  int i = prefix_index(oc);
  int a = prefix[i] == 0;

  prefix[i] = oc;
  prefix_cnt += a;
  return a;
}
#endif


static int add_ip_prefix(instruction *ip,unsigned char oc)
{
  int i = prefix_index(oc);
  int a = ip->ext.prefix[i] == 0;

  ip->ext.prefix[i] = oc;
  ip->ext.num_prefixes += a;
  return a;
}


static int add_seg_prefix(instruction *ip,int segrn)
{
  switch (segrn) {
    case ESEG_REGNUM: return add_ip_prefix(ip,OC_ESEG_PREFIX);
    case CSEG_REGNUM: return add_ip_prefix(ip,OC_CSEG_PREFIX);
    case SSEG_REGNUM: return add_ip_prefix(ip,OC_SSEG_PREFIX);
    case DSEG_REGNUM: return add_ip_prefix(ip,OC_DSEG_PREFIX);
    case FSEG_REGNUM: return add_ip_prefix(ip,OC_FSEG_PREFIX);
    case GSEG_REGNUM: return add_ip_prefix(ip,OC_GSEG_PREFIX);
    default: ierror(0);
  }
  return 0;
}


static char suffix_from_reg(instruction *ip)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  int i,ot,io;

  /* i/o instruction? */
  io = (mnemo->operand_type[0] & IOPortReg) ||
       (mnemo->operand_type[1] & IOPortReg);

  /* determine size qualifier from last register operand */
  for (i=MAX_OPERANDS-2; i>=0; i--) {
    if (ip->op[i] == NULL)
      continue;
    ot = ip->op[i]->parsed_type;

    if ((ot & Reg) && !(io && (ot & IOPortReg))) {
      if (ot & Reg8)
        ip->qualifiers[0] = b_str;
      else if (ot & Reg16)
        ip->qualifiers[0] = w_str;
      else if (ot & Reg32)
        ip->qualifiers[0] = l_str;
      else if (ot & Reg64)
        ip->qualifiers[0] = q_str;
      else
        ierror(0);
      break;
    }
  }

  return ip->qualifiers[0] ?
         tolower((unsigned char)ip->qualifiers[0][0]) : '\0';
}


static void chk_byte_reg(instruction *ip)
{
  int i,ot;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] == NULL)
      break;
    ot = ip->op[i]->type;

    if (ot & (AnyReg|FloatReg|FloatAcc)) {
      if (ot & Reg8)
        continue;  /* 8-bit register is just perfect */

      if ((ot & WordReg) && ip->op[i]->basereg->reg_num < 4) {
        if (mode_flag!=CODE_64BIT /*@@@ || (ot & IOPortReg)*/) {
          regsym *oldreg = ip->op[i]->basereg;

          if (ot & Reg16)
            ip->op[i]->basereg -= AX_INDEX - AL_INDEX;
          else
            ip->op[i]->basereg -= EAX_INDEX - AL_INDEX;
          /* warning: using register x instead of y due to suffix */
          cpu_error(9,ip->op[i]->basereg->reg_name,oldreg->reg_name,
                    ip->qualifiers[0][0]);
          ip->op[i]->type &= ~Reg;
          ip->op[i]->type |= Reg8;
          ip->op[i]->parsed_type &= ~Reg;
          ip->op[i]->parsed_type |= Reg8;
        }
        continue;
      }

      /* any other register is not allowed */
      cpu_error(10,ip->op[i]->basereg->reg_name,ip->qualifiers[0][0]);
      suffix_from_reg(ip);
      break;
    }
  }
}


static void chk_word_reg(instruction *ip)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  int i,ot;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] == NULL)
      break;
    ot = ip->op[i]->type;

    if (ot & (AnyReg|FloatReg|FloatAcc)) {
      /* allow 8-bit registers only when exclusively required */
      if ((ot & Reg8) && (mnemo->operand_type[i] & (Reg16|Reg32|Acc))) {
        cpu_error(10,ip->op[i]->basereg->reg_name,ip->qualifiers[0][0]);
        suffix_from_reg(ip);
        break;
      }
      else if ((ot & Reg32) &&
               (mnemo->operand_type[i] & (Reg16|Acc))) {
        regsym *oldreg = ip->op[i]->basereg;

        if (mode_flag == CODE_64BIT) {
          cpu_error(10,ip->op[i]->basereg->reg_name,ip->qualifiers[0][0]);
          suffix_from_reg(ip);
          break;
        }
        ip->op[i]->basereg -= EAX_INDEX - AX_INDEX;
        /* warning: using register x instead of y due to suffix */
        cpu_error(9,ip->op[i]->basereg->reg_name,oldreg->reg_name,
                  ip->qualifiers[0][0]);
        ip->op[i]->type &= ~Reg;
        ip->op[i]->type |= Reg16;
        ip->op[i]->parsed_type &= ~Reg;
        ip->op[i]->parsed_type |= Reg16;
      }
    }
  }
}


static void chk_long_reg(instruction *ip)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  int i,ot;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] == NULL)
      break;
    ot = ip->op[i]->type;

    if (ot & (AnyReg|FloatReg|FloatAcc)) {
      /* allow 8-bit registers only when exclusively required */
      if ((ot & Reg8) && (mnemo->operand_type[i] & (Reg16|Reg32|Acc))) {
        cpu_error(10,ip->op[i]->basereg->reg_name,ip->qualifiers[0][0]);
        suffix_from_reg(ip);
        break;
      }
      else if ((ot & Reg16) &&
               (mnemo->operand_type[i] & (Reg32|Acc))) {
        regsym *oldreg = ip->op[i]->basereg;

        if (mode_flag == CODE_64BIT) {
          cpu_error(10,ip->op[i]->basereg->reg_name,ip->qualifiers[0][0]);
          suffix_from_reg(ip);
          break;
        }
        ip->op[i]->basereg += EAX_INDEX - AX_INDEX;
        /* warning: using register x instead of y due to suffix */
        cpu_error(9,ip->op[i]->basereg->reg_name,oldreg->reg_name,
                  ip->qualifiers[0][0]);
        ip->op[i]->type &= ~Reg;
        ip->op[i]->type |= Reg32;
        ip->op[i]->parsed_type &= ~Reg;
        ip->op[i]->parsed_type |= Reg32;
      }
    }
  }
}


static void chk_quad_reg(instruction *ip)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  int i,ot;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] == NULL)
      break;
    ot = ip->op[i]->type;

    if (ot & (AnyReg|FloatReg|FloatAcc)) {
      /* allow 8-bit registers only when exclusively required */
      if ((ot & Reg8) && (mnemo->operand_type[i] & (Reg16|Reg32|Acc))) {
        cpu_error(10,ip->op[i]->basereg->reg_name,ip->qualifiers[0][0]);
        suffix_from_reg(ip);
        break;
      }
      else if ((ot & (Reg16|Reg32)) &&
               (mnemo->operand_type[i] & (Reg32|Acc))) {  /* @@@ Reg32? */
        cpu_error(10,ip->op[i]->basereg->reg_name,ip->qualifiers[0][0]);
        suffix_from_reg(ip);
        break;
      }
    }
  }
}


static int assign_suffix(instruction *ip)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  char suffix = ip->qualifiers[0] ?
                tolower((unsigned char)ip->qualifiers[0][0]) : '\0';

  if (suffix == '\0' &&
      (mnemo->ext.opcode_modifier &
       (b_Illegal|w_Illegal|l_Illegal|s_Illegal|x_Illegal|q_Illegal))
      == (b_Illegal|w_Illegal|l_Illegal|s_Illegal|x_Illegal|q_Illegal)) {
    return 1;	/* no suffix needed */
  }

  if (mnemo->ext.opcode_modifier & (Size16|Size32|Size64)) {
    if (mnemo->ext.opcode_modifier & Size16)
      ip->qualifiers[0] = w_str;
    else if (mnemo->ext.opcode_modifier & Size64)
      ip->qualifiers[0] = q_str;
    else
      ip->qualifiers[0] = l_str;
  }
  else if (ip->ext.flags & HAS_REG_OPER) {
    if (suffix == '\0') {
      /* a missing suffix can be determined by the last register operand */
      suffix = suffix_from_reg(ip);
    }
    /* check if register operands match the given suffix */
    else if (suffix == 'b')
      chk_byte_reg(ip);
    else if (suffix == 'w')
      chk_word_reg(ip);
    else if (suffix == 'l')
      chk_long_reg(ip);
    else if (suffix == 'q')
      chk_quad_reg(ip);
    else if (!(mnemo->ext.opcode_modifier & IgnoreSize)) {
      suffix_from_reg(ip);
      cpu_error(11,suffix);  /* illegal suffix */
      return 0;
    }
  }
  else if (suffix=='\0' && (mnemo->ext.opcode_modifier & DefaultSize)) {
    /* not needed for i386 */
  }

  if (suffix=='\0' && (mnemo->ext.opcode_modifier & W)) {
    /* need suffix to determine size of instruction! */
    cpu_error(12);  /* instruction has no suffix and no regs */
    ip->qualifiers[0] = b_str;
    return 0;
  }
  return 1;
}


static int process_suffix(instruction *ip,int final)
{
  char suffix = ip->qualifiers[0] ?
                tolower((unsigned char)ip->qualifiers[0][0]) : '\0';

  if (suffix != 'b') {
    /* we have to select word or dword operation mode */
    mnemonic *mnemo = &mnemonics[ip->code];

    if (cpudebug & 64) {
      printf("%s(%c%c): process_suffix(%dbit): %c\n",mnemo->name,
             (mnemo->ext.opcode_modifier & W) ? 'W' : ' ',
             (mnemo->ext.opcode_modifier & ShortForm) ? 'S' : ' ',
             mode_flag==CODE_64BIT?64:(mode_flag==CODE_32BIT?32:16),
             suffix ? suffix : ' ');
    }
    if (mnemo->ext.opcode_modifier & W) {
      if (mnemo->ext.opcode_modifier & ShortForm)
        ip->ext.base_opcode |= 8;
      else
        ip->ext.base_opcode |= 1;
    }

    if (suffix!='q' && !(mnemo->ext.opcode_modifier & IgnoreSize)) {
      if ((suffix=='w' && mode_flag==CODE_32BIT) ||
          (suffix=='l' && mode_flag==CODE_16BIT) ||
          (mode_flag==CODE_64BIT &&
           (mnemo->ext.opcode_modifier & JmpByte))) {
        /* we need a prefix to select the correct size */
        unsigned char code = OC_DATA_PREFIX;

        if (mnemo->ext.opcode_modifier & JmpByte)
          code = OC_ADDR_PREFIX;  /* jcxz, loop */

        if (cpudebug & 64)
          printf("\tadding prefix %02x\n",(unsigned)code);

        if (!add_ip_prefix(ip,code)) {
          if (!final)
            cpu_error(2);  /* same type of prefix used twice */
          return 0;
        }
      }
    }

    /* size floating point instruction */
    if (suffix == 'l') {
      if (mnemo->ext.opcode_modifier & FloatMF)
        ip->ext.base_opcode ^= 4;
    }
  }

  return 1;
}


static uint32_t suffix_flag(instruction *ip)
/* return the No_?Suf flag for the instruction's suffix */
{
  uint32_t chksuffix;

  switch (ip->qualifiers[0] ?
          tolower((unsigned char)ip->qualifiers[0][0]) : '\0') {
    case 'b': chksuffix = b_Illegal; break;
    case 'w': chksuffix = w_Illegal; break;
    case 'l': chksuffix = l_Illegal; break;
    case 's': chksuffix = s_Illegal; break;
    case 'q': chksuffix = q_Illegal; break;
    case 'x': chksuffix = x_Illegal; break;
    case '\0': chksuffix = 0; break;
    default: ierror(0); break;
  }
  return chksuffix;
}


static int find_next_mnemonic(instruction *ip)
/* finds a mnemonic with the same name, which fits the given
   operand types and suffix */
{
  char *inst_name = mnemonics[ip->code].name;
  int code = ip->code + 1;
  mnemonic *mnemo = &mnemonics[code];
  uint32_t chksuffix = suffix_flag(ip);

  if ((cpudebug & 32) && !(cpudebug & 4)) {
    printf("Finding next matching instruction for (opcode=0x%x):",
           ip->ext.base_opcode);
    print_operands(ip,-1);
  }

  while (!strcmp(inst_name,mnemo->name)) {
    int i,given,allowed,overlap,new_types[MAX_OPERANDS];

    for (i=0; i<MAX_OPERANDS; i++) {
      if (allowed = mnemo->operand_type[i]) {
        if (ip->op[i]) {
          given = ip->op[i]->parsed_type;
          overlap = allowed & given;
          if ((overlap & ~JmpAbs) &&
              (given & (BaseIndex|JmpAbs)) ==
               (overlap & (BaseIndex|JmpAbs))) {
            new_types[i] = overlap;
          }
          else
            break;
        }
        else
          break;
      }
      else {
        if (ip->op[i])
          break;
      }
    }

    if (i == MAX_OPERANDS) {
      /* all operands match, check suffix and CPU type */
      if (cpudebug & 32) {
        printf("\toperands match, suffixOK=%d, cpuOK=%d\n",
               (mnemo->ext.opcode_modifier & chksuffix) == 0,
               (mnemo->ext.available & cpu_type) == mnemo->ext.available);
      }
      if (!(mnemo->ext.opcode_modifier & chksuffix) &&
          (mnemo->ext.available & cpu_type)==mnemo->ext.available) {
        for (i=0; i<MAX_OPERANDS; i++) {
          if (ip->op[i]) {
            if (cpudebug & 32) {
              printf("\tnew op %d: ",i+1);
              print_operand_type(ip->op[i]->parsed_type,'|');
              printf(" & ");
              print_operand_type(new_types[i],'|');
              printf(" = ");
              print_operand_type(ip->op[i]->parsed_type&new_types[i],'|');
              printf("\n");
            }
            ip->op[i]->type = ip->op[i]->parsed_type & new_types[i];
          }
        }
        ip->code = code;
        ip->ext.base_opcode = mnemo->ext.base_opcode;
        assign_suffix(ip);
        if (cpudebug & 32)
          printf("\tNew opcode=0x%x!\n",ip->ext.base_opcode);
        return 1;
      }
    }

    /* try next entry from mnemonics table */
    mnemo++;
    code++;
  }

  if (cpudebug & 32)
    printf("\tNo matching instruction found!\n");
  return 0;
}


static int fix_imm_size(instruction *ip,operand *op,int final)
/* apply suffix to immediate operand */
{
  char suffix = ip->qualifiers[0] ?
                tolower((unsigned char)ip->qualifiers[0][0]) : '\0';
  int ot = op->type;

  if ((ot & (Imm8|Imm8S|Imm16|Imm32|Imm32S)) &&
      ot!=Imm8 && ot!=Imm8S && ot!=Imm16 &&
      ot!=Imm32 && ot!=Imm32S && ot!=Imm64) {

    switch (suffix) {
      case 'b': ot &= Imm8|Imm8S; break;
      case 'w': ot &= Imm16; break;
      case 'l': ot &= Imm32; break;
      case 'q': ot &= Imm64|Imm32S; break;
      case '\0':
        if ((mode_flag==CODE_16BIT) ^ (ip->ext.prefix[DATA_PREFIX]!=0))
          ot &= Imm16;
        else
          ot &= Imm32|Imm32S;
        break;
      default:
        ierror(0);
        break;
    }

    if (ot!=Imm8 && ot!=Imm8S && ot!=Imm16 &&
        ot!=Imm32 && ot!=Imm32S && ot!=Imm64) {
      if (final)
        cpu_error(24);  /* can't determine imm. op. size w/o suffix */
      return 0;
    }
  }
  return ot;
}


static void optimize_imm(instruction *ip,operand *op,section *sec,
                         taddr pc,int final)
/* determine smallest type which can hold the immediate mode */
{
  taddr val;
  int ot,nt;

  if (!eval_expr(op->value,&val,sec,pc)) {
    /* reloc or unknown symbols have always full size until they are known */
    ot = Imm32;  /* @@@ fixme? gas also allows Imm8-references */
  }
  else {
    /* set valid operand types for this number */
    ot = Imm64;
    if (val>=-0x80000000LL && val<=0xffffffffLL) {
      ot |= Imm32;
      if (val<=0x7fffffffLL) {
        ot |= Imm32S;
        if (val>=-0x8000 && val<=0xffff) {
          ot |= Imm16;
          if (val>=-0x80 && val<=0xff) {
            ot |= Imm8;
            if (val<=0x7f)
              ot |= Imm8S;
          }
        }
      }
    }
  }
  nt = ot & fix_imm_size(ip,op,final);

  while (!nt) {
    /* instruction doesn't support required modes, look for another one */
    int oldtype = op->type;

    op->type = ot;
    if (!find_next_mnemonic(ip)) {
      op->type = oldtype;
      break;
    }
    nt = op->type & fix_imm_size(ip,op,final);
  }
  if (nt)
    op->type = nt;
  else if (final)
    cpu_error(23);  /* instruction doesn't support these operand sizes */
}


static void optimize_jump(instruction *ip,operand *op,section *sec,
                          taddr pc,int final)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  int mod = mnemo->ext.opcode_modifier;
  int label_in_sec;
  symbol *base;
  taddr val,diff;

  if (!eval_expr(op->value,&val,sec,pc)) {
    if (find_base(op->value,&base,sec,pc) != BASE_OK) {
      if (final)
        general_error(38);  /* illegal relocation */
      return;
    }
    label_in_sec = !is_pc_reloc(base, sec);
  }
  else
    label_in_sec = 1;

  if (mod & JmpByte) {
    op->type = Disp8;
    if (final && label_in_sec) {
      diff = val - (pc + ip->ext.last_size);
      if (diff<-0x80 || diff>0x7f)
        cpu_error(22,diff);  /* jump destination out of range */
    }
  }
  else if (mod & JmpDword) {
    if (mode_flag == CODE_16BIT)
      op->type &= ~(Disp32|Disp32S);
    else
      op->type &= ~Disp16;
    if (final && label_in_sec) {
      diff = val - (pc + ip->ext.last_size);
      if (mode_flag == CODE_16BIT) {
        if (diff<-0x8000 || diff>0x7fff)
          cpu_error(22,diff);  /* jump destination out of range */
      }
      else {
        if (diff<-0x80000000LL || diff>0x7fffffffLL)
          cpu_error(22,diff);  /* jump destination out of range */
      }
    }
  }
  else {  /* Jump */
    op->type = (mode_flag == CODE_16BIT) ? Disp16 : Disp32;
    if (label_in_sec && ip->ext.last_size>=0) {
      /* check if it fits in 8-bit */
      diff = val - (pc + ip->ext.last_size);
      if (diff>=-0x80 && diff<=0x7f && (ip->ext.flags&OPTFAILED)!=OPTFAILED) {
        op->type = Disp8;
        return;
      }
      else if (final) {
        if (mode_flag == CODE_16BIT) {
          if (diff<-0x8000 || diff>0x7fff)
            cpu_error(22,diff);  /* jump dest. out of range */
        }
        else {
          if (diff<-0x80000000LL || diff>0x7fffffffLL)
            cpu_error(22,diff);  /* jump dest. out of range */
        }
      }
    }
    if (ip->ext.base_opcode == OC_JMP_DISP)
      ip->ext.base_opcode = 0xe9;    /* jmp <long distance> */
    else
      ip->ext.base_opcode += 0xf10;  /* j<cond> <long distance> */
  }
}


static void optimize_disp(operand *op,section *sec,taddr pc,int final)
{
  taddr val;

  if (!eval_expr(op->value,&val,sec,pc)) {
    /* reloc or unknown symbols have always full size until they are known */
    if (mode_flag == CODE_16BIT) {
      op->type &= ~(Disp8|Disp32|Disp32S|Disp64);
      if (final && !(op->type & Disp16))
        cpu_error(21,16);  /* need at least n bits for a relocatable symbol */
    }
    else {  /* CODE_32BIT */
      op->type &= ~(Disp8|Disp16);
      if (final && !(op->type & (Disp32|Disp32S|Disp64)))
        cpu_error(21,32);  /* need at least n bits for a relocatable symbol */
    }
  }
  else {
    /* try to use the smallest type for absolute displacements */
    if (mode_flag == CODE_16BIT) {
      op->type &= ~(Disp32|Disp32S|Disp64);
      if (final && (val<-0x8000 || val>0x7fff))
        cpu_error(25,16);  /* doesn't fit into 16 bits */
      if (val>=-0x80 && val<=0x7f && (op->type & Disp8))
        op->type &= ~Disp16;
      else
        op->type &= ~Disp8;
    }
    else {  /* CODE_32BIT */
      op->type &= ~Disp16;  /* no 16bit displacements possible */
      if (val>=-0x80000000LL && val<=0xffffffffLL && (op->type & Disp32)) {
        op->type &= ~Disp64;
        if (val<=0x7fffffffLL && (op->type & Disp32S))
          op->type &= ~Disp32;
        if (val>=-0x80 && val<=0x7f && (op->type & Disp8))
          op->type &= ~(Disp32|Disp32S);
        else
          op->type &= ~Disp8;
      }
      else
        op->type &= ~(Disp8|Disp16|Disp32|Disp32S);
    }
  }
}


static int mode_from_disp_size(int type)
{
  return (type & Disp8) ? 1 : (type & (Disp16 | Disp32 | Disp32S)) ? 2 : 0;
}


static int make_modrm(instruction *ip,mnemonic *mnemo,int final)
{
  int defaultseg = -1;
  int i,reg_operands,mem_operands;

  /* count number of register and memory operands */
  for (i=0,reg_operands=0,mem_operands=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i]) {
      if (ip->op[i]->type & AnyReg)
        reg_operands++;
      else if (ip->op[i]->type & AnyMem)
        mem_operands++;
    }
  }
  ip->ext.flags |= MODRM_BYTE;
  if (final && (cpudebug & 8)) {
    printf("make_modrm(): %s: %d reg operands, %d mem operands\n",
           mnemo->name,reg_operands,mem_operands);
  }

  if (reg_operands == 2) {
    i = (ip->op[0]->type & AnyReg) ? 0 : 1;  /* first register operand */
    ip->ext.rm.mode = 3;
    if (!(mnemo->operand_type[i+1] & AnyMem)) {
      ip->ext.rm.reg = ip->op[i+1]->basereg->reg_num;
      ip->ext.rm.regmem = ip->op[i]->basereg->reg_num;
    }
    else {
      ip->ext.rm.reg = ip->op[i]->basereg->reg_num;
      ip->ext.rm.regmem = ip->op[i+1]->basereg->reg_num;
    }
  }

  else {
    if (mem_operands) {
      operand *memop = ip->op[(ip->op[0]->type & AnyMem) ? 0 :
                              ((ip->op[1]->type & AnyMem) ? 1 : 2)];

      if (final && (cpudebug & 8)) {
        if (memop->basereg) {
          printf("\tbase=%%%s(",memop->basereg->reg_name);
          print_operand_type(memop->basereg->reg_type,'|');
          putchar(')');
        }
        else
          printf("\tbase=none");
        if (memop->indexreg) {
          printf(" index=%%%s(",memop->indexreg->reg_name);
          print_operand_type(memop->indexreg->reg_type,'|');
          putchar(')');
        }
        else
          printf(" index=none");
        printf(" scale=%d\n",scale_factor_tab[memop->log2_scale]);
      }
      defaultseg = DSEG_REGNUM;

      if (memop->basereg == NULL) {
        ip->ext.rm.mode = 0;
        if (final && memop->value==NULL)
          memop->value = number_expr(0);

        if (memop->indexreg == NULL) {
          /* no base and no index */
          if ((mode_flag==CODE_16BIT) ^ (ip->ext.prefix[ADDR_PREFIX]!=0) &&
              mode_flag!=CODE_64BIT) {
            ip->ext.rm.regmem = NO_BASEREG16;
            memop->type &= ~Disp;
            memop->type |= Disp16;
          }
          else if (mode_flag!=CODE_64BIT || ip->ext.prefix[ADDR_PREFIX]) {
            ip->ext.rm.regmem = NO_BASEREG;
            memop->type &= ~Disp;
            memop->type |= Disp32;
          }
          else {
            ip->ext.rm.regmem = TWO_BYTE_ESCAPE;
            ip->ext.sib.base = NO_BASEREG;
            ip->ext.sib.index = NO_INDEXREG;
            memop->type &= ~Disp;
            memop->type |= Disp32S;
            ip->ext.flags |= SIB_BYTE;
          }
        }

        else {
          /* no base, only index */
          ip->ext.rm.regmem = TWO_BYTE_ESCAPE;
          ip->ext.sib.base = NO_BASEREG;
          ip->ext.sib.index = memop->indexreg->reg_num;
          ip->ext.sib.scale = memop->log2_scale;
          memop->type &= ~Disp;
          if (mode_flag != CODE_64BIT)
            memop->type |= Disp32;
          else
            memop->type |= Disp32S;
          ip->ext.flags |= SIB_BYTE;
        }
      }

      else if (memop->basereg->reg_type == BaseIndex) {
        /* RIP base register for 64-bit mode */
        ip->ext.rm.regmem = NO_BASEREG;
        memop->type &= ~Disp;
        memop->type |= Disp32S;
        memop->flags |= OPER_PCREL;
      }

      else if (memop->basereg->reg_type & Reg16) {
        /* 16-bit mode base register */
        switch (memop->basereg->reg_num) {
          case 3:  /* %bx */
            if (memop->indexreg)  /* (%bx,%si) or (%bx,%di) */
              ip->ext.rm.regmem = memop->indexreg->reg_num - 6;
            else
              ip->ext.rm.regmem = 7;
            break;
          case 5:  /* %bp */
            defaultseg = SSEG_REGNUM;
            if (memop->indexreg) {  /* (%bp,%si) or (%bp,%di) */
              ip->ext.rm.regmem = memop->indexreg->reg_num - 4;
            }
            else {
              ip->ext.rm.regmem = 6;
              if (!(memop->type & Disp)) {
                memop->type |= Disp8;   /* fake 0(%bp) */
                if (final)
                  memop->value = number_expr(0);
              }
            }
            break;
          default:  /* (%si) or (%di) */
            ip->ext.rm.regmem = memop->basereg->reg_num - 2;
            break;
        }
        ip->ext.rm.mode = mode_from_disp_size(memop->type);
      }

      else {
        /* 32/64 bit mode base register */
        if (mode_flag==CODE_64BIT && (memop->type & Disp))
          memop->type = (memop->type&Disp8) ? Disp8|Disp32S : Disp32S;

        ip->ext.rm.regmem = memop->basereg->reg_num;
        ip->ext.sib.base = memop->basereg->reg_num;
        if (memop->basereg->reg_num == EBP_REGNUM) {
          defaultseg = SSEG_REGNUM;
          if (memop->value == NULL) {
            memop->type |= Disp8;  /* fake 8-bit zero-displacement */
            if (final)
              memop->value = number_expr(0);
          }
        }
        else if (memop->basereg->reg_num == ESP_REGNUM) {
          defaultseg = SSEG_REGNUM;
        }
        ip->ext.sib.scale = memop->log2_scale;

        if (memop->indexreg == NULL) {
          ip->ext.sib.index = NO_INDEXREG;
        }
        else {
          ip->ext.sib.index = memop->indexreg->reg_num;
          ip->ext.rm.regmem = TWO_BYTE_ESCAPE;
          ip->ext.flags |= SIB_BYTE;
        }
        ip->ext.rm.mode = mode_from_disp_size(memop->type);
        if (ip->ext.rm.regmem == TWO_BYTE_ESCAPE)
          ip->ext.flags |= SIB_BYTE;
      }
    }

    if (reg_operands) {
      operand *regop = ip->op[(ip->op[0]->type & AnyReg) ? 0 :
                              ((ip->op[1]->type & AnyReg) ? 1 : 2)];

      if (final && (cpudebug & 8)) {
        printf("\tregister=%%%s(",regop->basereg->reg_name);
        print_operand_type(regop->basereg->reg_type,'|');
        printf(")\n");
      }
      if (mnemo->ext.extension_opcode != NO_EXTOPCODE) {
        ip->ext.rm.regmem = regop->basereg->reg_num;
      }
      else {
        ip->ext.rm.reg = regop->basereg->reg_num;
      }
      if (!mem_operands)
        ip->ext.rm.mode = 3;
    }

    /* insert extension opcode when available */
    if (mnemo->ext.extension_opcode != NO_EXTOPCODE)
      ip->ext.rm.reg = mnemo->ext.extension_opcode;
  }

  if (final && (cpudebug & 8)) {
    printf("\tMod/RM: mode=%d reg=%d regmem=%d\n",
           (int)ip->ext.rm.mode,(int)ip->ext.rm.reg,(int)ip->ext.rm.regmem);
    if (ip->ext.flags & SIB_BYTE) {
      printf("\tSIB:    scale=%d index=%d base=%d\n",
             (int)ip->ext.sib.scale,(int)ip->ext.sib.index,(int)ip->ext.sib.base);
    }
  }
  return defaultseg;
}


static int process_operands(instruction *ip,int final)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  int defaultseg = -1;
  int mod = mnemo->ext.opcode_modifier;
  int i;
  regsym *seg;

  if ((mod & FakeLastReg) && ip->op[0]) {
    /* convert "imul %imm,%reg" into "imul %imm,%reg,%reg" and
       convert "clr %reg" into "xor %reg,%reg" */
    int regoper = (ip->op[0]->type & Reg) ? 0 : 1;

    ip->op[regoper+1] = ip->op[regoper];
  }

  if ((mod & ShortForm) && ip->op[0]) {
    /* copy register number to base-opcode in short-format */
    int regoper = (ip->op[0]->type & (Reg|FloatReg)) ? 0 : 1;

    ip->ext.base_opcode |= ip->op[regoper]->basereg->reg_num;

    if ((mod & Deprecated) && final) {
      if (ip->op[1]) {
        /* translating to fxxxx %reg,%reg */
        cpu_error(16,mnemo->name,ip->op[1]->basereg->reg_name,
                  ip->op[0]->basereg->reg_name);
      }
      else {
        /* translating to fxxxx %reg */
        cpu_error(17,mnemo->name,ip->op[0]->basereg->reg_name);
      }
    }
  }

  else if (mod & M) {
    defaultseg = make_modrm(ip,mnemo,final);
  }

  else if (mod & (Seg2ShortForm|Seg3ShortForm)) {
    if (mnemo->ext.base_opcode==OC_POP_SREG2 && ip->op[0]->basereg && final) {
      if (ip->op[0]->basereg->reg_num == CSEG_REGNUM) {
        cpu_error(15,ip->op[0]->basereg->reg_name);  /* cannot pop %cs */
        return 0;
      }
    }
    ip->ext.base_opcode |= ip->op[0]->basereg->reg_num << 3;
  }

  else if (mnemo->ext.base_opcode == OC_MOV_ACC_DISP) {
    defaultseg = DSEG_REGNUM;
  }

  else if (mod & StrInst) {
    defaultseg = DSEG_REGNUM;
  }

  /* if a segment was explicitely specified and differs from the
     instruction's default we need to select it by another opcode prefix */
  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] != NULL) {
      if (seg = ip->op[i]->segoverride) {
        if (seg->reg_num != defaultseg) {
          if (!add_seg_prefix(ip,seg->reg_num)) {
            if (!final)
              cpu_error(2);  /* same type of prefix used twice */
            return 0;
          }
        }
      }
    }
  }

  return 1;
}


static int get_disp_bits(int t)
{
  if (t & Disp8)
    return 8;
  if (t & Disp16)
    return 16;
  if (t & (Disp32|Disp32S))
    return 32;
  return 64;
}


static int get_imm_bits(int t)
{
  if (t & (Imm8|Imm8S))
    return 8;
  if (t & Imm16)
    return 16;
  if (t & (Imm32|Imm32S))
    return 32;
  return 64;
}


static size_t get_opcode_size(instruction *ip)
{
  uint32_t c = ip->ext.base_opcode;
  size_t size = 1;

  if (c > 0xff)
    size++;
  if (c > 0xffff)
    size++;

  return size;
}  


static size_t imm_size(int t)
{
  size_t size = 0;

  if (t & Imm) {
    if (t & (Imm8|Imm8S))
      size++;
    else if (t & Imm16)
      size += 2;
    else if (t & Imm64)
      size += 8;
    else
      size += 4;
  }
  return size;
}


static size_t disp_size(int t)
{
  size_t size = 0;

  if (t & Disp) {
    if (t & Disp8)
      size++;
    else if (t & Disp16)
      size += 2;
    else if (t & Disp64)
      size += 8;
    else
      size += 4;
  }
  return size;
}


static size_t finalize_instruction(instruction *ip,section *sec,
                                   taddr pc,int final)
/* execute optimizations, calculate instruction opcode, opcode-extension,
   modrm- and sib-bytes, and return its size in bytes */
{
  mnemonic *mnemo = &mnemonics[ip->code];
  size_t size;
  int i;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i]) {
      /* optimizations */

      if (ip->op[i]->type & Imm) {
        /* use smallest immediate size */
        optimize_imm(ip,ip->op[i],sec,pc,final);
      }
      else if (ip->op[i]->type & Disp) {
        if (mnemo->ext.opcode_modifier & (Jmp|JmpByte|JmpDword)) {
          /* check jump-distances and try to use 8-bit form */
          optimize_jump(ip,ip->op[i],sec,pc,final);
        }
        else {
          /* use the smallest type for absolute displacements */
          optimize_disp(ip->op[i],sec,pc,final);
        }
      }

      if (final && ip->op[i]->scalefactor!=NULL) {
        /* have to evaluate scale factor now */
        taddr sf;

        if (!eval_expr(ip->op[i]->scalefactor,&sf,sec,pc)) {
          cpu_error(18);  /* absolute scale factor required */
          ip->op[i]->log2_scale = 0;
        }
        else {
          switch (sf) {
            case 1: sf = 0; break;
            case 2: sf = 1; break;
            case 4: sf = 2; break;
            case 8: sf = 3; break;
            default:
              cpu_error(19);  /* illegal scale factor */
              break;
          }
          ip->op[i]->log2_scale = (int)sf;
        }
      }
      else
        ip->op[i]->log2_scale = 0;
    }
  }

  /* process operation size, add prefixes when required */
  process_suffix(ip,final);

  /* set base opcode, process operands and generate modrm/sib if present */
  process_operands(ip,final);

  /* determine size of instruction */
  size = get_opcode_size(ip);
  size += ip->ext.num_prefixes;
  if (ip->ext.flags & MODRM_BYTE)
    size++;
  if (ip->ext.flags & SIB_BYTE)
    size++;
  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i]) {
      size += imm_size(ip->op[i]->type);
      size += disp_size(ip->op[i]->type);
    }
    else
      break;
  }

  if (cpudebug & 16)
    printf("%08lx: (%u) %s",(unsigned long)pc,(unsigned)size,mnemo->name);

  return size;
}


static unsigned char make_byte233(unsigned char b76,unsigned char b543,
                                  unsigned char b210)
/* make a byte from three bit-fields: bit7-6, bit5-3 and bit2-0 */
{
  return ((b76 & 3)<<6) | ((b543 & 7)<<3) | (b210&7);
}


static unsigned char *write_taddr(unsigned char *d,taddr val,size_t bits)
{
  switch (bits) {
    case 8:
      *d++ = val & 0xff;
      break;
    case 16:
    case 32:
    case 64:
      d = setval(0,d,bits>>3,val);
      break;
    default:
      ierror(0);
      break;
  }
  return d;
}


static unsigned char *output_prefixes(unsigned char *d,instruction *ip)
{
  unsigned char p;
  int i;

  for (i=0; i<MAX_PREFIXES; i++) {
    if (p = ip->ext.prefix[i])
      *d++ = p;
  }
  return d;
}


static unsigned char *output_opcodes(unsigned char *d,instruction *ip)
{
  uint32_t oc = (uint32_t)ip->ext.base_opcode;

  if (oc > 0xffff)
    *d++ = (oc >> 16) & 0xff;
  if (oc > 0xff)
    *d++ = (oc >> 8) & 0xff;
  *d++ = oc & 0xff;

  if (ip->ext.flags & MODRM_BYTE)
    *d++ = make_byte233(ip->ext.rm.mode,ip->ext.rm.reg,ip->ext.rm.regmem);

  if (ip->ext.flags & SIB_BYTE)
    *d++ = make_byte233(ip->ext.sib.scale,ip->ext.sib.index,ip->ext.sib.base);

  return d;
}


static unsigned char *output_disp(dblock *db,unsigned char *d,
                                  instruction *ip,section *sec,taddr pc)
{
  int i;
  size_t bits;
  operand *op;
  taddr val;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (op = ip->op[i]) {
      if (op->type & Disp) {
        mnemonic *mnemo = &mnemonics[ip->code];
        bits = get_disp_bits(op->type);

        if (!eval_expr(op->value,&val,sec,pc)) {
          symbol *base;

          if (find_base(op->value,&base,sec,pc) == BASE_OK) {
            if ((mnemo->ext.opcode_modifier & (Jmp|JmpByte|JmpDword))
                || (op->flags & OPER_PCREL)) {
              /* handle pc-relative displacement for jumps */
              if (!is_pc_reloc(base,sec)) {
                val = val - (pc + (d-(unsigned char *)db->data) + (bits>>3));
              }
              else {
                val -= bits>>3;
                add_extnreloc(&db->relocs,base,val,REL_PC,0,bits,
                              (int)(d-(unsigned char *)db->data));
              }
            }
            else {
              /* reloc for a normal absolute displacement */
              add_extnreloc(&db->relocs,base,val,REL_ABS,0,bits,
                            (int)(d-(unsigned char *)db->data));
            }
          }
          else
            general_error(38);  /* illegal relocation */
        }
        else {  /* constant/absolute */
          if ((mnemo->ext.opcode_modifier & (Jmp|JmpByte|JmpDword))
                || (op->flags & OPER_PCREL)) {
            /* handle pc-relative jumps to absolute labels */
            val = val - (pc + (d-(unsigned char *)db->data) + (bits>>3));
          }
        }
        d = write_taddr(d,val,bits);
      }
    }
  }
  return d;
}


static unsigned char *output_imm(dblock *db,unsigned char *d,
                                 instruction *ip,section *sec,taddr pc)
{
  int i,ot;
  size_t bits;
  operand *op;
  taddr val;

  for (i=0; i<MAX_OPERANDS; i++) {
    if (op = ip->op[i]) {
      if (ot = (op->type & Imm)) {
        bits = get_imm_bits(ot);

#if 0 /* done in optimize_imm() */
        if (bits>8 && ((ot & Imm16) && ((ot & Imm32) || (ot & Imm32S)))) {
          /* 16 or 32 bit immediate depends on current data size */
          if ((mode_flag==CODE_16BIT) ^ (ip->ext.prefix[DATA_PREFIX]!=0))
            bits = 16;
          else
            bits = 32;
        }
#endif

        if (!eval_expr(op->value,&val,sec,pc)) {
          symbol *base;

          if (find_base(op->value,&base,sec,pc) == BASE_OK) {
            add_extnreloc(&db->relocs,base,val,REL_ABS,0,bits,
                          (int)(d-(unsigned char *)db->data));
          }
          else
            general_error(38);  /* illegal relocation */
        }
        d = write_taddr(d,val,bits);
      }
    }
  }
  return d;
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
    if (s-name==6 && !strncmp(name,"code16",6)) {
      mode_flag = CODE_16BIT;
      return s;
    }
    else if (s-name==6 && !strncmp(name,"code32",6)) {
      mode_flag = CODE_32BIT;
      return s;
    }
    else if (s-name==6 && !strncmp(name,"code64",6)) {
      mode_flag = CODE_64BIT;
      return s;
    }
  }
  return start;
}


char *parse_instruction(char *s,int *inst_len,char **ext,int *ext_len,
                        int *ext_cnt)
/* parse instruction and save extension locations */
{
  hashdata data;
  char *inst = s;
  int len;

  /* reset opcode prefixes */
  memset(prefix,0,sizeof(prefix));
  prefix_cnt = 0;
  if (cpudebug & 1)
    printf("parse_instruction: \"%s\"\n",s);

  for (;;) {
    while (*s && !isspace((unsigned char)*s))
      s++;
    len = s - inst;

    if (find_namelen(mnemohash,inst,len,&data)) {
#if 0  /*@@@ need a way to support prefixes at the same line with vasm */
      mnemonic *mnemo = &mnemonics[data.idx];

      if (mnemo->ext.opcode_modifier & IsPrefix) {
        /* matched a prefix instruction, remember it and look for more */
        s = skip(s);

        if (ISIDSTART(*s)) {
          /* real opcode or another prefix follows, save current prefix */
          if (!(mnemo->ext.available & cpu_type))
            cpu_error(0);  /* instruction not supported on selected arch. */

          /* do not allow addr16 in 16-bit and addr32 in 32-bit mode */
          if (((mnemo->ext.opcode_modifier&Size16) && mode_flag==CODE_16BIT)
              || ((mnemo->ext.opcode_modifier&Size32) &&
                  mode_flag==CODE_32BIT)) {
            cpu_error(7,mnemo->name);  /* redundant prefix ignored */
          }
          else {
            /* enter new prefix */
            if (cpudebug & 1) {
              printf("\tadd prefix %02x for \"%s\"\n",
                     (unsigned)mnemo->ext.base_opcode,mnemo->name);
            }
            if (!add_prefix(mnemo->ext.base_opcode))
              cpu_error(2);  /* same type of prefix used twice */
          }
          inst = s;
          continue;  /* parse next opcode or prefix */
        }
      }
#endif
    }

    else {
      /* no direct match, so look if we can strip a suffix from it */
      if (len >= 2) {
        char x = *(s-1);

        if (x=='b' || x=='w' || x=='l' || x=='s' || x=='q' || x=='x') {
          /* a potential suffix found, save it */
          int cnt = *ext_cnt;

          ext[cnt] = s-1;
          ext_len[cnt] = 1;
          *ext_cnt = ++cnt;
          --len;
        }
      }
    }
    break;
  }

  *inst_len = len;
  if (cpudebug & 1) {
    printf("\tinst=\"%.*s\" suffix=\"%.*s\"\n",len,inst,
           *ext_cnt?ext_len[0]:0, *ext_cnt?ext[0]:emptystr);
  }
  return s;
}


int set_default_qualifiers(char **q,int *q_len)
/* fill in pointers to default qualifiers, return number of qualifiers */
{
  /* FIXME: no default size for x86 instructions? */
  return 0;
}


static regsym *parse_reg(char **pp)
/* parse a register name, return pointer to regsym when successful */
{
  char *p,*start;
  regsym *r;

  p = skip(*pp);
  if (*p++ == '%') {
    start = p;
    if (ISIDSTART(*p)) {
      p++;
      while (ISIDCHAR(*p))
        p++;
      /* special case float-registers: %st(n) */
      if (p-start==2 &&
          (!strncmp(start,"st(",3) || !strncmp(start,"ST(",3))) {
        if (isdigit((unsigned char)*(p+1)) && *(p+2)==')')
          p += 3;
      }
      if (r = find_regsym_nc(start,p-start)) {
        if ((r->reg_flags & (RegRex64|RegRex)) && mode_flag!=CODE_64BIT)
          return NULL;
        *pp = p;
        return r;
      }
    }
  }
  return NULL;
}


int parse_operand(char *p,int len,operand *op,int requirements)
/* Parses operands, reads expressions and assigns relocation types */
{
  char *start = p;
  int need_mem_oper = 0;
  int given_type,overlap;

  if (cpudebug & 2) {
    printf("parse_operand (reqs=%08lx): \"%.*s\"\n",
           (unsigned long)requirements,len,p);
  }

  /* @@@ This does no longer work, since save_symbols()/restore_symbols().
  given_type = op->parsed_type; */
  given_type = 0;  /* ... so parse the operand every time again */

  if (given_type == 0) {
    p = skip(p);

    if (*p == '%') {
      /* check for a segment override first */
      char *savedp = p;
      regsym *sreg = parse_reg(&p);

      p = skip(p);
      if (sreg!=NULL && (sreg->reg_type & (SegReg2|SegReg3)) && *p++==':') {
        op->segoverride = sreg;
        need_mem_oper = 1;  /* only memory operands may follow */
        p = skip(p);
      }
      else {
        /* no segment override - forget all we did */
        p = savedp;
      }
    }

    if (*p == '*') {
      /* indicates absolute jumps */
      p = skip(++p);
      given_type = JmpAbs;
    }
    else
      given_type = 0;

    if (*p == '%') {
      /* single register operand */
      if (need_mem_oper) {
        cpu_error(14);  /* memory operand expected */
        return PO_CORRUPT;
      }
      if (op->basereg = parse_reg(&p)) {
        op->flags |= OPER_REG;
        given_type |= op->basereg->reg_type & ~BaseIndex;
      }
      else {
        cpu_error(8);  /* unknown register specified */
        return PO_CORRUPT;
      }
    }

    else if (*p == '$') {
      /* immediate operand */
      if (need_mem_oper) {
        cpu_error(14);  /* memory operand expected */
        return PO_CORRUPT;
      }
      if (given_type & JmpAbs) {
        cpu_error(3);  /* immediate operand illegal with absolute jump */
        return PO_CORRUPT;
      }
      p++;
      op->value = parse_expr(&p);
      given_type = Imm;  /* exact size is not available at this stage */
    }

    else {
      if (*p != '(') {
        /* read displacement (or data) */
        given_type |= Disp;  /* exact size is not available at this stage */
        if ((requirements & FloatData) == FloatData) {
          op->value = parse_expr_float(&p);
          given_type |= FloatData;
        }
        else
          op->value = parse_expr(&p);
        p = skip(p);
      }

      if (*p == '(') {
        /* BaseIndex mode: read base, index, scale */
        p = skip(++p);
        if (*p != ',') {
          if (!(op->basereg = parse_reg(&p))) {
            cpu_error(4);  /* base register expected */
            return PO_CORRUPT;
          }
          p = skip(p);
        }
        if (*p == ',') {
          p = skip(++p);
          if (!(op->indexreg = parse_reg(&p))) {
            cpu_error(5);  /* scale factor without index register */
            return PO_CORRUPT;
          }
          p = skip(p);
          if (*p == ',') {
            p = skip(++p);
            op->scalefactor = parse_expr(&p);
            p = skip(p);
          }
        }
        if (*p++ != ')') {
          cpu_error(6);  /* missing ')' in baseindex addressing mode */
          return PO_CORRUPT;
        }
        given_type |= BaseIndex;
      }
    }
    p = skip(p);
    if (p - start < len)
      cpu_error(1);  /* trailing garbage in operand */
    op->parsed_type = given_type;  /* remember type for next try */
  }

  overlap = given_type & requirements;
  if (cpudebug & 2) {
    printf("\ttype given: %08lx  overlap with reqs: %08lx - ",
           (unsigned long)given_type,(unsigned long)overlap);
  }
  if ((overlap & ~JmpAbs) &&
      (given_type & (BaseIndex|JmpAbs)) ==
       (overlap & (BaseIndex|JmpAbs))) {
    /* ok, parsed operand type matches requirements */
    if (cpudebug & 2)
      printf("MATCHED!\n");
    op->type = overlap;

    /* @@@ check register types??? */
    /* If given types r0 and r1 are registers they must be of the same type
       unless the expected operand type register overlap is null.
       Note that Acc in a template matches every size of reg.  */
    return PO_MATCH;
  }

  if (cpudebug & 2)
    printf("failed\n");
  return PO_NOMATCH;
}


void init_instruction_ext(instruction_ext *ixp)
{
  int i;

  memset(ixp,0,sizeof(instruction_ext));
  if (prefix_cnt) {
    ixp->num_prefixes = prefix_cnt;
    for (i=0; i<MAX_PREFIXES; i++)
      ixp->prefix[i] = prefix[i];
  }
  ixp->last_size = -1;
}


size_t instruction_size(instruction *realip,section *sec,taddr pc)
/* Calculate the size of the current instruction; must be identical
   to the data created by eval_instruction. */
{
  mnemonic *mnemo = &mnemonics[realip->code];
  int i,diff;
  size_t size;

  /* assign the suffix, which was unknown during operand evaluation */
  if (!(realip->ext.flags & SUFFIX_CHECKED)) {
    if (cpudebug & 4) {
      printf("instruction_size():");
      print_operands(realip,pc);
    }
    /* set base opcode from mnemonic */
    realip->ext.base_opcode = mnemo->ext.base_opcode;

    for (i=0; i<MAX_OPERANDS; i++) {
      if (realip->op[i]) {
        if (realip->op[i]->flags & OPER_REG)
          realip->ext.flags |= HAS_REG_OPER;
      }
      else
        break;
    }

    /* try to assign a suffix */
    assign_suffix(realip);
    realip->ext.flags |= SUFFIX_CHECKED;

    /* and check if instruction supports suffix and matches selected cpu */
    if ((mnemo->ext.opcode_modifier & suffix_flag(realip)) ||
        (mnemo->ext.available & cpu_type)!=mnemo->ext.available) {
      if (!find_next_mnemonic(realip))
        cpu_error(0);  /* instruction not supported on selected arch. */
    }
  }

  /* work on a copy of the current instruction and finalize it */
  size = finalize_instruction(copy_inst(realip),sec,pc,0);

  if (realip->ext.last_size>=0 && (diff=realip->ext.last_size-(int)size)!=0) {
    if (diff > 0) {
      if (cpudebug & 16)
        printf(" (%d bytes gained)\n",diff);
      realip->ext.flags |= POSOPT;
    }
    else {
      if (cpudebug & 16)
        printf(" (%d bytes lost)\n",-diff);
      realip->ext.flags |= NEGOPT;
    }
  }
  else {
    if (cpudebug & 16)
      printf("\n");
  }
  realip->ext.last_size = size;

  return size;
}


dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
/* Convert an instruction into a DATA atom including relocations,
   if necessary. */
{
  dblock *db = new_dblock();
  unsigned char *d;

  db->size = finalize_instruction(ip,sec,pc,1);
  db->data = mymalloc(db->size);
  if (cpudebug & 16)
    printf("\n");
  if (cpudebug & 4) {
    printf("eval_instruction():");
    print_operands(ip,pc);
  }
  d = output_prefixes(db->data,ip);
  d = output_opcodes(d,ip);
  d = output_disp(db,d,ip,sec,pc);
  d = output_imm(db,d,ip,sec,pc);

  return db;
}


dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
/* Create a dblock (with relocs, if necessary) for size bits of data. */
{
  dblock *db = new_dblock();
  taddr val;
  tfloat flt;

  db->size = bitsize >> 3;
  db->data = mymalloc(db->size);

  if (type_of_expr(op->value) == FLT) {
    if (!eval_expr_float(op->value,&flt))
      general_error(60);  /* cannot evaluate floating point */

    switch (bitsize) {
      case 32:
        conv2ieee32(0,db->data,flt);
        break;
      case 64:
        conv2ieee64(0,db->data,flt);
        break;
      default:
	cpu_error(20,bitsize);  /* illegal bitsize */
        break;
    }
  }
  else {
    if (!eval_expr(op->value,&val,sec,pc)) {
      symbol *base;
      int btype;
    
      btype = find_base(op->value,&base,sec,pc);
      if (base)
        add_extnreloc(&db->relocs,base,val,
                      btype==BASE_PCREL?REL_PC:REL_ABS,
                      0,bitsize,0);
      else if (btype != BASE_NONE)
        general_error(38);  /* illegal relocation */
    }
    write_taddr(db->data,val,bitsize);
  }

  return db;
}


int init_cpu()
{
  int i;
  regsym *r;

  if (!(cpu_type & CPU64))
    cpu_type |= CPUNo64;

  for (i=0; i<mnemonic_cnt; i++) {
    if (!strcmp(mnemonics[i].name,"addr16"))
      OC_ADDR_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"data16"))
      OC_DATA_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"lock"))
      OC_LOCK_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"wait"))
      OC_WAIT_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"cs"))
      OC_CSEG_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"ds"))
      OC_DSEG_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"es"))
      OC_ESEG_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"fs"))
      OC_FSEG_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"gs"))
      OC_GSEG_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"ss"))
      OC_SSEG_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"rep"))
      OC_REPE_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"repne"))
      OC_REPNE_PREFIX = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"pop") &&
             mnemonics[i].operand_type[0] == SegReg2)
      OC_POP_SREG2 = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"mov") &&
             mnemonics[i].operand_type[1] == Acc)
      OC_MOV_ACC_DISP = (unsigned char)mnemonics[i].ext.base_opcode;
    else if (!strcmp(mnemonics[i].name,"jmp") &&
             mnemonics[i].operand_type[0] == Disp)
      OC_JMP_DISP = (unsigned char)mnemonics[i].ext.base_opcode;
  }

  /* define all register symbols */
  for (r=x86_regsyms; r->reg_name!=NULL; r++)
    add_regsym(r);

  return 1;
}


int cpu_args(char *p)
{
  if (!strncmp(p,"-cpudebug=",7)) {
    if (!sscanf(p+10,"%li",&cpudebug))
      return 0;
  }
  else if (!strncmp(p,"-m",2)) {
    p += 2;
    if (!strcmp(p,"8086")) cpu_type = CPU086;
    else if (!strcmp(p,"i186")) cpu_type = CPU086|CPU186;
    else if (!strcmp(p,"i286")) cpu_type = CPU086|CPU186|CPU286;
    else if (!strcmp(p,"i386")) cpu_type = CPU086|CPU186|CPU286|CPU386;
    else if (!strcmp(p,"i486")) cpu_type = CPU086|CPU186|CPU286|CPU386|CPU486;
    else if (!strcmp(p,"i586")) cpu_type = CPU086|CPU186|CPU286|CPU386|CPU486|CPU586|CPUMMX;
    else if (!strcmp(p,"i686")) cpu_type = CPU086|CPU186|CPU286|CPU386|CPU486|CPU586|CPU686|CPUMMX|CPUSSE;
    else if (!strcmp(p,"pentium")) cpu_type = CPU086|CPU186|CPU286|CPU386|CPU486|CPU586|CPUMMX;
    else if (!strcmp(p,"pentiumpro")) cpu_type = CPU086|CPU186|CPU286|CPU386|CPU486|CPU586|CPU686|CPUMMX|CPUSSE;
    else if (!strcmp(p,"pentium4")) cpu_type = CPU086|CPU186|CPU286|CPU386|CPU486|CPU586|CPU686|CPUP4|CPUMMX|CPUSSE|CPUSSE2;
    else if (!strcmp(p,"k6")) cpu_type = CPU086|CPU186|CPU286|CPU386|CPU486|CPU586|CPUK6|CPUMMX|CPU3dnow;
    else if (!strcmp(p,"athlon")) cpu_type = CPU086|CPU186|CPU286|CPU386|CPU486|CPU586|CPU686|CPUK6|CPUAthlon|CPUMMX|CPU3dnow;
    else if (!strcmp(p,"sledgehammer")) cpu_type = CPU086|CPU186|CPU286|CPU386|CPU486|CPU586|CPU686|CPUK6|CPUAthlon|CPUSledgehammer|CPUMMX|CPU3dnow|CPUSSE|CPUSSE2;
    else if (!strcmp(p,"64")) {
      /* 64 bits architecture */
      cpu_type = CPUAny|CPU64;
      bytespertaddr = 8;
    }
    else return 0;
  }
  return 1;
}
