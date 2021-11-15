/*
** cpu.c Motorola M68k, CPU32 and ColdFire cpu-description file
** (c) in 2002-2021 by Frank Wille
*/

#include <math.h>
#include "vasm.h"
#include "error.h"

#include "operands.h"

struct specreg SpecRegs[] = {
#include "specregs.h"
};
static int specreg_cnt = sizeof(SpecRegs)/sizeof(SpecRegs[0]);

mnemonic mnemonics[] = {
#include "opcodes.h"
};
int mnemonic_cnt = sizeof(mnemonics)/sizeof(mnemonics[0]);

struct cpu_models models[] = {
#include "cpu_models.h"
};
int model_cnt = sizeof(models)/sizeof(models[0]);


char *cpu_copyright="vasm M68k/CPU32/ColdFire cpu backend 2.4b (c) 2002-2021 Frank Wille";
char *cpuname = "M68k";
int bitsperbyte = 8;
int bytespertaddr = 4;

int m68k_mid = 1;                     /* default a.out MID: 68000/68010 */

static uint32_t cpu_type = m68000;
static expr *baseexp[7];              /* basereg: expression loaded to reg. */
static signed char sdreg = -1;        /* current small-data base register */
static signed char last_sdreg = -1;
static unsigned char phxass_compat = 0;
static unsigned char devpac_compat = 0;
static unsigned char gas = 0;         /* true enables GNU-as mnemonics */
static unsigned char sgs = 0;         /* true enables & as immediate prefix */
static unsigned char no_fpu = 0;      /* true: FPU code/direct. disallowed */
static unsigned char elfregs = 0;     /* true: %Rn instead of Rn reg. names */
static unsigned char fpu_id = 1;      /* default coprocessor id for FPU */
static unsigned char opt_gen = 1;     /* generic optimizations (not Devpac) */
static unsigned char opt_movem = 0;   /* MOVEM Rn -> MOVE Rn */
static unsigned char opt_pea = 0;     /* MOVE.L #x,-(sp) -> PEA x */
static unsigned char opt_clr = 0;     /* MOVE #0,<ea> -> CLR <ea> */
static unsigned char opt_st = 0;      /* MOVE.B #-1,<ea> -> ST <ea> */
static unsigned char opt_lsl = 0;     /* LSL #1,Dn -> ADD Dn,Dn */
static unsigned char opt_mul = 0;     /* MULU/MULS #n,Dn -> LSL/ASL #n,Dn */
static unsigned char opt_div = 0;     /* DIVU/DIVS.L #n,Dn -> LSR/ASR #n,Dn */
static unsigned char opt_fconst = 1;  /* Fxxx.D #m,FPn -> Fxxx.S #m,FPn */
static unsigned char opt_brajmp = 0;  /* branch to different sect. into jump */
static unsigned char opt_pc = 1;      /* <label> -> (<label>,PC) */
static unsigned char opt_bra = 1;     /* B<cc>.L -> B<cc>.W -> B<cc>.B */
static unsigned char opt_allbra = 0;  /* also optimizes sized branches */
static unsigned char opt_jbra = 0;    /* JMP/JSR <ext> -> BRA.L/BSR.L (020+) */
static unsigned char opt_disp = 1;    /* (0,An) -> (An), etc. */
static unsigned char opt_abs = 1;     /* optimize absolute addreses to 16bit */
static unsigned char opt_moveq = 1;   /* MOVE.L #x,Dn -> MOVEQ #x,Dn */
static unsigned char opt_nmovq = 0;   /* MOVE.L #x,Dn -> MOVEQ #x,Dn & NEG Dn*/
static unsigned char opt_quick = 1;   /* ADD/SUB #x,Rn -> ADDQ/SUBQ #x,Rn */
static unsigned char opt_branop = 1;  /* BRA.B *+2 -> NOP */
static unsigned char opt_bdisp = 1;   /* base displacement optimization */
static unsigned char opt_odisp = 1;   /* outer displacement optimization */
static unsigned char opt_lea = 1;     /* ADD/SUB #x,An -> LEA (x,An),An */
static unsigned char opt_lquick = 1;  /* LEA (x,An),An -> ADDQ/SUBQ #x,An */
static unsigned char opt_immaddr = 1; /* <op>.L #x,An -> <op>.W #x,An */
static unsigned char opt_speed = 0;   /* optimize for speed, code may grow */
static unsigned char opt_size = 0;    /* optimize for size, even when slower */
static unsigned char opt_sc = 0;      /* external JMP/JSR are 16-bit PC-rel. */
static unsigned char opt_sd = 0;      /* small data opts: abs.L -> (d16,An) */
static unsigned char no_opt = 0;      /* don't optimize at all! */
static unsigned char warn_opts = 0;   /* warn on optimizations/translations */
static unsigned char convert_brackets = 0;  /* convert [ into ( for <020 */
static unsigned char typechk = 1;     /* check value types and ranges */
static unsigned char ign_unambig_ext = 0;  /* don't check unambig. size ext. */
static unsigned char ign_unsized_ext = 0;  /* don't check size on unsized */
static unsigned char regsymredef = 0; /* allow redefinition of reg. symbols */
static unsigned char kick1hunks = 0;  /* no optim. to 32-bit PC-displacem. */
static unsigned char no_dpc = 0;      /* abs. PC-displacments not allowed */
static unsigned char extsd = 0;       /* small-data with ext. addr. modes */
static char current_ext;              /* extension of current parsed inst. */

static char b_str[] = "b";
static char w_str[] = "w";
static char l_str[] = "l";
static char s_str[] = "s";
static char d_str[] = "d";
static char x_str[] = "x";
static char p_str[] = "p";

static char optc_name[] = "__OPTC";
static char cpu_name[] = "__CPU";
static char mmu_name[] = "__MMU";
static char fpu_name[] = "__FPU";
static char g2_name[] = "__G2";
static char lk_name[] = "__LK";
static char movembytes_name[] = "_MOVEMBYTES";
static char movemregs_name[] = "_MOVEMREGS";
static char movemsize_name[] = " MOVEMSIZE";

static symbol *movembytes,*movemregs,*movemsize;

static int OC_JMP,OC_JSR,OC_MOVEQ,OC_MOV3Q,OC_LEA,OC_PEA,OC_SUBA,OC_CLR;
static int OC_ST,OC_ADDQ,OC_SUBQ,OC_ADDA,OC_ADD,OC_BRA,OC_BSR,OC_TST;
static int OC_NOT,OC_NOOP,OC_FNOP,OC_MOVEA,OC_EXT,OC_MVZ,OC_MOVE;
static int OC_ASRI,OC_LSRI,OC_ASLI,OC_LSLI,OC_NEG;
static int OC_FMOVEMTOLIST,OC_FMOVEMTOSPEC,OC_FMOVEMFROMSPEC;
static int OC_FMUL,OC_FSMUL,OC_FDMUL,OC_FSGLMUL,OC_LOAD,OC_SWAP;

static struct {
  int *var;
  const char *name;
  int16_t optype[2];
} code_tab[] = {
  /* Note: keep same order as in mnemonics table! */
  &OC_ADD,              "add",    DA,0,
  &OC_ADDA,             "adda",   0,0,
  &OC_ADDQ,             "addq",   0,AD,
  &OC_ASLI,             "asl",    QI,0,
  &OC_ASRI,             "asr",    QI,0,
  &OC_BRA,              "bra",    0,0,
  &OC_BSR,              "bsr",    0,0,
  &OC_CLR,              "clr",    0,0,
  &OC_EXT,              "ext",    0,0,
  &OC_FMOVEMTOLIST,     "fmovem", MR,FL,
  &OC_FMOVEMFROMSPEC,   "fmovem", FS,AM,
  &OC_FMOVEMTOSPEC,     "fmovem", MA,FS,
  &OC_FMUL,             "fmul",   FA,F_,
  &OC_FSMUL,            "fsmul",  FA,F_,
  &OC_FDMUL,            "fdmul",  FA,F_,
  &OC_FNOP,             "fnop",   0,0,
  &OC_FSGLMUL,          "fsglmul",FA,F_,
  &OC_JMP,              "jmp",    0,0,
  &OC_JSR,              "jsr",    0,0,
  &OC_LEA,              "lea",    0,0,
  &OC_LOAD,             "load",   0,0,
  &OC_LSLI,             "lsl",    QI,0,
  &OC_LSRI,             "lsr",    QI,0,
  &OC_MOV3Q,            "mov3q",  0,0,
  &OC_MOVE,             "move",   DA,AD,
  &OC_MOVEA,            "movea",  0,0,
  &OC_MOVEQ,            "moveq",  0,0,
  &OC_MVZ,              "mvz",    0,0,
  &OC_NEG,              "neg",    D_,0,
  &OC_NOT,              "not",    0,0,
  &OC_PEA,              "pea",    0,0,
  &OC_ST,               "st",     AD,0,
  &OC_SUBA,             "suba",   0,0,
  &OC_SUBQ,             "subq",   0,AD,
  &OC_SWAP,             "swap",   D_,0,
  &OC_TST,              "tst",    0,0,
  &OC_NOOP,             " no-op", 0,0
};

/* Serveral instruction copies allow optimizations to generate 
   additional instructions.
   The ipslot has to be reset to 0, before using copy_instruction(),
   ip_singleop() and ip_dualop(). */
#define MAX_IP_COPIES 4
static int ipslot;
static instruction newip[MAX_IP_COPIES];
static operand newop[MAX_IP_COPIES][MAX_OPERANDS];


operand *new_operand(void)
{
  return mycalloc(sizeof(operand));
}


static void free_op_exp(operand *op)
{
  if (op) {
    if (op->value[0])
      free_expr(op->value[0]);
    if (op->value[1])
      free_expr(op->value[1]);
    op->value[0] = op->value[1] = NULL;
  }
}


static void free_operand(operand *op)
{
  if (op) {
    free_op_exp(op);
    myfree(op);
  }
}


static operand *clr_operand(operand *op)
{
  memset(op,0,sizeof(operand));
  return op;
}


void init_instruction_ext(instruction_ext *ixp)
{
  ixp->un.real.flags = 0;
  ixp->un.real.last_size = -1;
  ixp->un.real.orig_ext = -1;
}


static instruction *clr_instruction(instruction *ip)
{
  memset(ip,0,sizeof(instruction));
  return ip;
}


static instruction *copy_instruction(instruction *sip)
/* copy an instruction and its operands */
{
  instruction *dip = &newip[ipslot];
  operand *dop = newop[ipslot];
  int i;

  if (ipslot >= MAX_IP_COPIES)
    ierror(0);
  dip->code = sip->code;
  dip->qualifiers[0] = sip->qualifiers[0];
  for (i=0; i<MAX_OPERANDS; i++) {
    if (sip->op[i] != NULL) {
      dip->op[i] = &dop[i];
      *dip->op[i] = *sip->op[i];
    }
    else
      dip->op[i] = NULL;
  }
  dip->ext = sip->ext;
  ++ipslot;
  return dip;
}


int m68k_data_operand(int bits)
/* return data operand type for these number of bits */
{
  switch (bits) {
    case 8: return OP_D8;
    case 16: return OP_D16;
    case 32: return OP_D32;
    case 64: return OP_D64;
    case OPSZ_FLOAT|32: return OP_F32;
    case OPSZ_FLOAT|64: return OP_F64;
    case OPSZ_FLOAT|96: return OP_F96;
  }
  cpu_error(38,OPSZ_BITS(bits)); /* data obj. with n bits size are not supp. */
  return 0;
}


int m68k_available(int idx)
/* Check if mnemonic is available for selected cpu_type. */
{
  return (mnemonics[idx].ext.available & cpu_type) != 0;
}


int m68k_operand_optional(operand *op,int type)
/* Check if operand is optional */
{
  if (optypes[type].flags & OTF_OPT) {
    memset(op,0,sizeof(operand));
    op->mode = -1;  /* @@@FIXME! */
    return 1;
  }
  return 0;
}


static int phxass_cpu_num(uint32_t type)
{
  static int cpus[] = {
    68000,68010,68020,68030,68040,68060
  };
  int i;

  for (i=5; i>=0; i--)
    if (type & (1L<<i))
      return cpus[i];

  return 0;  /* not a cpu known to PhxAss, like for example ColdFire */
}


static void set_optc_symbol(void)
{
  /* set PhxAss __OPTC symbol from current optimization flags */
  taddr optc = 0;

  if (!no_opt) {
    if (opt_disp && opt_abs && opt_moveq && opt_lea && opt_immaddr)
      optc |= 0x001;
    if (opt_pc)
      optc |= 0x002;
    if (opt_quick && opt_lquick)
      optc |= 0x004;
    if (opt_bra && opt_brajmp)
      optc |= 0x108;  /* T is always set together with B */
    if (opt_lsl)
      optc |= 0x010;
    if (opt_pea)
      optc |= 0x020;
    if (opt_clr && opt_st && opt_fconst)
      optc |= 0x040;
    if (opt_gen)
      optc |= 0x080;
    if (opt_movem)
      optc |= 0x200;
  }

  set_internal_abs(optc_name,optc);
}


static void set_g2_symbol(void)
{
  /* Devpac __G2 symbol
   * Bits 0-7:   version and features (e.g. 43) - we don't use it!
   * Bits 8-15:  cpu type - 68000
   * Bits 16-23: host system - we set all bits to indicate an unknown host
   */
  set_internal_abs(g2_name,0xff0000|((phxass_cpu_num(cpu_type)-68000)<<8));
}


static void check_apollo_conflicts(void)
{
  static int apollo_checks_done = 0;

  if (!apollo_checks_done) {
    /* When the Apollo cpu type is enabled for the first time, we have
       to make sure that "load" is disabled as a directive. */
    hashdata data;

    if (find_name_nc(dirhash,mnemonics[OC_LOAD].name,&data)) {
      rem_hashentry(dirhash,mnemonics[OC_LOAD].name,nocase);
      /*cpu_error(63,mnemonics[OC_LOAD].name);*/
    }
    apollo_checks_done = 1;
  }
}


void cpu_opts(void *opts)
/* set cpu options for following atoms */
{
  int cmd = ((optcmd *)opts)->cmd;
  int arg = ((optcmd *)opts)->arg;

  if (cmd>OCMD_NOOPT && cmd<OCMD_OPTWARN && arg!=0)
    no_opt = 0;

  switch (cmd) {
    case OCMD_NOP:
      break;
    case OCMD_CPU:
      cpu_type = arg;
      if (phxass_compat) {
        set_internal_abs(cpu_name,phxass_cpu_num(cpu_type));
        set_internal_abs(mmu_name,(cpu_type & mmmu)!=0);
      }
      if (devpac_compat)
        set_g2_symbol();
      set_internal_abs(vasmsym_name,cpu_type&CPUMASK);
      if (cpu_type & apollo)
        check_apollo_conflicts();
      break;
    case OCMD_FPU:
      fpu_id = arg;
      if (phxass_compat)
        set_internal_abs(fpu_name,(cpu_type & mfloat)?fpu_id:0);
      break;
    case OCMD_SDREG:
      sdreg = arg;
      break;

    case OCMD_NOOPT: no_opt=arg; break;
    case OCMD_OPTGEN: opt_gen=arg; break;
    case OCMD_OPTMOVEM: opt_movem=arg; break;
    case OCMD_OPTPEA: opt_pea=arg; break;
    case OCMD_OPTCLR: opt_clr=arg; break;
    case OCMD_OPTST: opt_st=arg; break;
    case OCMD_OPTLSL: opt_lsl=arg; break;
    case OCMD_OPTMUL: opt_mul=arg; break;
    case OCMD_OPTDIV: opt_div=arg; break;
    case OCMD_OPTFCONST: opt_fconst=arg; break;
    case OCMD_OPTBRAJMP: opt_brajmp=arg; break;
    case OCMD_OPTJBRA: opt_jbra=arg; break;
    case OCMD_OPTPC: opt_pc=arg; break;
    case OCMD_OPTBRA: opt_bra=arg; break;
    case OCMD_OPTDISP: opt_disp=arg; break;
    case OCMD_OPTABS: opt_abs=arg; break;
    case OCMD_OPTMOVEQ: opt_moveq=arg; break;
    case OCMD_OPTNMOVQ: opt_nmovq=arg; break;
    case OCMD_OPTQUICK: opt_quick=arg; break;
    case OCMD_OPTBRANOP: opt_branop=arg; break;
    case OCMD_OPTBDISP: opt_bdisp=arg; break;
    case OCMD_OPTODISP: opt_odisp=arg; break;
    case OCMD_OPTLEA: opt_lea=arg; break;
    case OCMD_OPTLQUICK: opt_lquick=arg; break;
    case OCMD_OPTIMMADDR: opt_immaddr=arg; break;
    case OCMD_OPTSPEED: opt_speed=arg; break;
    case OCMD_OPTSIZE: opt_size=arg; break;
    case OCMD_SMALLCODE: opt_sc=arg; break;
    case OCMD_SMALLDATA: opt_sd=arg; break;

    case OCMD_OPTWARN: warn_opts=arg; break;
    case OCMD_CHKPIC: pic_check=arg; break;
    case OCMD_CHKTYPE: typechk=arg; break;
    case OCMD_NOWARN: no_warn=arg; break;

    default: ierror(0); break;
  }
}


static void add_cpu_opt(section *s,int cmd,int arg)
{
  if (s || current_section) {
    optcmd *new = mymalloc(sizeof(optcmd));

    new->cmd = cmd;
    new->arg = arg;
    add_atom(s,new_opts_atom(new));
    cpu_opts(new);
  }
  else {
    /* no section known at this point, so set the option immediately: it will
       automatically become the initial option of the first section */
    optcmd o;

    o.cmd = cmd;
    o.arg = arg;
    cpu_opts(&o);
  }
}


static void cpu_opts_optinit(section *s)
/* create initial optimization atoms */
{
  add_cpu_opt(s,OCMD_NOOPT,no_opt);
  add_cpu_opt(s,OCMD_OPTGEN,opt_gen);
  add_cpu_opt(s,OCMD_OPTMOVEM,opt_movem);
  add_cpu_opt(s,OCMD_OPTPEA,opt_pea);
  add_cpu_opt(s,OCMD_OPTCLR,opt_clr);
  add_cpu_opt(s,OCMD_OPTST,opt_st);
  add_cpu_opt(s,OCMD_OPTLSL,opt_lsl);
  add_cpu_opt(s,OCMD_OPTMUL,opt_mul);
  add_cpu_opt(s,OCMD_OPTDIV,opt_div);
  add_cpu_opt(s,OCMD_OPTFCONST,opt_fconst);
  add_cpu_opt(s,OCMD_OPTBRAJMP,opt_brajmp);
  add_cpu_opt(s,OCMD_OPTJBRA,opt_jbra);
  add_cpu_opt(s,OCMD_OPTPC,opt_pc);
  add_cpu_opt(s,OCMD_OPTBRA,opt_bra);
  add_cpu_opt(s,OCMD_OPTDISP,opt_disp);
  add_cpu_opt(s,OCMD_OPTABS,opt_abs);
  add_cpu_opt(s,OCMD_OPTMOVEQ,opt_moveq);
  add_cpu_opt(s,OCMD_OPTNMOVQ,opt_nmovq);
  add_cpu_opt(s,OCMD_OPTQUICK,opt_quick);
  add_cpu_opt(s,OCMD_OPTBRANOP,opt_branop);
  add_cpu_opt(s,OCMD_OPTBDISP,opt_bdisp);
  add_cpu_opt(s,OCMD_OPTODISP,opt_odisp);
  add_cpu_opt(s,OCMD_OPTLEA,opt_lea);
  add_cpu_opt(s,OCMD_OPTLQUICK,opt_lquick);
  add_cpu_opt(s,OCMD_OPTIMMADDR,opt_immaddr);
  add_cpu_opt(s,OCMD_OPTSPEED,opt_speed);
  add_cpu_opt(s,OCMD_OPTSIZE,opt_size);
  add_cpu_opt(s,OCMD_SMALLCODE,opt_sc);
  add_cpu_opt(s,OCMD_SMALLDATA,opt_sd);

  if (phxass_compat)
    set_optc_symbol();
}


void cpu_opts_init(section *s)
/* set initial cpu opts */
{
  add_cpu_opt(s,OCMD_CPU,cpu_type);
  add_cpu_opt(s,OCMD_FPU,fpu_id);
  add_cpu_opt(s,OCMD_SDREG,sdreg);
  cpu_opts_optinit(s);
  add_cpu_opt(s,OCMD_OPTWARN,warn_opts);
  add_cpu_opt(s,OCMD_CHKPIC,pic_check);
  add_cpu_opt(s,OCMD_CHKTYPE,typechk);
  add_cpu_opt(s,OCMD_NOWARN,no_warn);
}


void print_cpu_opts(FILE *f,void *opts)
{
  static const char *ocmds[] = {
    "opt generic","opt movem","opt pea","opt clr","opt st","opt lsl",
    "opt mul","opt div","opt float const","opt branch to jump",
    "opt jump to longbranch","opt pc-relative","opt branch",
    "opt displacement","opt absolute","opt moveq","opt neg.moveq",
    "opt quick", "opt branch to nop","opt base disp","opt outer disp",
    "opt adda/subq to lea","opt lea to addq/subq","opt immediate areg",
    "opt for speed","opt for size","opt small code","opt small data",
    "warn about optimizations","PIC check","type and range checks",
    "hide all warnings"
  };
  static const char *cpus[32] = {
    "m68000","m68010","m68020","m68030","m68040","m68060",
    "m6888x","m68851","cpu32",
    "CF-ISA_A","CF-ISA_A+","CF-ISA_B","CF-ISA_C",
    "CF-hwdiv","CF-MAC","CF-EMAC","CF-USP","CF-FPU","CF-MMU",NULL,
    "ac68080",
    NULL
  };
  int cmd = ((optcmd *)opts)->cmd;
  int arg = ((optcmd *)opts)->arg;

  fprintf(f,"opts: ");
  if (cmd == OCMD_CPU) {
    int i;

    arg &= CPUMASK;
    fprintf(f,"cpu types:");
    for (i=0; i<32; i++) {
      if ((arg & (1<<i)) && cpus[i])
        fprintf(f," %s",cpus[i]);
    }
  }
  else if (cmd == OCMD_NOP)
    fprintf(f,"none");
  else if (cmd == OCMD_FPU)
    fprintf(f,"fpu id %d (f%xxx)",arg,(unsigned)arg<<1);
  else if (cmd == OCMD_SDREG) {
    if (arg >= 0)
      fprintf(f,"small data base reg is a%d",arg);
    else
      fprintf(f,"small data is disabled");
  }
  else if (cmd == OCMD_NOOPT)
    fprintf(f,"optimizations %sabled",arg?"dis":"en");
  else
    fprintf(f,"%s (%d)",ocmds[cmd-OCMD_OPTGEN],arg);
}


static void conv2ieee80(int be,uint8_t *buf,tfloat f)
/* extended precision */
/* @@@ Warning: precision is lost! Converting to double precision. */
{
  uint64_t man;
  uint32_t exp;
  union {
    double dp;
    uint64_t x;
  } conv;

  conv.dp = (double)f;
  if (conv.x == 0) {
    memset(buf,0,12);  /* 0.0 */
  }
  else if (conv.x == 0x8000000000000000LL) {
    if (be) {
      buf[0] = 0x80;
      memset(buf+1,0,11);  /* -0.0 */
    }
    else {
      buf[11] = 0x80;
      memset(buf,0,11);  /* -0.0 */
    }
  }
  else {
    man = ((conv.x & 0xfffffffffffffLL) << 11) | 0x8000000000000000LL;
    exp = ((conv.x >> 52) & 0x7ff) - 0x3ff + 0x3fff;
    if (be) {
      buf[0] = ((conv.x >> 56) & 0x80) | (exp >> 8);
      buf[1] = exp & 0xff;
      buf[2] = 0;
      buf[3] = 0;
      setval(1,buf+4,8,man);
    }
    else {
      buf[11] = ((conv.x >> 56) & 0x80) | (exp >> 8);
      buf[10] = exp & 0xff;
      buf[9] = 0;
      buf[8] = 0;
      setval(0,buf,8,man);
    }
  }
}


static void conv2packed(unsigned char *buf,long double f)
{
  /* @@@@@@@ to be implemented */
}


static taddr reverse(uint32_t v,int size)
/* reverse bit-order in v */
{
  int i;
  uint32_t r = 0;

  for (i=0; i<size; i++) {
    r <<= 1;
    if (v & (1L<<i))
      r++;
  }
  return (taddr)r;
}


static int cntones(taddr v,int bits)
/* count number of bits set */
{
  int i,r;

  for (i=0,r=0; i<bits; i++) {
    r += v & 1;
    v >>= 1;
  }
  return r;
}


static int lsbit(taddr v,int start,int end)
/* return index of first least significant bit set */
{
  int i;

  for (i=start; i<end; i++) {
    if (v & (1<<i))
      break;
  }
  return i;
}


static int msbit(taddr v,int start,int end)
/* return index of first most significant bit set */
{
  int i;

  for (i=start; i>end; i--) {
    if (v & (1<<i))
      break;
  }
  return i;
}


static int getextcode(char c)
{
  switch (tolower((unsigned char)c)) {
    case 'b':
      return EXT_BYTE;
    case 'w':
      return EXT_WORD;
    case 'l':
      return EXT_LONG;
    case 's':
      return EXT_SINGLE;
    case 'd':
      return EXT_DOUBLE;
    case 'x':
      return EXT_EXTENDED;
    case 'p':
      return EXT_PACKED;
  }
  return 0;
}


static int getmacextcode(char c)
{
  switch (tolower((unsigned char)c)) {
    case 'l':
      return EXT_LOWER;
    case 'u':
      return EXT_UPPER;
  }
  return 0;
}


static int read_extension(char **s,int extcode)
{
  char *p = *s;

  if (*p++ == '.') {
    int x = getextcode(*p++);

    if (x) {
      extcode = x;
      *s = p;
    }
  }
  return extcode;
}


static int is_float_ext(void)
{
  switch (current_ext) {
    case 's':
    case 'd':
    case 'x':
    case 'p':
      return 1;
  }
  return 0;
}


static uint16_t lc_ext_to_size(char ext)
/* convert lower-case extension character to a SIZE_xxx code */
{
  switch (ext) {
    case 'b': return SIZE_BYTE;
    case 'w': return SIZE_WORD;
    case 'l': return SIZE_LONG;
    case 's': return SIZE_SINGLE;
    case 'd': return SIZE_DOUBLE;
    case 'x': return SIZE_EXTENDED;
    case 'p': return SIZE_PACKED;
    case 'q': return SIZE_DOUBLE;
  }
  return 0;
}


static int branch_size(char ext)
/* branch size for each size-extension code */
{
  switch (ext) {
    case 'b':
    case 's':
      return 0;
    case 'l':
      return 4;
  }
  return 2;
}


int ext_unary_eval(int type,taddr val,taddr *result,int cnst)
{
  switch (type) {
    case CNTONES:
      *result = cnst ? (taddr)cntones(val,32) : 0;
      return 1;
    default:
      break;
  }
  return 0;  /* unknown type */
}


static uint16_t eval_rlsymbol(char **start)
/* Parse and evaluate a register list symbol, return its value.
   Return zero otherwise. May cause an error message on illegal symbol type. */
{
  symbol *sym;
  char *name;
  taddr val = 0;

  if (name = parse_symbol(start)) {
    if ((sym = find_symbol(name)) &&
        (sym->flags & REGLIST) && sym->type==EXPRESSION) {
      if (!eval_expr(sym->expr,&val,NULL,0))
        ierror(0);  /* REGLIST must be constant */
    }
    else
      cpu_error(66);  /* not a valid register list symbol */
    myfree(name);
  }
  return (uint16_t)val;
}


static signed char getreg(char **start,int indexreg)
/* checks stream for a data or address register (might include extension)
   return -1 on failure, otherwise return a bit-field containing
   the following information:
  Bit7 6     5     4     3         2    1    0
   ---------------------------------------------
  | 0 | extension (1-7) | An reg. | reg. number |
   --------------------------------------------- */
{
  char *s = *start;
  char *p = NULL;
  char *loc,*q;
  signed char reg = -1;
  regsym *sym;

  if (loc = get_local_label(&s)) {
    p = loc;
    q = loc + strlen(loc);
  }
  else if (ISIDSTART(*s) || (elfregs && *s=='%')) {
    p = s++;
    while (ISIDCHAR(*s) && *s!='.')
      s++;
    q = s;
    if (elfregs && *p=='%')
      p++;
    if (s-p == 2) {
      if ((*p=='D' || *p=='d' || *p=='A' || *p=='a') &&
          (*(p+1)>='0' && *(p+1)<='7'))
        reg = ((*p=='A' || *p=='a') ? REGAn : 0) | (*(p+1) - '0');
      else if ((*p=='S' || *p=='s') && (*(p+1)=='P' || *(p+1)=='p'))
        reg = REGAn + 7;
    }
  }
  if (reg<0 && p!=NULL && ((sym = find_regsym(p,q-p)) != NULL)) {
    /* register symbol found */
    if (sym->reg_type==RSTYPE_Dn || sym->reg_type==RSTYPE_An)
      reg = ((sym->reg_type==RSTYPE_An) ? REGAn : 0)
            | (signed char)sym->reg_num;
  }
  myfree(loc);

  if (reg >= 0) {
    if (*s == '.') {
      int extcode;

      if (!indexreg) {
        /* ColdFire MAC register extension? */
        extcode = getmacextcode(*(s+1));
      }
      else {
        /* index register extension */
        extcode = getextcode(*(s+1));
      }
      if (extcode) {
        reg |= extcode << 4;
        *start = s + 2;
      }
    }
    else
      *start = s;
  }
  return reg;
}


static uint16_t scan_Rnlist(char **start)
/* returns bit field for a Dn/An register list
   returns 0 otherwise */
{
  char *p = *start;

  if (getreg(&p,0) >= 0) {
    uint16_t list = 0;
    signed char lastreg=-1,reg,rx;
    int rangemode = 0;

    for (p=*start; *p; p++) {
      p = skip(p);
      if (rangemode && (*p>='0' && *p<='7')) {  /* allows d0-7 */
        reg = (lastreg & REGAn) | (*p - '0');
        p++;
      }
      else if ((reg = getreg(&p,0)) < 0) {
        cpu_error(2);  /* invalid register list */
        return 0;
      }

      if (rangemode) {
        /* lastreg...reg describes a range of registers */
        list &= ~(1<<lastreg);
        if (lastreg > reg) {
          rx = reg;
          reg = lastreg;
          lastreg = rx;
        }
        else if (lastreg == reg)
          cpu_error(17);  /* Rn-Rn */
        for (rx=lastreg; rx<=reg; rx++) {
          if (list & (1<<rx))
            cpu_error(17);  /* double register in list */
          else
            list |= 1<<rx;
        }
        rangemode = 0;
      }
      else {
        if (list & (1<<reg))
          cpu_error(17);  /* double register in list */
        else
          list |= 1<<reg;
      }

      lastreg = reg;
      p = skip(p);
      if (*p == '-')
        rangemode = 1;
      else if (*p != '/')
        break;
    }
    *start = p;
    return list;
  }
  return eval_rlsymbol(start);
}


static signed char getbreg(char **start)
{
  signed char reg = -1;

  if (cpu_type & apollo) {
    char *s = *start;
    char *p = NULL;
    char *loc,*q;
    regsym *sym;

    if (loc = get_local_label(&s)) {
      p = loc;
      q = loc + strlen(loc);
    }
    else if (ISIDSTART(*s) || (elfregs && *s=='%')) {
      p = s++;
      while (ISIDCHAR(*s) && *s!='.')
        s++;
      q = s;
      if (elfregs && *p=='%')
        p++;
      if (s-p==2 && (*p=='B' || *p=='b') && (*(p+1)>='0' && *(p+1)<='7'))
        reg = *(p+1) - '0';
    }
    if (reg<0 && p!=NULL && ((sym = find_regsym(p,q-p)) != NULL)) {
      /* register symbol found */
      if (sym->reg_type==RSTYPE_Bn)
        reg = (signed char)sym->reg_num;
    }
    myfree(loc);
    if (reg >= 0)
      *start = s;
  }
  return reg;
}


static signed char getfreg(char **start)
/* checks stream for an FPU register
   return -1 on failure, 0-7 for FP0-FP7 and
   10 for FPIAR, 11 for FPSR and 12 for FPCR */
{
  signed char reg = -1;

  if (cpu_type & (mfloat|mcffpu)) {
    char *s = *start;
    char *p = NULL;
    char *loc,*q;
    regsym *sym;

    if (loc = get_local_label(&s)) {
      p = loc;
      q = loc + strlen(loc);
    }
    else if (ISIDSTART(*s) || (elfregs && *s=='%')) {
      p = s++;
      while (ISIDCHAR(*s) && *s!='.')
        s++;
      q = s;
      if (elfregs && *p=='%')
        p++;
      if (s-p==3 && !strnicmp(p,"FP",2) && (*(p+2)>='0' && *(p+2)<='7'))
        reg = *(p+2) - '0';
      else if (s-p==5 && !strnicmp(p,"FPIAR",5))
        reg = 10;
      else if (s-p==4 && !strnicmp(p,"FPSR",4))
        reg = 11;
      else if (s-p==4 && !strnicmp(p,"FPCR",4))
        reg = 12;

    }
    if (reg<0 && p!=NULL && ((sym = find_regsym(p,q-p)) != NULL)) {
      /* register symbol found */
      if (sym->reg_type==RSTYPE_FPn)
        reg = (signed char)sym->reg_num;
    }
    myfree(loc);
    if (reg >= 0)
      *start = s;
  }
  return reg;
}


static uint16_t scan_FPnlist(char **start)
/* returns bit field for a FPn or FPIAR/FPSR/FPCR register list
   returns 0 otherwise */
{
  char *p = *start;

  if (getfreg(&p) >= 0) {
    signed char lastreg=-1,reg,rx;
    uint16_t list = 0;
    int fpnmode = -1;
    int rangemode = 0;

    for (p=*start; *p; p++) {
      p = skip(p);
      if (rangemode && fpnmode && (*p>='0' && *p<='7')) { /* allows fp0-7 */
        reg = *p++ - '0';
      }
      else if ((reg = getfreg(&p)) < 0) {
        cpu_error(2);  /* invalid register list */
        return 0;
      }

      if (fpnmode < 0) {
        fpnmode = (reg > 7) ? 0 : 1;
      }
      else {
        /* disallow mixing of fp0-fp7 and fpiar/fpsr/fpcr lists */
        if ((reg<=7 && !fpnmode) || (reg>7 && fpnmode)) {
          cpu_error(2);  /* invalid register list */
          return 0;
        }
      }

      if (fpnmode) {
        if (rangemode) {
          /* lastreg...reg describes a range of registers */
          if (lastreg > reg) {
            rx = reg;
            reg = lastreg;
            lastreg = rx;
          }
          for (rx=lastreg; rx<=reg; rx++)
            list |= 1<<(7-rx);
          rangemode = 0;
        }
        else
          list |= 1<<(7-reg);
      }
      else
        list |= 1<<reg;

      lastreg = reg;
      p = skip(p);
      if (*p == '-') {
        if (!fpnmode)
          cpu_error(2);  /* invalid register list */
        else
          rangemode = 1;
      }
      else if (*p != '/')
        break;
    }
    *start = p;
    return list;
  }
  return eval_rlsymbol(start);
}


static char *getspecreg(char *s,operand *op,int first,int last)
{
  char *name = NULL;

  if ((*s=='<' && *(s+1)=='<') || (*s=='>' && *(s+1)=='>')) {
    /* ColdFire MAC scale factor << or >>, treated as special reg. name */
    name = s;
    s += 2;
  }
  else {
    if (elfregs) {
      if (*s++ != '%')
        return NULL;
    }
    if (ISIDSTART(*s)) {
      name = s++;
      while (ISIDCHAR(*s))
        s++;
    }
  }

  if (name) {
    int i,len;

    for (i=first,len=s-name; i<=last; i++) {
      if (!strnicmp(name,SpecRegs[i].name,len)) {
        if (SpecRegs[i].name[len] != '\0')
          continue;
        if (!(SpecRegs[i].available & cpu_type))
          break;  /* not available for current CPU */
        op->mode = MODE_SpecReg;
        op->reg = i;  /* @@@ Warning: indexes 128-255 are stored negative! */
        op->value[0] = number_expr(SpecRegs[i].code);
        return s;
      }
    }
  }
  return NULL;
}


static int get_any_register(char **start,operand *op,int required)
/* checks for Dn, An, register lists and any other special register;
   fill op->mode, op->register, op->value[0] accordingly when successful
   and return with != 0 */
{
  char *s = *start;
  signed char reg;
  struct optype *ot = &optypes[required];

  if ((reg = getreg(start,0)) >= 0) {
    /* Dn or An */
    char *p = skip(*start);

    op->mode = REGisAn(reg) ? MODE_An : MODE_Dn;
    op->reg = REGget(reg);

    if (ot->flags & OTF_VXRNG4) {
      /* Apollo AMMX four-vector-registers range */
      if (!REGisAn(reg) && *p=='-') {
        *start = skip(p+1);
        if (!(reg&3) && getreg(start,0)==reg+3)
          return 1;  /* D0-D3 or D4-D7 */
      }
      *start = s;
      return 0;
    }
    else if (*p=='-' || *p=='/' || (ot->flags & OTF_REGLIST)) {
      /* it's a register list */
      op->mode = MODE_Extended;
      op->reg = REG_RnList;
      *start = s;
      op->value[0] = number_expr((taddr)scan_Rnlist(start));
    }
    else {
      unsigned char sf = reg >> 4;

      if (sf==EXT_UPPER || sf==EXT_LOWER || (ot->flags & FL_MAC)) {
        /* ColdFire MAC U/L register extension found, store in bf_offset */
        op->flags |= FL_MAC;
        op->bf_offset = sf == EXT_UPPER;
      }
    }
    return 1;
  }

  else if ((reg = getfreg(start)) >= 0) {
    /* FPn */
    char *p = skip(*start);

    if (*p=='-' || *p=='/' || (ot->flags & OTF_REGLIST)) {
      /* it's a register list */
      uint16_t lst;

      op->mode = MODE_Extended;
      op->reg = REG_FPnList;
      *start = s;
      lst = scan_FPnlist(start);
      if (reg >= 10)
        op->flags |= FL_FPSpec;  /* fpiar/fpcr/fpsr list */
      op->value[0] = number_expr((taddr)lst);
    }
    else {
      op->mode = MODE_FPn;
      if (reg >= 10) {
        /* fpiar,fpsr,fpcr */
        switch (reg) {
          case 10: op->reg = 1; break;
          case 11: op->reg = 2; break;
          case 12: op->reg = 4; break;
          default: ierror(0); break;
        }
        op->flags |= FL_FPSpec;
      }
      else
        op->reg = reg;
    }
    return 1;
  }

  else if ((reg = getbreg(start)) >= 0) {
    /* Bn (Apollo only) */
    op->mode = MODE_An;
    op->reg = REGget(reg);
    op->flags |= FL_BnReg;
    return 1;
  }

  else /*if (ot->flags & OTF_SPECREG)*/ {
    /* check, if it's one of our special register symbols (CCR,SR, etc.) */
    int first,last;

    if (ot->flags & OTF_SRRANGE) {
      first = ot->first;
      last = ot->last;
    }
    else {
      first = 0;
      last = specreg_cnt - 1;
    }

    if (s = getspecreg(s,op,first,last)) {
      if (ot->flags & OTF_VXRNG4) {
        /* need a four-vector-register range, E0-E3, E20-E23, etc. */
        int vxreg = (unsigned char)op->reg;
        operand dummy;

        s = skip(s);
        if (*s=='-' && !(SpecRegs[vxreg].code&3)) {
          s = skip(s+1);
          if ((s = getspecreg(s,&dummy,vxreg+3,vxreg+3)) == NULL)
            return 0;
        }
        else
          return 0;
      }
      *start = s;
      return 1;
    }
  }

  return 0;
}


static short getbasereg(char **start)
/* returns any register which could be a base or index register,
   including an optional extension and a scaling factor,
   like d0-d7,a0-a7,b0-b7,pc,zd0,zd7,za0-za7,zpc
   Bits 0-4:
    0 -  7    = d0-d7
    8 - 15    = a0-a7
   this means Bit 3 identifies an address register
    16        = pc
   Bit  5     = treat a0-a7 as b0-b7 (Apollo Core only)
   Bit  7     = Zero-flag (for suppressed registers: zdn,zan,zpc)
   Bits 8-10  = [REGext_Shift] optional extension (1=.b, 2=.w ... 7=.p)
   Bits 12-13 = [REGscale_Shift] scale factor (0=*1, 1=*2, 2=*4, 3=*8)
   returns -1 when no valid register was found */
{
  char *s = *start;
  char *p = NULL;
  char *loc,*q;
  short r = 0;
  regsym *sym;

  if (loc = get_local_label(&s)) {
    p = loc;
    q = loc + strlen(loc);
    r = -1;
  }
  else if (ISIDSTART(*s) || (elfregs && *s=='%')) {
    p = s++;
    while (ISIDCHAR(*s) && *s!='.')
      s++;
    if (elfregs && *p=='%')
      p++;
    if ((s-p)==3 && (*p=='z' || *p=='Z')) {
      r |= REGZero;
      p++;
    }
    if ((s-p) == 2) {
      if ((*p=='D' || *p=='d' || *p=='A' || *p=='a') &&
          (*(p+1)>='0' && *(p+1)<='7'))
        r |= ((*p=='A' || *p=='a') ? REGAn : 0) | (short)(*(p+1) - '0');
      else if ((*p=='S' || *p=='s') && (*(p+1)=='P' || *(p+1)=='p'))
        r |= REGAn+7;
      else if ((*p=='P' || *p=='p') && (*(p+1)=='C' || *(p+1)=='c'))
        r |= REGPC;
      else if ((cpu_type & apollo) && (*p=='B' || *p=='b') &&
               (*(p+1)>='0' && *(p+1)<='7'))
        r |= (short)(*(p+1) - '0') | REGBn | REGAn;
      else
        r = -1;
    }
    else
      r = -1;
    p = *start;
    q = s;
  }
  else
    r = -1;
  if (r<0 && p!=NULL && ((sym = find_regsym(p,q-p)) != NULL)) {
    /* register symbol found */
    if (sym->reg_type==RSTYPE_Dn || sym->reg_type==RSTYPE_An ||
        ((cpu_type&apollo) && sym->reg_type==RSTYPE_Bn)) {
      r = ((sym->reg_type==RSTYPE_Dn) ? 0 : REGAn)
          | (signed char)sym->reg_num;
      if (sym->reg_type == RSTYPE_Bn)
        r |= REGBn;
    }
  }
  myfree(loc);

  if (r >= 0) {
    if (*s == '.') {  /* read size extension */
      int extcode = getextcode(*(s+1));

      if (extcode && !ISIDCHAR(*(s+2))) {
        r |= extcode << REGext_Shift;
        s += 2;
      }
      else
        return -1;
    }

    if (*s == '*') {  /* read scale factor */
      short fac;

      s++;
      fac = (short)parse_constexpr(&s);
      switch (fac) {
        case 1:
          break;
        case 2:
          r |= 1 << REGscale_Shift;
          break;
        case 4:
          r |= 2 << REGscale_Shift;
          break;
        case 8:
          r |= 3 << REGscale_Shift;
          break;
        default:
          cpu_error(10);  /* illegal scale factor */
          break;
      }
    }
    *start = s;
  }
  return r;
}


static void set_index(operand *op,short i)
/* fill index register, including size and scale, into format word */
{
  if (i >= 0) {
    unsigned s = REGscale(i);

    op->flags |= FL_UsesFormat;

    if (REGisZero(i)) {
      op->flags |= FL_020up;
      op->format |= FW_FullFormat | FW_IndexSuppress;
      /* clear Postindexed, even when zero-reg. was specified as post-index */
      op->format &= ~FW_Postindexed;
    }
    else if (s) {
      /* ColdFire allows scale factors *2 and *4 (*8 when FPU is present),
         otherwise 68020+ is required. */
      if (!(cpu_type & mcf) || (s > 2 && !(cpu_type & mcffpu)))
        op->flags |= FL_020up;
    }

    op->format |= (REGisAn(i) ? FW_IndexAn : 0) |
                  FW_IndexReg(i) |
                  ((REGext(i)==EXT_LONG) ? FW_LongIndex : 0) |
                  FW_Scale(s);
  }
}


static taddr getbfk(char **p,int *dflag)
/* get a bit field specifier {Dm:Dn}/{m:n} or a k-factor {#n}/{Dn},
   dflag is set for a data register. */
{
  signed char reg = getreg(p,0);

  if (reg >= 0) {
    *dflag = 1;
    if (REGisDn(reg))
      return 0x80 + REGget(reg);
    else
      cpu_error(18);  /* data register required */
  }
  else {
    *dflag = 0;
    if (**p == '#')  /* skip for k-factor and gcc-syntax */
      *p += 1;
    return parse_constexpr(p);
  }
  return 0;
}


static void check_basereg(operand *op)
/* Check if the operand's address register matches one of the currently
   active BASEREG registers and automatically subtract its base-expression
   from the operand's displacement value. */
{
  if (op->reg>=0 && op->reg<=6 && baseexp[op->reg] && op->value[0]) {
    if (find_base(op->value[0],NULL,NULL,0) == BASE_OK) {
      expr *new = make_expr(SUB,op->value[0],copy_tree(baseexp[op->reg]));

      simplify_expr(new);
      op->value[0] = new;
      op->flags |= FL_BaseReg;  /* mark potential BASEREG expression */
    }
  }
}


static int fix_basereg(operand *op,int final)
/* Check whether the left side of the "<exp> - <base>" expression is still
   undefined. When it became a constant expression in the meantime, then
   do not treat it any longer as a BASEREG operand. */
{
  taddr val;
  expr *left;

  if (op->value[0]!=NULL && (left=op->value[0]->left)!=NULL) {
    if (eval_expr(left,&val,NULL,0)) {
      /* Kill the subtrahend of the base-relative expression. */
      if (final) {
        op->value[0]->left = NULL;
        free_expr(op->value[0]);
      }
      op->value[0] = left;
      op->flags &= ~FL_BaseReg;
      return 1;  /* fixed */
    }
  }
  return 0;  /* nothing changed */
}


static char *parse_immediate(char *start,operand *op,int is_float,int is_quad)
/* Parse an immediate operand of unknown size, which can be byte, word,
   long, quadword, single-, double-, extended-precision float or packed.
   is_float: Decimal constants are parsed as floating point. Hex, octal or
   binary contants directly into IEEE format.
   is_quad: 64-bit expressions are converted into thuge type, not taddr,
   so they don't allow labels and relocations. */
{
  if (is_float)
    op->value[0] = parse_expr_float(&start);
  else if (is_quad)
    op->value[0] = parse_expr_huge(&start);
  else
    op->value[0] = parse_expr(&start);
  return start;
}


static int base_disp_and_ext(operand *op,char **p)
/* Parse expression to value[0] and return the displacement-extension when
   given (or 0 when missing). Returns -1 when no valid expression was found. */
{
  if ((op->value[0] = parse_expr(p)) != NULL) {
    int disp_size;

    if ((disp_size = read_extension(p,0)) != 0)
      op->flags |= FL_NoOptBase;  /* do not optimize, when size is given */
    return disp_size;
  }
  return -1;
}


int parse_operand(char *p,int len,operand *op,int required)
{
  uint16_t reqmode = optypes[required].modes;
  uint32_t reqflags = optypes[required].flags;
  char *start = p;
  int i;

  op->mode = op->reg = -1;
  op->flags = 0;
  op->format = 0;
  op->value[0] = op->value[1] = NULL;
  p = skip(p);
  if (convert_brackets && !(cpu_type & (m68020up|cpu32|mcf))) {
    char c,*p2=p;

    while ((c = *p2) != '\0') {
      if (c == '[')
        *p2 = '(';
      else if (c== ']')
        *p2 = ')';
      p2++;
    }
  }

  if (reqflags & OTF_DATA) {
    /* a data definition */
    op->mode = MODE_Extended;
    op->reg = REG_Immediate;
    p = parse_immediate(p,op,(reqflags&OTF_FLTIMM)!=0,
                        (reqflags&OTF_QUADIMM)!=0);
  }
  else if (*p=='#' || (sgs && *p=='&')) {
    /* immediate addressing mode */
    p++;
    op->mode = MODE_Extended;
    op->reg = REG_Immediate;
    p = parse_immediate(p,op,(reqflags&OTF_FLTIMM)!=0 && is_float_ext(),
                        (reqflags&OTF_QUADIMM)!=0);
  }
  else {
    if (get_any_register(&p,op,required)) {
      char *ptmp = skip(p);

      if (*ptmp == ':') {
        /* possible register-pair definition */
        signed char reg;

        ptmp = skip(ptmp+1);

        if ((cpu_type&apollo) &&
            (op->mode==MODE_Dn || op->mode==MODE_An || op->mode==MODE_SpecReg)
            && !(op->flags&FL_BnReg)) {
          if (reqflags & OTF_VXRNG2) {
            if (op->mode==MODE_Dn && !(op->reg&1)) {
              /* Apollo: Dn:Dn+1 (AMMX) */
              reg = getreg(&ptmp,0);
              if (reg == op->reg+1)
                p = ptmp;
            }
            else if (op->mode == MODE_SpecReg) {
              /* Apollo: En:En+1 (AMMX) */
              int vxreg = (unsigned char)op->reg;
              operand dummy;

              if (!(SpecRegs[vxreg].code&1) &&
                  (ptmp = getspecreg(ptmp,&dummy,vxreg+1,vxreg+1)))
                p = ptmp;
            }
          }
          else if (op->mode != MODE_SpecReg) {
            /* Apollo: Rm:Rn */
            reg = getreg(&ptmp,0);
            if (reg >= 0) {
              if (op->mode == MODE_An) {
                op->mode = MODE_Dn; /* make it appear as Dn/DoubleReg mode */
                op->reg |= REGAn;   /* restore An-bit for Rm */
              }
              op->reg |= reg << 4;  /* insert Rn with 4 bits too */
              op->flags |= FL_DoubleReg;
              p = ptmp;
            }
            else
              cpu_error(44);  /* register expected */
          }
        }
        else if (op->mode == MODE_Dn) {
          /* Dm:Dn expected */
          reg = getreg(&ptmp,0);
          if (reg>=0 && REGisDn(reg)) {
            op->reg |= reg << 4;
            op->flags |= FL_DoubleReg | FL_020up;
            p = ptmp;
          }
          else
            cpu_error(18);  /* data register required */
        }
        else if (op->mode == MODE_FPn) {
          /* FPm:FPn expected */
          reg = getfreg(&ptmp);
          if (reg>=0 && reg<=7) {
            op->reg |= reg << 4;
            op->flags |= FL_DoubleReg;
            p = ptmp;
          }
          else
            cpu_error(42);  /* FP register required */
        }
      }
      p = skip(p);
    }

    else {
      /* no direct register */
      int disp_size = 0;
      int od_size;
      char *start_term = NULL;

      if (*p=='-' && *(p+1)=='(') {
        char *ptmp = skip(p+2);
        signed char reg;

        reg = getreg(&ptmp,0);
        if (reg<0 && (cpu_type&apollo)) {
          if ((reg = getbreg(&ptmp)) >= 0)
            op->flags |= FL_BnReg;  /* Apollo Core: use Bn instead of An */
        }

        if (reg >= 0) {
          /* addressing mode An indirect with predecrement */
          if (!REGisAn(reg))
            cpu_error(4);  /* address register required */
          ptmp = skip(ptmp);
          if (*ptmp != ')')
            cpu_error(3);  /* missing ) */
          else
            ptmp++;
          p = ptmp;
          op->mode = MODE_AnPreDec;
          op->reg = REGget(reg);
        }
      }

      parse_expression:
      if ((*p!='(' || start_term!=NULL) && op->mode<0) {
        /* this can only be an expression - parse it */
        disp_size = base_disp_and_ext(op,&p);
        p = skip(p);
      }

      if (*p=='(' && op->mode<0) {  /* "(..." */
        int mem_indir;
        short reg,idx=-1;

        start_term = p;
        p = skip(p+1);

        parse_indir:
        if (*p == '[') {
          /* 020+ memory indirect addressing mode */
          p = skip(p+1);
          if (op->value[0]) {
            /* An already parsed displacement expression before '[' */
            /* becomes an outer displacement for compatibility. */
            if (!devpac_compat)
              cpu_error(6);  /* warn about displacement at bad position */
            op->value[1] = op->value[0];
            od_size = disp_size ? disp_size : EXT_WORD;
            op->value[0] = NULL;
            disp_size = 0;
            if (op->flags & FL_NoOptBase) {
              op->flags &= ~FL_NoOptBase;
              op->flags |= FL_NoOptOuter;
            }
          }
          mem_indir = 1;
        }
        else
          mem_indir = 0;

        reg = getbasereg(&p);
        if (reg<0 && op->value[0]==NULL) {
          /* no register identified and still no value read: try it again */
          disp_size = base_disp_and_ext(op,&p);
          p = skip(p);
          if (*p == ')') {
            /* expression was only the first term: read the full expression */
            p = start_term;
            goto parse_expression;
          }
          if (*p == ',') {    /* "(displacement," expects register */
            p = skip(p+1);
            if ((reg = getbasereg(&p)) < 0) {
              if (*p == '[')
                goto parse_indir;
              cpu_error(7);  /* base or index register expected */
              return PO_CORRUPT;
            }
          }
        }

        /* check for illegal displacement extension */
        if (op->value[0] && disp_size>EXT_LONG) {
          cpu_error(5);  /* bad extension */
          disp_size = 0;
        }

        p = skip(p);
        if (mem_indir && reg<0) {
          /* "([val" without register means base reg is suppressed */
          reg = REGZero | REGAn | 0;
        }
        else if (reg>0 && REGisZero(reg))
          op->flags |= FL_ZBase;  /* ZAn was explicitely specified */

        if (reg >= 0) {  /* "(Rn" or "(d,Rn" or "([Rn" or "([bd,Rn" */
          int clbrk = 0;

          if ((REGisAn(reg) || REGisPC(reg)) &&
              !REGscale(reg) && !REGext(reg)) {
            /* Rn is a base register An or PC, try to read index now */

            if (*p == ']' && mem_indir) {  /* "([bd,Rn]" */
              /* if we ever see an index, it will be postindexed */
              op->format |= FW_Postindexed;
              p = skip(p+1);
              clbrk = 1;
            }

            parse_index:
            if (*p == ',') {  /* read index register */
              char *pidx = p;

              p = skip(p+1);
              if ((idx = getbasereg(&p)) >= 0) {
                /* "(d,Rn,Xn" or "([bd,Rn],Xn" or "([bd,Rn,Xn" */
                p = skip(p);
                if (*p == ']' && mem_indir) {  /* "([bd,Rn,Xn]" */
                  p = skip(p+1);
                  if (clbrk)
                    cpu_error(13);  /* too many ] */
                  else
                    clbrk = 1;
                }
              }
              else {
                if (!mem_indir || !clbrk) {
                  if (op->value[0] == NULL) {
                    /* (An,bd) is treated as (bd,An) for compatibility */
                    if ((disp_size = base_disp_and_ext(op,&p)) < 0) {
                      cpu_error(12);  /* index register expected */
                      return PO_CORRUPT;
                    }
                    else {
                      if (!devpac_compat)
                        cpu_error(6);  /* displacement at bad position */
                      p = skip(p);
                      goto parse_index;
                    }
                  }
                  else {
                    cpu_error(12);  /* index register expected */
                    return PO_CORRUPT;
                  }
                }
                else
                  p = pidx;  /* back to ',' to parse outer displacement */
              }
            }
            if (idx < 0)
              op->format &= ~FW_Postindexed;  /* index was suppressed */
          }
          else {
            /* Rn is already the index, assume ZA0 as base */
            if (!(reqflags & FL_DoubleReg)) {
              idx = reg;
              reg = REGZero | REGAn | 0;
              op->flags &= ~FL_ZBase;
            }
          }

          if (mem_indir) {
            if (!clbrk) {
              if (*p != ']')
                cpu_error(8);  /* missing ] */
              else
                p = skip(p+1);
            }
            if (*p == ',') {  /* "([bd,Rn],Xn,od" or "([bd,Rn,Xn],od" */
              /* read outer displacement */
              p = skip(p+1);
              if (op->value[1] == NULL) {
                if ((op->value[1] = parse_expr(&p)) != NULL) {
                  if ((od_size = read_extension(&p,0)) != 0)
                    op->flags |= FL_NoOptOuter;  /* do not optimize with size */
                  else
                    od_size = EXT_WORD;
                  p = skip(p);
                }
                else
                  cpu_error(14);  /* missing outer displacement */
              }
            }
            if (op->value[1] != NULL) {
              /* set outer displacement */
              if (od_size == EXT_WORD)
                op->format |= FW_IndSize(FW_Word);
              else if (od_size == EXT_LONG)
                op->format |= FW_IndSize(FW_Long);
              else
                cpu_error(5);  /* bad extension */
            }
            else
              op->format |= FW_IndSize(FW_Null);  /* no outer disp. given */
          }
        }

        if (*p==',' && op->value[0]==NULL) {
          /* (Rn,bd) is treated as (bd,Rn) for compatibility reasons */
          p = skip(p+1);
          disp_size = base_disp_and_ext(op,&p);
          if (disp_size >= 0) {
            p = skip(p);
            if (!devpac_compat)
              cpu_error(6);  /* warn about displacement at bad position */
          }
        }
        if (*p++ != ')')
          cpu_error(15,')');  /* ) expected */

        /* parsing completed, determine addressing mode */

        if (reg >= 0) {
          if (idx >= 0) {
            if (REGisPC(idx)) {
              cpu_error(16);  /* can't use PC register as index */
              return PO_CORRUPT;
            }
            if (REGisBn(idx)) {
              cpu_error(61,(int)REGget(idx));  /* can't use Bn as index */
              return PO_CORRUPT;
            }
            if (cpu_type & mcf) {
              if (REGext(idx)!=0 && REGext(idx)!=EXT_LONG)
                cpu_error(5);  /* bad extension - only .l for ColdFire */
              idx &= ~(EXT_MASK<<REGext_Shift);
              idx |= EXT_LONG<<REGext_Shift;
            }
            else if (REGext(idx)!=0 &&
                     REGext(idx)!=EXT_LONG && REGext(idx)!=EXT_WORD) {
              cpu_error(5);  /* bad extension */
              idx &= ~(EXT_MASK<<REGext_Shift);
            }
            if (REGisZero(idx))
              op->flags |= FL_ZIndex;  /* ZRn was explicitely specified */
          }

          /* set default displacement sizes */
          if (!disp_size)
            disp_size = idx<0 ? EXT_WORD : EXT_BYTE;

          if (!mem_indir && !REGisZero(reg) && op->value[1]==NULL &&
              ((idx<0 && disp_size==EXT_WORD) ||
               (idx>=0 && disp_size==EXT_BYTE && !REGisZero(idx)))) {
            /* normal 68000 addressing modes, including 020+ scaling */
            if (idx < 0) {
              if (op->value[0]) {
                if (REGisPC(reg)) {
                  op->mode = MODE_Extended;           /* (d16,PC) */
                  op->reg = REG_PC16Disp;
                }
                else {
                  op->mode = MODE_An16Disp;           /* (d16,An) */
                  op->reg = REGget(reg);
                  check_basereg(op);
                }
              }
              else if (REGisPC(reg)) {
                op->mode = MODE_Extended;             /* (PC) -> (0,PC) */
                op->reg = REG_PC16Disp;
                op->value[0] = number_expr(0);
              }
              else {
                if (*p == '+') {
                  op->mode = MODE_AnPostInc;          /* (An)+ */
                  op->reg = REGget(reg);
                  p++;
                }
                else {
                  char *ptmp = skip(p);

                  op->mode = MODE_AnIndir;            /* (An) */
                  op->reg = REGget(reg);
                  if (*ptmp == ':') {
                    /* (Rm):(Rn) expected, store reg as 4 bit */
                    op->reg = REGgetA(reg);
                    ptmp = skip(ptmp+1);
                    if (*ptmp == '(') {
                      ptmp = skip(ptmp+1);
                      reg = getreg(&ptmp,0);
                      if (reg >= 0) {
                        ptmp = skip(ptmp);
                        if (*ptmp++ == ')') {         /* (Rn):(Rm) */
                          op->reg |= REGgetA(reg) << 4;
                          op->flags |= FL_DoubleReg | FL_020up | FL_noCPU32;
                          p = ptmp;
                        }
                      }
                    }
                    if (p != ptmp) {
                      cpu_error(1);  /* illegal addressing mode */
                      return PO_CORRUPT;
                    }
                  }
                }
              }
            }
            else {
              if (!op->value[0]) {
                /* need a displacement for indexed addressing modes */
                op->value[0] = number_expr(0);
              }
              if (REGisPC(reg)) {                     /* (d8,PC,Xn) */
                op->mode = MODE_Extended;
                op->reg = REG_PC8Format;
              }
              else {
                op->mode = MODE_An8Format;            /* (d8,An,Xn) */
                op->reg = REGget(reg);
                check_basereg(op);
              }
              set_index(op,idx);
            }
          }
          else {
            /* the remaining addressing modes require a full format word */
            op->format |= FW_FullFormat;
            op->flags |= FL_UsesFormat | FL_020up;
            /* no memory-indirect modes for CPU32 */
            if (mem_indir)
              op->flags |= FL_noCPU32;

            if (REGisPC(reg)) {
              op->mode = MODE_Extended;               /* ([bd,PC,Xn],od) */
              op->reg = REG_PC8Format;
            }
            else {
              op->mode = MODE_An8Format;              /* ([bd,An,Xn],od) */
              op->reg = REGget(reg);
              check_basereg(op);
            }
            if (REGisZero(reg))
              op->format |= FW_BaseSuppress;

            if (idx < 0)
              idx = REGZero | 0;  /* no index given: assume ZD0.w */
            set_index(op,idx);

            if (op->value[0]) {
              if (disp_size == EXT_LONG)
                op->format |= FW_BDSize(FW_Long);
              else
                op->format |= FW_BDSize(FW_Word);
            }
            else
              op->format |= FW_BDSize(FW_Null);  /* base disp. suppressed */
          }

          if (REGisBn(reg))
            op->flags |= FL_BnReg;  /* Apollo Core: Bn instead of An */
        }
      }

      if (op->mode<0 && op->value[0]!=NULL) {
        /* we have read a value but no register at all,
           then it's an absolute addressing mode */
        op->mode = MODE_Extended;
        if (disp_size == EXT_WORD) {
          op->reg = REG_AbsShort;
        }
        else {
          if (reqflags & OTF_REGLIST) {
            op->reg = (required==RL) ? REG_RnList : REG_FPnList;
            /* op->flags |= FL_PossRegList;  @@@ not needed? */
          }
          else
            op->reg = REG_AbsLong;
          if (disp_size!=0 && disp_size!=EXT_LONG)
            cpu_error(5);  /* bad extension */
        }
      }

      p = skip(p);
      if (*p=='&' || (reqflags & FL_MAC)) {
        /* ColdFire MAC MASK specifier */
        op->flags |= FL_MAC;
        if (*p == '&') {
          op->bf_width = 1;
          p = skip(p+1);
        }
        else
          op->bf_width = 0;
      }
    }

    if (*p == '{') {
      /* bit field specifier or k-factor */
      int dflag,absk = 0;
      taddr bfval;

      p = skip(p+1);
      if (*p == '#')
        absk = 1;   /* probably absolute k-factor */
      bfval = getbfk(&p,&dflag);
      op->flags |= dflag ? FL_BFoffsetDyn : 0;
      p = skip(p);
      if (*p==':') {
        absk = 0;
        p = skip(p+1);
        op->bf_offset = (unsigned char)bfval;
        op->bf_width = (unsigned char)getbfk(&p,&dflag);
        op->flags |= dflag ? FL_BFwidthDyn : 0;
        p = skip(p);
        if (*p == '}')
          p++;
        else
          cpu_error(15,'}');  /* } expected */
        op->flags |= FL_Bitfield | FL_020up | FL_noCPU32;
      }
      else if (*p == '}') {
        if (absk) {
          op->bf_offset = (unsigned char)bfval;
          if (typechk && (bfval<-64 || bfval>63))
            cpu_error(21);  /* value from -64 to 63 required */
        }
        else  /* k-factor is a data register */
          op->bf_offset = (unsigned char)(bfval & 7) << 4;
        op->flags |= FL_KFactor;
        p++;
      }
      else {
        cpu_error(1);  /* illegal addressing mode */
        return PO_CORRUPT;
      }
    }
  }

  /* compare parsed addressing mode against requirements */

  for (i=0; i<16; i++) {
    if (reqmode & (1<<i)) {
      /*printf("%x:%x %d:%d %d:%d\n",op->flags&FL_CheckMask,reqflags&FL_CheckMask,op->mode,addrmodes[i].mode,op->reg,addrmodes[i].reg);*/
      if ((op->flags&FL_CheckMask)==(reqflags&FL_CheckMask) &&
          addrmodes[i].mode==op->mode &&
          (addrmodes[i].reg<0 || addrmodes[i].reg==op->reg)) {
        if (reqflags & OTF_CHKREG) {
          if ((unsigned char)op->reg < optypes[required].first ||
              (unsigned char)op->reg > optypes[required].last)
            return PO_NOMATCH;
        }
#if 0 /* @@@ not used */
        if (reqflags & OTF_CHKVAL) {
          if (op->value[0] == NULL)
            ierror(0);
          simplify_expr(op->value[0]);
          if (op->value[0]->type == NUM) {
            if (op->value[0]->c.val < (taddr)optypes[required].first ||
                op->value[0]->c.val > (taddr)optypes[required].last)
              return PO_NOMATCH;
          }
          else
            ierror(0);
        }
#endif
        if (required == DP) {
          /* never optimize d(An) operand for MOVEP */
          op->flags |= FL_NoOpt;
          if (op->mode == MODE_AnIndir) {
            /* translate (An) into 0(An) for MOVEP */
            op->mode = MODE_An16Disp;
            op->value[0] = number_expr(0);
            cpu_error(48,(int)op->reg,(int)op->reg);  /* warn about it */
          }
        }

        p = skip(p);
        if (*p!='\0' && p<(start+len))
          cpu_error(67);  /* trailing garbage in operand */
        return PO_MATCH;
      }
    }
  }

  return PO_NOMATCH;
}


static void eval_oper(operand *op,section *sec,taddr pc,int final)
/* evaluate operand expression */
{
  int i;

  for (i=0; i<2; i++) {
    op->base[i] = NULL;
    if (type_of_expr(op->value[i]) == NUM) {
eval:
      if (!eval_expr(op->value[i],&op->extval[i],sec,pc)) {
        op->basetype[i] = find_base(op->value[i],&op->base[i],sec,pc);

        if (op->basetype[i] == BASE_ILLEGAL) {
          if (op->flags & FL_BaseReg) {
            if (fix_basereg(op,final))
              goto eval;  /* evaluate fixed operand again */
          }
          if (final)
            general_error(38);  /* illegal relocation */
        }
      }
      op->flags |= FL_ExtVal0 << i;
    }
    else {
      op->extval[i] = 0x7fffffff;  /* dummy to prevent immediate opt. */
      op->flags &= ~(FL_ExtVal0 << i);
    }
  }
}


static int copy_float_exp(unsigned char *d,operand *op,int size)
/* write immediate floating point expression of 'size' to
   destination buffer, return 0 when everything was ok, else error-code */
{
  int et;
  thuge h;
  tfloat f;

  if (op->flags & FL_ExtVal0)
    et = NUM;
  else
    et = type_of_expr(op->value[0]);

  if (et == NUM) {
    if (op->base[0])
      return 20;  /* constant integer expression required */
  }
  else if (et == HUG) {
    if (!eval_expr_huge(op->value[0],&h))
      return 59;  /* cannot evaluate huge integer */
  }
  else if (et == FLT) {
    if (!eval_expr_float(op->value[0],&f))
      return 60;  /* cannot evaluate floating point */
  }
  else
    return 37;  /* immediate operand has illegal type */

  switch (size) {

    case EXT_SINGLE:
      if (et == NUM)
        setval(1,d,4,op->extval[0]);
      else if (et == HUG)
        huge_to_mem(1,d,4,h);
      else
        conv2ieee32(1,d,f);
      break;

    case EXT_DOUBLE:
      if (et == NUM)
        setval_signext(1,d,4,4,op->extval[0]);
      else if (et == HUG)
        huge_to_mem(1,d,8,h);
      else
        conv2ieee64(1,d,f);
      break;

    case EXT_EXTENDED:
      if (et == NUM)
        setval_signext(1,d,8,4,op->extval[0]);
      else if (et == HUG)
        huge_to_mem(1,d,12,h);
      else
        conv2ieee80(1,d,f);
      break;

    case EXT_PACKED:
      if (et == NUM)
        setval_signext(1,d,8,4,op->extval[0]);
      else if (et == HUG)
        huge_to_mem(1,d,12,h);
      else
        conv2packed(d,f);
      break;

    default:
      return 34;  /* illegal opcode extension */
  }

  return 0;
}


static void optimize_oper(operand *op,struct optype *ot,section *sec,
                          taddr pc,taddr cpc,int final)
/* evaluate expressions in operand and try to optimize addressing modes */
{
  int size16[2]; /* true, when extval[] fits into 16 bits */
  taddr pcdisp;  /* calculated pc displacement = (label - current_pc) */
  int pcdisp16;  /* true, when pcdisp fits into 16 bits */
  int absdpc;    /* true, when PC-displ. is const. in a non-absolute sect. */
  int undef;     /* true, when first base-symbol is still undefined */
  int secrel;    /* pc-relative reference in current section */
  int bdopt;     /* true, when base-displacement optimization allowed */
  int odopt;     /* true, when outer-displacement optimization allowed */

  if (!(op->flags & FL_DoNotEval))
    eval_oper(op,sec,pc,final);

  if ((op->flags & FL_NoOpt) == FL_NoOpt)
    return;

  /* optimize and fix addressing modes */

  bdopt = !(op->flags & FL_NoOptBase);
  odopt = !(op->flags & FL_NoOptOuter);
  size16[0] = op->extval[0]>=-0x8000 && op->extval[0]<=0x7fff;
  size16[1] = op->extval[1]>=-0x8000 && op->extval[1]<=0x7fff;
  pcdisp = op->extval[0] - cpc;
  pcdisp16 = pcdisp>=-0x8000 && pcdisp<=0x7fff;
  absdpc = !(sec->flags & ABSOLUTE) && op->base[0]==NULL;
  undef = (op->base[0]==NULL) ? 0 : EXTREF(op->base[0]);
  secrel = (sec->flags & ABSOLUTE) ? op->base[0]==NULL :
           op->base[0]!=NULL && LOCREF(op->base[0]) && op->base[0]->sec==sec;

  if (bdopt) {  /* base displacement optimizations allowed */

    if (op->mode==MODE_An16Disp) {
      if (opt_disp && !op->base[0] && op->extval[0]==0 &&
          (ot->modes & (1<<AM_AnIndir))) {
        /* (0,An) --> (An) */
        op->mode = MODE_AnIndir;
        if (final) {
          free_expr(op->value[0]);
          if (warn_opts>1)
            cpu_error(49,"(0,An)->(An)");
        }
        op->value[0] = NULL;
      }
      else if (((op->base[0] && !undef) || (!op->base[0] && !size16[0])) &&
               (cpu_type & (m68020up|cpu32)) && op->reg!=sdreg &&
               (ot->modes & (1<<AM_An8Format))) {
        /* (d16,An) --> (bd32,An,ZDn.w) for 020+ only */
        op->mode = MODE_An8Format;
        op->format = FW_FullFormat | FW_IndexSuppress | FW_BDSize(FW_Long);
        op->flags |= FL_UsesFormat | FL_020up;
        if (final && warn_opts>1)
          cpu_error(50,"(d16,An)->(bd32,An,ZDn.w)");
      }
    }

    else if (op->mode==MODE_Extended && op->reg==REG_PC16Disp &&
             (cpu_type & (m68020up|cpu32)) &&
             (ot->modes & (1<<AM_PC8Format))) {
      if ((absdpc && !size16[0]) || (secrel && !pcdisp16)) {
        /* (d16,PC) --> (bd32,PC,ZDn.w) for 020+ only */
        op->reg = REG_PC8Format;
        op->format = FW_FullFormat | FW_IndexSuppress | FW_BDSize(FW_Long);
        op->flags |= FL_UsesFormat | FL_020up;
        if (final && warn_opts>1)
          cpu_error(50,"(d16,PC)->(bd32,PC,ZDn.w)");
      }
    }

    else if (op->mode==MODE_An8Format && (op->flags & FL_UsesFormat) &&
             !(op->format & FW_FullFormat)) {
      if ((cpu_type & (m68020up|cpu32)) &&
          ((op->base[0] && !undef) ||
           (!op->base[0] && (op->extval[0]<-0x80 || op->extval[0]>0x7f)))) {
        /* (d8,An,Rn) --> (bd,An,Rn) */
        op->format &= 0xff00;
        op->format |= FW_FullFormat | FW_BDSize(FW_Word);
        op->flags |= FL_020up;
        if (final && warn_opts>1)
          cpu_error(50,"(d8,An,Rn)->(bd,An,Rn)");
      }
    }

    else if (op->mode==MODE_Extended && op->reg==REG_PC8Format &&
             (op->flags & FL_UsesFormat) && !(op->format & FW_FullFormat)) {
      int pcdisp8 = absdpc ?
                    (op->extval[0]>=-0x80 && op->extval[0]<=0x7f) :
                    ((pcdisp>=-0x80 && pcdisp<=0x7f) || undef);

      if ((cpu_type & (m68020up|cpu32)) && !pcdisp8) {
        /* (d8,PC,Rn) --> (bd,PC,Rn) */
        op->format &= 0xff00;
        op->format |= FW_FullFormat | FW_BDSize(FW_Word);
        op->flags |= FL_020up;
        if (final && warn_opts>1)
          cpu_error(50,"(d8,PC,Rn)->(bd,PC,Rn)");
      }
    }

    else if (op->mode==MODE_Extended && op->reg==REG_AbsShort &&
             (ot->modes & (1<<AM_AbsLong))) {
      if (!op->base[0] && !size16[0]) {
        /* absval.w --> absval.l */
        op->reg = REG_AbsLong;
        if (final && warn_opts>1)
          cpu_error(50,"abs.w->abs.l");
      }
      else if (op->base[0] && typechk && LOCREF(op->base[0])) {
        /* label.w --> label.l */
        op->reg = REG_AbsLong;
        if (final)
          cpu_error(22);  /* need 32 bits to reference a program label */
      }
    }

    else if (op->mode==MODE_Extended && op->reg==REG_AbsLong) {
      if (opt_abs && !op->base[0] && size16[0] &&
          (ot->modes & (1<<AM_AbsShort))) {
        /* absval.l --> absval.w */
        op->reg = REG_AbsShort;
        if (final && warn_opts>1)
          cpu_error(49,"abs.l->abs.w");
      }
      else if (sdreg>=0 && op->base[0]!=NULL &&
               (ot->modes & (1<<AM_An16Disp)) &&
               ((opt_gen && (op->base[0]->flags&NEAR)) ||
                (opt_sd && LOCREF(op->base[0]) &&
                 (op->base[0]->sec->flags&NEAR_ADDRESSING))) &&
               op->extval[0]>=0 && op->extval[0]<=0xffff) {
        /* label.l --> label(An) base relative near addressing */
        op->mode = MODE_An16Disp;
        op->reg = sdreg;
        if (final && warn_opts>1)
          cpu_error(49,"label->(label,An)");
      }
      else if (opt_pc && (ot->modes & (1<<AM_PC16Disp))) {
        if (secrel && pcdisp16) {
          /* label.l --> d16(PC) */
          op->reg = REG_PC16Disp;
          if (final && warn_opts>1)
            cpu_error(49,"label->(d16,PC)");
        }
      }
    }
  }

  if (op->mode==MODE_An8Format && (op->flags & FL_UsesFormat) &&
      (op->format & FW_FullFormat)) {

    if (FW_getIndSize(op->format)==FW_None) {  /* no memory indirection */
      if (FW_getBDSize(op->format) > FW_Null) {
        if (opt_bdisp && bdopt && !op->base[0] && op->extval[0]==0) {
          /* (0,An,...) --> (An,...) */
          op->format &= ~FW_BDSize(FW_SizeMask);
          op->format |= FW_BDSize(FW_Null);
          if (final && warn_opts>1)
            cpu_error(49,"(0,An,...)->(An,...)");
        }
        else if (bdopt && (op->base[0] || (!op->base[0] && !size16[0])) &&
                 FW_getBDSize(op->format)==FW_Word) {
          /* (bd16,An,...) --> (bd32,An,...) */
          op->format &= ~FW_BDSize(FW_SizeMask);
          op->format |= FW_BDSize(FW_Long);
          if (final && warn_opts>1)
            cpu_error(50,"(bd16,An,...)->(bd32,An,...)");
        }
        else if (opt_bdisp && bdopt && !op->base[0] && size16[0] &&
                 FW_getBDSize(op->format)==FW_Long) {
          if ((op->format & FW_IndexSuppress) &&
              (ot->modes & (1<<AM_An16Disp)) &&
              !(op->flags & FL_ZIndex)) {
            /* (bd32,An,ZRn) --> (d16,An) */
            op->mode = MODE_An16Disp;
            op->format = 0;
            op->flags &= ~(FL_UsesFormat | FL_020up | FL_noCPU32);
            if (final && warn_opts>1)
              cpu_error(49,"(bd32,An,ZRn)->(d16,An)");
          }
          else {
            /* (bd32,An,Rn) --> (bd16,An,Rn) */
            op->format &= ~FW_BDSize(FW_SizeMask);
            op->format |= FW_BDSize(FW_Word);
            if (final && warn_opts>1)
              cpu_error(49,"(bd32,An,Rn)->(d16,An,Rn)");
          }
        }
      }
      else if (opt_gen && !op->base[0] &&
               (op->format & FW_IndexSuppress) &&
               !(op->format & FW_BaseSuppress) &&
               (ot->modes & (1<<AM_AnIndir)) &&
               !(op->flags & FL_ZIndex)) {
        /* (An,ZRn) --> (An) */
        op->mode = MODE_AnIndir;
        op->format = 0;
        op->flags &= ~(FL_UsesFormat | FL_020up | FL_noCPU32);
        if (final && warn_opts>1)
          cpu_error(49,"(An,ZRn)->(An)");
      }
    }

    else {  /* memory indirection */
      if (opt_bdisp && bdopt && !op->base[0] &&
          FW_getBDSize(op->format)>FW_Null && op->extval[0]==0) {
        /* ([0,An,...],...) --> ([An,...],...) */
        op->format &= ~FW_BDSize(FW_SizeMask);
        op->format |= FW_BDSize(FW_Null);
        if (final && warn_opts>1)
           cpu_error(49,"([0,An,...],...)->([An,...],...)");
      }
      else if (bdopt && (op->base[0] || (!op->base[0] && !size16[0])) &&
               FW_getBDSize(op->format)==FW_Word) {
        /* ([bd16,An,...],...) --> ([bd32,An,...],...) */
        op->format &= ~FW_BDSize(FW_SizeMask);
        op->format |= FW_BDSize(FW_Long);
        if (final && warn_opts>1)
           cpu_error(50,"([bd16,An,...],...)->([bd32,An,...],...)");
      }
      else if (opt_bdisp && bdopt && !op->base[0] && size16[0] &&
               FW_getBDSize(op->format)==FW_Long) {
        /* ([bd32,An,...],...) --> ([bd16,An,...],...) */
        op->format &= ~FW_BDSize(FW_SizeMask);
        op->format |= FW_BDSize(FW_Word);
        if (final && warn_opts>1)
           cpu_error(49,"([bd32,An,...],...)->([bd16,An,...],...)");
      }
      if (FW_getIndSize(op->format) >= FW_Word) {  /* outer displacement */
        if (opt_odisp && odopt && !op->base[1] && op->extval[1]==0) {
          /* ([...],0) --> ([...]) */
          op->format &= ~FW_IndSize(FW_SizeMask);
          op->format |= FW_IndSize(FW_Null);
          if (final && warn_opts>1)
             cpu_error(49,"([...],0)->([...])");
        }
        else if (odopt && (op->base[1] || (!op->base[1] && !size16[1])) &&
                 FW_getIndSize(op->format)==FW_Word) {
          /* ([...],od16) --> ([...],od32) */
          op->format &= ~FW_IndSize(FW_SizeMask);
          op->format |= FW_IndSize(FW_Long);
          if (final && warn_opts>1)
             cpu_error(50,"([...],od16)->([...],od32)");
        }
        else if (opt_odisp && odopt && !op->base[1] && size16[1] &&
                 FW_getIndSize(op->format)==FW_Long) {
          /* ([...],od32) --> ([...],od16) */
          op->format &= ~FW_IndSize(FW_SizeMask);
          op->format |= FW_IndSize(FW_Word);
          if (final && warn_opts>1)
             cpu_error(49,"([...],od32)->([...],od16)");
        }
      }
    }
  }

  else if (op->mode==MODE_Extended && op->reg==REG_PC8Format &&
           (op->flags & FL_UsesFormat) && (op->format & FW_FullFormat)) {

    if (FW_getIndSize(op->format)==FW_None) {  /* no memory indirection */
      if (FW_getBDSize(op->format) > FW_Null) {
        if (opt_bdisp && bdopt && absdpc && op->extval[0]==0) {
          /* (0,PC,...) --> (PC,...) */
          op->format &= ~FW_BDSize(FW_SizeMask);
          op->format |= FW_BDSize(FW_Null);
          if (final && warn_opts>1)
             cpu_error(49,"(0,PC,...)->(PC,...)");
        }
        else if (bdopt && ((secrel && !pcdisp16) || (absdpc && !size16[0]))
                 && FW_getBDSize(op->format)==FW_Word) {
          /* (bd16,PC,...) --> (bd32,PC,...) */
          op->format &= ~FW_BDSize(FW_SizeMask);
          op->format |= FW_BDSize(FW_Long);
          if (final && warn_opts>1)
             cpu_error(50,"(bd16,PC,...)->(bd32,PC,...)");
        }
        else if (opt_bdisp && bdopt &&
                 ((secrel && pcdisp16) || (absdpc && size16[0]))
                 && FW_getBDSize(op->format)==FW_Long) {
          if ((op->format & FW_IndexSuppress) &&
              (ot->modes & (1<<AM_PC16Disp)) &&
              !(op->flags & FL_ZIndex)) {
            /* (bd32,PC,ZRn) --> (d16,PC) */
            op->reg = REG_PC16Disp;
            op->format = 0;
            op->flags &= ~(FL_UsesFormat | FL_020up | FL_noCPU32);
            if (final && warn_opts>1)
               cpu_error(49,"(bd32,PC,ZRn)->(d16,PC)");
          }
          else {
            /* (bd32,PC,Rn) --> (bd16,PC,Rn) */
            op->format &= ~FW_BDSize(FW_SizeMask);
            op->format |= FW_BDSize(FW_Word);
            if (final && warn_opts>1)
               cpu_error(49,"(bd32,PC,Rn)->(bd16,PC,Rn)");
          }
        }
      }
    }

    else {  /* memory indirection */
      if (opt_bdisp && bdopt && absdpc &&
          FW_getBDSize(op->format)>FW_Null && op->extval[0]==0) {
        /* ([0,PC,...],...) --> ([PC,...],...) */
        op->format &= ~FW_BDSize(FW_SizeMask);
        op->format |= FW_BDSize(FW_Null);
        if (final && warn_opts>1)
            cpu_error(49,"([0,PC,...],...)->([PC,...],...)");
      }
      else if (bdopt && ((secrel && !pcdisp16) || (absdpc && !size16[0])) &&
               FW_getBDSize(op->format)==FW_Word) {
        /* ([bd16,PC,...],...) --> ([bd32,PC,...],...) */
        op->format &= ~FW_BDSize(FW_SizeMask);
        op->format |= FW_BDSize(FW_Long);
        if (final && warn_opts>1)
            cpu_error(50,"([bd16,PC,...],...)->([bd32,PC,...],...)");
      }
      else if (opt_bdisp && bdopt &&
               ((secrel && pcdisp16) || (absdpc && size16[0])) &&
               FW_getBDSize(op->format)==FW_Long) {
        /* ([bd32,PC,...],...) --> ([bd16,PC,...],...) */
        op->format &= ~FW_BDSize(FW_SizeMask);
        op->format |= FW_BDSize(FW_Word);
        if (final && warn_opts>1)
            cpu_error(49,"([bd32,PC,...],...)->([bd16,PC,...],...)");
      }
      if (FW_getIndSize(op->format) >= FW_Word) {  /* outer displacement */
        if (opt_odisp && !op->base[1] && odopt && op->extval[1]==0) {
          /* ([...],0) --> ([...]) */
          op->format &= ~FW_IndSize(FW_SizeMask);
          op->format |= FW_IndSize(FW_Null);
          if (final && warn_opts>1)
              cpu_error(49,"([...],0)->([...])");
        }
        else if (odopt && (op->base[1] || (!op->base[1] && !size16[1])) &&
                 FW_getIndSize(op->format)==FW_Word) {
          /* ([...],od16) --> ([...],od32) */
          op->format &= ~FW_IndSize(FW_SizeMask);
          op->format |= FW_IndSize(FW_Long);
          if (final && warn_opts>1)
              cpu_error(50,"([...],od16)->([...],od32)");
        }
        else if (opt_odisp && odopt && !op->base[1] && size16[1] &&
                 FW_getIndSize(op->format)==FW_Long) {
          /* ([...],od32) --> ([...],od16) */
          op->format &= ~FW_IndSize(FW_SizeMask);
          op->format |= FW_IndSize(FW_Word);
          if (final && warn_opts>1)
              cpu_error(49,"([...],od32)->([...],od16)");
        }
      }
    }
  }
}


static int optypes_subset(mnemonic *mold,mnemonic *mnew)
/* returns TRUE, when optypes of mold are a subset of mnew */
{
  int i;

  for (i=0; i<MAX_OPERANDS; i++) {
    int ot_old = mold->operand_type[i];
    int ot_new = mnew->operand_type[i];
    uint32_t fl_old = optypes[ot_old].flags;
    uint32_t fl_new = optypes[ot_new].flags;
    uint16_t m = optypes[ot_old].modes;

    if ((!ot_old && ot_new) || (!ot_new && ot_old))
      return 0;  /* different number of operands */

    if ((optypes[ot_new].modes & m) != m ||
        (fl_old&FL_CheckMask) != (fl_new&FL_CheckMask))
      return 0;  /* addressing modes are not a subset of current mnemo */

    if ((fl_old & OTF_SPECREG) && (fl_new & OTF_SPECREG)) {
      if (optypes[ot_old].first < optypes[ot_new].first ||
          optypes[ot_old].last > optypes[ot_new].last)
        return 0;  /* special register range is not a subset */
    }
  }

  return 1;
}


static instruction *ip_dualop(int code,char *q,signed char mode1,
                              signed char reg1,uint16_t flags1,
                              uint16_t format1,expr *exp1,
                              signed char mode2, signed char reg2,
                              uint16_t flags2, uint16_t format2,
                              expr *exp2)
{
  instruction *ip;

  if (ipslot >= MAX_IP_COPIES)
    ierror(0);
  ip = clr_instruction(&newip[ipslot]);
  ip->code = code;
  ip->qualifiers[0] = q;
  ip->op[0] = clr_operand(&newop[ipslot][0]);
  ip->op[0]->mode = mode1;
  ip->op[0]->reg = reg1;
  ip->op[0]->flags = flags1;
  ip->op[0]->format = format1;
  ip->op[0]->value[0] = exp1;
  if (mode2 >= 0) {
    ip->op[1] = clr_operand(&newop[ipslot][1]);
    ip->op[1]->mode = mode2;
    ip->op[1]->reg = reg2;
    ip->op[1]->flags = flags2;
    ip->op[1]->format = format2;
    ip->op[1]->value[0] = exp2;
  }
  ++ipslot;
  return ip;
}


static instruction *ip_singleop(int code,char *q,signed char mode,
                                signed char reg,uint16_t flags,
                                uint16_t format,expr *exp)
{
  return ip_dualop(code,q,mode,reg,flags,format,exp,-1,0,0,0,NULL);
}


static int test_incr_ea(operand *op,taddr offset)
/* test if we could increment the EA in this operand */
{
  signed char m,r;
  taddr val;

  m = op->mode;
  if (m<MODE_AnIndir || m>MODE_Extended)
    return 0;
  val = op->extval[0] + offset;
  if (m == MODE_An8Format) {
    if (op->format & FW_FullFormat)
      return 0;  /* too complex, doesn't make sense */
    if (val<-0x80 || val>0x7f)
      return 0;
  }
  else if (m == MODE_An16Disp) {
    if (val<-0x8000 || val>0x7fff)
      return 0;
  }
  else if (m == MODE_Extended) {
    r = op->reg;
    if (r > REG_AbsLong)
      return 0;  /* no PC-modes for now */
  }
  return 1;
}


static void incr_ea(operand *op,taddr offset,int final)
/* increment the EA in 'op' by 'offset' when the addressing mode is suitable */
{
  if (final) {
    signed char m = op->mode;
    signed char r = op->reg;

    if (m == MODE_AnIndir) {
      /* change to (d16,An) and allocate the d16 expression */
      op->mode = MODE_An16Disp;
      op->value[0] = number_expr(offset);
    }
    else if (m==MODE_An16Disp || m==MODE_An8Format ||
             (m==MODE_Extended && (r==REG_AbsShort || r==REG_AbsLong))) {
      /* increment the expression by 'offset' */
      op->value[0] = make_expr(ADD,op->value[0],number_expr(offset));
    }
  }
  else {
    /* we will definitely need (d16,An) after incrementation for (An) */
    if (op->mode == MODE_AnIndir) {
      op->mode = MODE_An16Disp;
      op->flags |= FL_NoOpt;
    }
  }
}


static int aindir_in_list(operand *op,taddr list)
/* tests if operand is (An)+ or -(An) and An is present in register list */
{
  if (op->mode==MODE_AnPostInc || op->mode==MODE_AnPreDec)
    return (list & (1 << (REGAn + REGget(op->reg)))) != 0;
  return 0;
}


static unsigned char optimize_instruction(instruction *iplist,section *sec,
                                          taddr pc,int final)
{
  instruction *ip = iplist;
  mnemonic *mnemo = &mnemonics[ip->code];
  char ext = ip->qualifiers[0] ?
             tolower((unsigned char)ip->qualifiers[0][0]) : '\0';
  unsigned char ipflags = ip->ext.un.real.flags;
  signed char lastsize = ip->ext.un.real.last_size;
  char orig_ext = (char)ip->ext.un.real.orig_ext;
  uint16_t oc;
  taddr val=0,cpc;
  int abs=0,pcrelok=0,i;

  /* See if the next instruction fits as well, and includes the
     addressing modes of the current one. Following instructions
     usually have higher CPU requirements. */
  while (!strcmp(mnemo->name,mnemonics[ip->code+1].name) &&
         (mnemonics[ip->code+1].ext.available & cpu_type) != 0) {
    mnemonic *nextmn = &mnemonics[ip->code+1];
    uint16_t nextsize = nextmn->ext.size;

    /* first check if next instruction supports current size extension */
    if ((mnemo->ext.size&SIZE_MASK) != SIZE_UNSIZED ||
        (nextsize&SIZE_MASK) != SIZE_UNSIZED) {
      if ((nextsize&S_CFCHECK) && (cpu_type&mcf))
        nextsize &= ~(SIZE_BYTE|SIZE_WORD);  /* ColdFire */
      if ((nextsize & lc_ext_to_size(ext)) == 0)
        break;  /* size not supported */
    }
    if (!optypes_subset(mnemo,nextmn) ||
        S_OPCODE_SIZE(nextmn->ext.size) > S_OPCODE_SIZE(mnemo->ext.size))
      break;  /* not all operand types supported or instruction is bigger */
    ip->code++;
  }
  mnemo = &mnemonics[ip->code];

  cpc = pc + (S_OPCODE_SIZE(mnemo->ext.size) << 1);
  if (phxass_compat) {
    /* For PhxAss-compatibility, set value of the "current-pc-symbol"
       to pc + S_OPCODE_SIZE. */
    pc = cpc;
  }

  /* Make sure JMP/JSR (label,PC) is never optimized (to a short-branch) */
  if ((mnemo->ext.opcode[0]==0x4ec0 || mnemo->ext.opcode[0]==0x4e80) &&
      ip->op[0]!=NULL && ip->op[0]->mode==MODE_Extended &&
      ip->op[0]->reg==REG_PC16Disp)
    ip->op[0]->flags |= FL_NoOpt;  /* do not optimize */

  /* evaluate and optimize operands */
  for (i=0; i<MAX_OPERANDS && ip->op[i]!=NULL; i++)
    optimize_oper(ip->op[i],&optypes[mnemo->operand_type[i]],sec,pc,cpc,final);

  /* resolve register lists in MOVEM instructions */
  if (mnemo->ext.opcode[0]==0x4880 && mnemo->operand_type[0]!=IR &&
      mnemo->ext.place[0]==D16 && mnemo->ext.place[1]==SEA &&
      ip->op[1]->base[0]==NULL && ip->op[1]->value[0]!=NULL &&
      ip->op[1]->value[0]->type==SYM &&
      (ip->op[1]->value[0]->c.sym->flags & REGLIST)) {
    /* destination operand seems to be the register list - swap them */
    ip->op[0]->reg = REG_AbsLong;
    ip->op[1]->reg = REG_RnList;
    ip->code += 3;  /* take matching mnemonic with swapped operands */
    mnemo = &mnemonics[ip->code];
    if (final && ip->op[0]->base[0]==NULL && ip->op[0]->value[0]!=NULL &&
        ip->op[0]->value[0]->type==SYM &&
        (ip->op[0]->value[0]->c.sym->flags & REGLIST))
      cpu_error(62);  /* register list on both sides */
  }
  /* resolve register lists in FMOVEM instructions */
  else if (mnemo->operand_type[0] == FL) {
    if (ip->op[1]->base[0]==NULL && ip->op[1]->value[0]!=NULL &&
        ip->op[1]->value[0]->type==SYM &&
        (ip->op[1]->value[0]->c.sym->flags & REGLIST)) {
      if (ip->op[1]->extval[0] & 0x1c00)
        ip->code = OC_FMOVEMTOSPEC;  /* list contains special registers */
      else if (mnemo->operand_type[1] == AC)
        ip->code = OC_FMOVEMTOLIST;
      else if (mnemo->operand_type[1] == CFMM)
        ip->code = OC_FMOVEMTOLIST - 1;
      else
        goto dontswap;
      /* destination operand seems to be register list - swap them */
      ip->op[0]->reg = REG_AbsLong;
      ip->op[1]->reg = REG_FPnList;
      mnemo = &mnemonics[ip->code];
      if (final && ip->op[0]->base[0]==NULL && ip->op[0]->value[0]!=NULL &&
          ip->op[0]->value[0]->type==SYM &&
          (ip->op[0]->value[0]->c.sym->flags & REGLIST))
        cpu_error(62);  /* register list on both sides */
    }
    else if (ip->op[0]->base[0]==NULL && (ip->op[0]->extval[0] & 0x1c00)) {
      /* register list in first operand contains special registers */
      ip->code = OC_FMOVEMFROMSPEC;
      mnemo = &mnemonics[ip->code];
    }
  }
  else if (mnemo->operand_type[1]==FL && ip->op[1]->base[0]==NULL &&
           (ip->op[1]->extval[0] & 0x1c00)) {
    /* register list in first operand contains special registers */
    ip->code = OC_FMOVEMTOSPEC;
    mnemo = &mnemonics[ip->code];
  }
  else if (mnemo->ext.place[1]==F13 && ip->op[0]->mode==MODE_Extended &&
           ip->op[0]->reg==REG_Immediate) {
    /* FMOVEM.L #x,FPcrs - may be 64 or 96 bits for two or three registers */
    switch (cntones(ip->op[1]->extval[0],16)) {
      case 2:
        ip->qualifiers[0] = d_str;  /* two registers: 64 bit immediate */
        break;
      case 3:
        ip->qualifiers[0] = x_str;  /* three registers: 96 bit immediate */
        break;
    }
  }
dontswap:
  ip->ext.un.copy.next = NULL;

  /* assign _MOVEMBYTES and _MOVEMREGS when opcode is MOVEM */
  if ((mnemo->ext.opcode[0] & 0xfbff) == 0x4880) {
    expr *rlexp = (mnemo->ext.opcode[0] & 0x0400) ?
                  ip->op[1]->value[0] : ip->op[0]->value[0];
    taddr val;

    if (!eval_expr(rlexp,&val,NULL,0) && final)
      general_error(30);  /* expression must be constant */
    movemregs->expr = rlexp;
    if (movemsize->expr->type != NUM)
      ierror(0);
    movemsize->expr->c.val = ext=='w' ? 2 : 4;
  }

  if (no_opt)
    return ipflags;   /* no optimizations wanted */

  /* STAGE 1
     all instructions optimized here may be optimized again in the 2nd stage */
  oc = mnemo->ext.opcode[0];
  if (ip->op[0]) {
    val = ip->op[0]->extval[0];
    abs = ip->op[0]->base[0]==NULL;
    pcrelok = (sec->flags&ABSOLUTE) ? abs :
              !abs && LOCREF(ip->op[0]->base[0])
              && ip->op[0]->base[0]->sec==sec;
  }

  if (opt_mul && abs && (oc==0xc0c0 || oc==0xc1c0 || oc==0x4c00) &&
      !(mnemo->ext.opcode[1] & 0x0400) &&
      ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_Immediate) {
    /* MULU/MULS #x,Dn */
    int muls = (oc&0x0100)!=0 || (mnemo->ext.opcode[1]&0x0800)!=0;

    if (val == 0) {
      /* mulu/muls #0,Dn -> moveq #0,Dn */
      ip->code = OC_MOVEQ;
      ip->qualifiers[0] = l_str;
      if (final && warn_opts)
        cpu_error(51,"mulu/muls #0,Dn -> moveq #0,Dn");
    }

    else if (val == 1) {
      if (ext=='w' && muls) {
        /* muls.w #1,Dn -> ext.l Dn */
        ip->code = OC_EXT;
        ip->qualifiers[0] = l_str;
        if (final) {
          free_operand(ip->op[0]);
          if (warn_opts)
            cpu_error(51,"muls.w #1,Dn -> ext.l Dn");
        }
        ip->op[0] = ip->op[1];
        ip->op[1] = NULL;
      }
      else if (ext=='w' && !muls && (cpu_type&(mcfb|mcfc))) {
        /* ColdFire ISA_B/ISA_C: mulu.w #1,Dn -> mvz.w Dn,Dn */
        ip->code = OC_MVZ;
        if (final) {
          free_operand(ip->op[0]);
          if (warn_opts)
            cpu_error(51,"mulu.w #1,Dn -> mvz.w Dn,Dn");
        }
        ip->op[0] = ip->op[1];
      }
      else if (ext == 'l') {
        /* mulu.l/muls.l #1,Dn -> tst.l Dn */
        ip->code = OC_TST;
        if (final) {
          free_operand(ip->op[0]);
          if (warn_opts)
            cpu_error(51,"mulu/muls.l #1,Dn -> tst.l Dn");
        }
        ip->op[0] = ip->op[1];
        ip->op[1] = NULL;
      }
    }

    else if (val==-1 && muls) {
      if (ext == 'w') {
        /* muls.w #-1,Dn -> ext.l Dn + neg.l Dn */
        if (opt_speed) {
          ip->code = OC_EXT;
          ip->qualifiers[0] = l_str;
          if (final) {
            free_operand(ip->op[0]);
            if (warn_opts)
              cpu_error(51,"muls.w #-1,Dn -> ext.l Dn + neg.l Dn");
          }
          ip->op[0] = ip->op[1];
          ip->op[1] = NULL;
          ip->ext.un.copy.next = ip_singleop(OC_NEG,l_str,MODE_Dn,
                                             ip->op[0]->reg,0,0,NULL);
        }
      }
      else {
        /* muls.l #-1,Dn -> neg.l Dn */
        ip->code = OC_NEG;
        if (final) {
          free_operand(ip->op[0]);
          if (warn_opts)
            cpu_error(51,"mulu/muls #-1,Dn -> neg.l Dn");
        }
        ip->op[0] = ip->op[1];
        ip->op[1] = NULL;
      }
    }

    else if (val>=2 && val<=0x100 && cntones(val,9)==1) {
      val = lsbit(val,1,9);
      if (ext=='w' && muls && opt_speed) {
        /* muls.w #x,Dn -> ext.l Dn + asl.l #x,Dn */
        instruction *ip2 = copy_instruction(ip);

        ip->code = OC_EXT;
        ip->qualifiers[0] = l_str;
        ip2->code = OC_ASLI;
        ip2->qualifiers[0] = l_str;
        ip2->op[0]->extval[0] = val;
        if (final) {
          free_operand(ip->op[0]);
          free_expr(ip2->op[0]->value[0]);
          ip2->op[0]->value[0] = number_expr(val);
          if (warn_opts)
            cpu_error(51,"muls.w #x,Dn -> ext.l Dn + asl.l #x,Dn");
        }
        else
          ip2->op[0]->flags |= FL_DoNotEval;
        ip->op[0] = ip->op[1];
        ip->op[1] = NULL;
        ip->ext.un.copy.next = ip2;  /* append ASL */
        ip = ip2;  /* make stage 2 look at the ASL */
      }
      else if (ext=='w' && !muls && (cpu_type&(mcfb|mcfc)) && opt_speed) {
        /* ColdFire ISA_B/ISA_C: mulu.w #x,Dn -> mvz.w Dn,Dn + lsl.l #x,Dn */
        instruction *ip2 = copy_instruction(ip);

        ip->code = OC_MVZ;
        ip->qualifiers[0] = w_str;
        ip->op[0] = ip->op[1];
        ip2->code = OC_LSLI;
        ip2->qualifiers[0] = l_str;
        ip2->op[0]->extval[0] = val;
        if (final) {
          free_expr(ip2->op[0]->value[0]);
          ip2->op[0]->value[0] = number_expr(val);
          if (warn_opts)
            cpu_error(51,"mulu.w #x,Dn -> mvz.w Dn,Dn + lsl.l #x,Dn");
        }
        else
          ip2->op[0]->flags |= FL_DoNotEval;
        ip->ext.un.copy.next = ip2;  /* append LSL */
        /* ip = ip2;  stage 2 optimization doesn't make sense for ColdFire */
      }
      else if (ext == 'l') {
        /* mulu/muls.l #x,Dn -> lsl/asl.l #x,Dn */
        ip->code = muls ? OC_ASLI : OC_LSLI;
        if (final) {
          free_expr(ip->op[0]->value[0]);
          ip->op[0]->value[0] = number_expr(val);
          if (warn_opts)
            cpu_error(51,"mulu/muls.l #x,Dn -> lsl/asl.l #x,Dn");
        }
        else
          ip->op[0]->flags |= FL_DoNotEval;
        ip->op[0]->extval[0] = val;
      }
    }

    else if (val<=-2 && val>=-0x100 && muls && cntones(-val,9)==1) {
      val = lsbit(-val,1,9);
      if (opt_speed && ext=='w') {
        /* muls.w #-x,Dn -> ext.l Dn + asl.l #x,Dn + neg.l Dn */
        instruction *ip2 = copy_instruction(ip);

        ip->code = OC_EXT;
        ip->qualifiers[0] = l_str;
        ip2->code = OC_ASLI;
        ip2->qualifiers[0] = l_str;
        ip2->op[0]->extval[0] = val;
        if (final) {
          free_operand(ip->op[0]);
          free_expr(ip2->op[0]->value[0]);
          ip2->op[0]->value[0] = number_expr(val);
          if (warn_opts)
            cpu_error(51,"muls.w #-x,Dn -> ext.l Dn + asl.l #x,Dn + neg.l Dn");
        }
        else
          ip2->op[0]->flags |= FL_DoNotEval;
        ip->op[0] = ip->op[1];
        ip->op[1] = NULL;
        ip->ext.un.copy.next = ip2;  /* append ASL */
        ip2->ext.un.copy.next = ip_singleop(OC_NEG,l_str,MODE_Dn,
                                           ip2->op[1]->reg,0,0,NULL);
        ip = ip2;  /* make stage 2 look at the ASL */
      }
      else if (ext == 'l' && opt_speed) {
        /* muls.l #-x,Dn -> asl.l #x,Dn + neg.l Dn */
        ip->code = OC_ASLI;
        if (final) {
          free_expr(ip->op[0]->value[0]);
          ip->op[0]->value[0] = number_expr(val);
          if (warn_opts)
            cpu_error(51,"muls.l #-x,Dn -> asl.l #x,Dn + neg.l Dn");
        }
        else
          ip->op[0]->flags |= FL_DoNotEval;
        ip->op[0]->extval[0] = val;
        ip->ext.un.copy.next = ip_singleop(OC_NEG,l_str,MODE_Dn,
                                           ip->op[1]->reg,0,0,NULL);
      }
    }
  }

  /* STAGE 2 - reread, in case instruction was optimized in stage 1 */
  if (ip->code >= 0) {
    mnemo = &mnemonics[ip->code];
    oc = mnemo->ext.opcode[0];
    ext = ip->qualifiers[0] ?
          tolower((unsigned char)ip->qualifiers[0][0]) : '\0';
  }

  if ((ip->code==OC_MOVE || oc==0x0040) &&
      ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_Immediate) {
    int movqabsl = opt_moveq && abs && ext=='l' && ip->op[1]->mode==MODE_Dn;

    /* MOVE/MOVEA immediate instruction */
    if (movqabsl && val>=-0x80 && val<=0x7f) {
      /* move.l #x,Dn --> moveq #x,Dn */
      ip->code = OC_MOVEQ;
      ip->qualifiers[0] = l_str;
      if (final && warn_opts>1)
        cpu_error(51,"move.l #x,Dn -> moveq #x,Dn");
    }
    else if (movqabsl && (!(cpu_type & (m68040|mcf)) || opt_size) &&
             ((val>=0x80 && val<=0xfe) || (val>=-0x100 && val<=-0x82)) &&
             !(val&1)) {
      /* move.l #x,Dn --> moveq #x>>1,Dn ; add.w Dn,Dn */
      ip->code = OC_MOVEQ;
      ip->qualifiers[0] = l_str;
      if (final) {
        free_expr(ip->op[0]->value[0]);
        ip->op[0]->value[0] = number_expr(val >> 1);
        if (warn_opts > 1)
          cpu_error(51,"move.l #x,Dn -> moveq #x,Dn + add.w Dn,Dn");
      }
      else
        ip->op[0]->flags |= FL_DoNotEval;
      ip->op[0]->extval[0] = val >> 1;
      ip->ext.un.copy.next = ip_dualop(OC_ADD,(cpu_type & mcf)?l_str:w_str,
                                       MODE_Dn,ip->op[1]->reg,0,0,NULL,
                                       MODE_Dn,ip->op[1]->reg,0,0,NULL);
    }
    else if (opt_nmovq && abs && ext=='l' && ip->op[1]->mode==MODE_Dn &&
             !(cpu_type & (m68040|mcf)) && val>=0x80 && val<=0xff) {
      /* move.l #x,Dn --> moveq #x^$ff,Dn ; not.b Dn */
      ip->code = OC_MOVEQ;
      ip->qualifiers[0] = l_str;
      if (final) {
        free_expr(ip->op[0]->value[0]);
        ip->op[0]->value[0] = number_expr(val^0xff);
        if (warn_opts)
          cpu_error(51,"move.l #x,Dn -> moveq #y,Dn + not.b Dn");
      }
      else
        ip->op[0]->flags |= FL_DoNotEval;
      ip->op[0]->extval[0] = val^0xff;
      ip->ext.un.copy.next = ip_singleop(OC_NOT,b_str,
                                         MODE_Dn,ip->op[1]->reg,0,0,NULL);
    }
    else if (movqabsl && !(cpu_type & m68040) &&
             (((val&0xffff)==0 && val>=0x10000 && val<=0x7f0000) ||
              ((val&0xffff)==0xffff && (utaddr)val>=0xff80ffff
               && (utaddr)val<=0xfffeffff))) {
      /* move.l #x,Dn --> moveq #x>>16,Dn ; swap Dn */
      ip->code = OC_MOVEQ;
      ip->qualifiers[0] = l_str;
      if (final) {
        free_expr(ip->op[0]->value[0]);
        ip->op[0]->value[0] = number_expr(val >> 16);
        if (warn_opts > 1)
          cpu_error(51,"move.l #x,Dn -> moveq #y,Dn + swap Dn");
      }
      else
        ip->op[0]->flags |= FL_DoNotEval;
      ip->op[0]->extval[0] = val >> 16;
      ip->ext.un.copy.next = ip_singleop(OC_SWAP,w_str,
                                         MODE_Dn,ip->op[1]->reg,0,0,NULL);
    }
    else if (opt_nmovq && abs && ext=='l' && ip->op[1]->mode==MODE_Dn &&
             !(cpu_type & (m68040|mcf)) &&
             (((utaddr)val>=0xff81 && (utaddr)val<=0xffff) ||
              ((utaddr)val>=0xffff0001 && (utaddr)val<=0xffff0080))) {
      /* move.l #x,Dn --> moveq #-x,Dn ; neg.w Dn */
      ip->code = OC_MOVEQ;
      ip->qualifiers[0] = l_str;
      if (final) {
        free_expr(ip->op[0]->value[0]);
        ip->op[0]->value[0] = number_expr(-((int16_t)val));
        if (warn_opts)
          cpu_error(51,"move.l #x,Dn -> moveq #y,Dn + neg.w Dn");
      }
      else
        ip->op[0]->flags |= FL_DoNotEval;
      ip->op[0]->extval[0] = -((int16_t)val);
      ip->ext.un.copy.next = ip_singleop(OC_NEG,w_str,
                                         MODE_Dn,ip->op[1]->reg,0,0,NULL);
    }
    else if (opt_size && movqabsl && (val&0xff)==0 && !(cpu_type & mcf) &&
             val>=0x100 && val<=0x7f00) {
      /* move.l #x,Dn --> moveq #x>>n,Dn ; lsl.w #n,Dn */
      instruction *ip2 = ip_dualop(OC_LSLI,w_str,
                                   MODE_Extended,REG_Immediate,
                                   FL_NoOpt,0,ip->op[0]->value[0], /* dummy */
                                   MODE_Dn,ip->op[1]->reg,FL_NoOpt,0,NULL);
      int shift = msbit(val,14,8) - 6;

      ip->code = OC_MOVEQ;
      ip->qualifiers[0] = l_str;
      if (final) {
        free_expr(ip->op[0]->value[0]);
        ip->op[0]->value[0] = number_expr(val >> shift);
        ip2->op[0]->value[0] = number_expr(shift);
        if (warn_opts)
          cpu_error(51,"move.l #x,Dn -> moveq #x>>n,Dn + lsl.w #n,Dn");
      }
      else {
        ip->op[0]->flags |= FL_DoNotEval;
        ip2->op[0]->flags |= FL_DoNotEval;
      }
      ip->op[0]->extval[0] = val >> shift;
      ip2->op[0]->extval[0] = shift;
      ip->ext.un.copy.next = ip2;
    }
    else if (opt_gen && abs && val==0 && !(oc&0x0040) &&
             ((cpu_type & (m68010up|mcf|cpu32)) || opt_clr)) {
      /* move #0,<ea> --> clr <ea> */
      ip->code = OC_CLR;
      if (final)
        free_operand(ip->op[0]);
      ip->op[0] = ip->op[1];
      ip->op[1] = NULL;
      if (final && (warn_opts>1 ||
                    (warn_opts && !(cpu_type&(m68010up|mcf|cpu32)))))
        cpu_error(51,"move #0 -> clr");
    }
    else if (opt_moveq && abs && ext=='l' && (val==-1 || (val>=1 && val<=7)) &&
             (cpu_type & (mcfb|mcfc))) {
      /* move.l #x,<ea> -> mov3q #x,<ea> for x=-1,1..7 */
      ip->code = OC_MOV3Q;
      if (final && warn_opts>1)
        cpu_error(51,"move.l #x -> mov3q #x");
    }
    else if (opt_st && abs && ext=='b' && !(oc&0x0040) && (val&0xff)==0xff) {
      /* move.b #-1,<ea> --> st <ea> */
      if (!(cpu_type & mcf) || ip->op[1]->mode==MODE_Dn) {
        ip->code = OC_ST;
        if (final)
          free_operand(ip->op[0]);
        ip->op[0] = ip->op[1];
        ip->op[1] = NULL;
        if (final && warn_opts)
          cpu_error(51,"move.b #-1 -> st");
      }
    }
    else if (opt_pea && ip->op[1]->mode==MODE_AnPreDec &&
             ip->op[1]->reg==7 && ext=='l') {
      /* move.l #x,-(a7) --> pea x */
      if (abs && val>=-0x8000 && val<=0x7fff) {
        ip->op[0]->reg = REG_AbsShort;
        ip->code = OC_PEA;
        if (final)
          free_operand(ip->op[1]);
        ip->op[1] = NULL;
        if (final && warn_opts)
          cpu_error(51,"move.l #x,-(a7) -> pea x.w");
      }
      else if (cpu_type & (m68000|m68010|m68020|m68030|cpu32)) {
        /* Faster for 68000-68030 only. */
        ip->op[0]->reg = REG_AbsLong;
        ip->code = OC_PEA;
        if (final)
          free_operand(ip->op[1]);
        ip->op[1] = NULL;
        if (final && warn_opts)
          cpu_error(51,"move.l #x,-(a7) -> pea x.l");
      }
    }
    else if (opt_gen && oc==0x0040) {
      if (abs && val==0) {
        /* movea #0,An --> suba.l An,An */
        ip->code = OC_SUBA;
        ip->qualifiers[0] = l_str;
        ip->op[0]->mode = MODE_An;
        ip->op[0]->reg = ip->op[1]->reg;
        if (final && warn_opts>1)
          cpu_error(51,"movea #0,An -> suba.l An,An");
      }
      else if (!abs && ext=='l') {
        /* movea.l #label,An --> lea label,An */
        ip->code = OC_LEA;
        ip->op[0]->reg = REG_AbsLong;
        if (final && warn_opts>1)
          cpu_error(51,"movea.l #label,An -> lea label,An");
      }
    }
  }

  else if (opt_moveq && abs && (oc & 0xff7f)==0x7100 &&
           ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_Immediate &&
           ((val>=-0x80 && !(oc&0x0080)) || val>=0) && val<=0x7f) {
    /* MVS #-128..127,Dn or MVZ #0..127,Dn --> MOVEQ */
    ip->code = OC_MOVEQ;
    ip->qualifiers[0] = l_str;
    if (final && warn_opts>1)
      cpu_error(51,"mvs/mvz #x,Dn -> moveq #x,Dn");
  }

  else if ((opt_gen || opt_movem) && (oc & 0xfbff) == 0x4880) {
    /* MOVEM */
    int o = (oc & 0x0400) ? 1 : 0;

    if (ip->op[o]->mode==MODE_Extended &&
        (ip->op[o]->reg==REG_RnList || ip->op[o]->reg==REG_Immediate) &&
        ip->op[o]->base[0]==NULL && !(ip->op[o]->flags & FL_NoOpt)) {
      taddr list = ip->op[o]->extval[0];
      int regs = cntones(list,16);

      if (regs == 0) {
        /* no registers in list - delete instruction */
        ip->code = -1;
        if (final && warn_opts>1)
          cpu_error(51,"movem deleted");
      }
      else if (regs == 1) {
        /* a single register - MOVEM <ea>,Rn --> MOVE <ea>,Rn */
        if ((opt_movem || (!(list&0xff) && o==1)) &&
            !aindir_in_list(ip->op[o^1],list)) {
          signed char r = lsbit(list,0,16);

          ip->code = OC_MOVE;
          ip->op[o]->mode = REGisAn(r) ? MODE_An : MODE_Dn;
          ip->op[o]->reg = REGget(r);
          if (final && (warn_opts>1 || (warn_opts && ((list&0xff) || o==0))))
            cpu_error(51,"movem ea,Rn -> move ea,Rn");
        }
      }
      else if (regs==2 && opt_speed &&
               ((cpu_type & m68040) || (!(cpu_type & (m68000|m68010)) &&
                ip->op[o^1]->mode<=MODE_AnPreDec))) {
        /* MOVEM with two registers is faster with two separate MOVEs,
           when not using 68000 or 68010. Addressing modes with displacement
           or extended addressing modes for 68040 only. */
        taddr offs = ext=='l' ? 4 : 2;

        if ((opt_movem || (!(list&0xff) && o==1)) &&
            test_incr_ea(ip->op[o^1],offs) &&
            !aindir_in_list(ip->op[o^1],list)) {
          signed char r = lsbit(list,0,16);
          instruction *ip2;

          /* make two identical move instructions */
          ip->code = OC_MOVE;
          ip->op[o]->mode = REGisAn(r) ? MODE_An : MODE_Dn;
          ip->op[o]->reg = REGget(r);
          ip2 = copy_instruction(ip);
          r = lsbit(list,r+1,16);
          /* determine which register to move first */
          if (o==0 && ip->op[1]->mode==MODE_AnPreDec) {
            ip->op[o]->mode = REGisAn(r) ? MODE_An : MODE_Dn;
            ip->op[o]->reg = REGget(r);
          }
          else {
            ip2->op[o]->mode = REGisAn(r) ? MODE_An : MODE_Dn;
            ip2->op[o]->reg = REGget(r);
          }
          /* increment EA of second move */
          incr_ea(ip2->op[o^1],offs,final);
          ip->ext.un.copy.next = ip2;  /* append the 2nd MOVE */
          if (final && (warn_opts>1 || (warn_opts && ((list&0xff) || o==0))))
            cpu_error(51,"movem ea,Rm/Rn -> move ea,Rm + move ea,Rn");
        }
      }
    }
  }

  else if (opt_gen && !strcmp(mnemo->name,"fmovem") &&
           (mnemo->ext.opcode[1] & 0xcfff) == 0xc000) {
    /* FMOVEM */
    int o = (mnemo->ext.opcode[1] & 0x2000) ? 0 : 1;

    if (ip->op[o]->mode==MODE_Extended && ip->op[o]->reg==REG_FPnList &&
        ip->op[o]->base[0]==NULL && !(ip->op[o]->flags & FL_NoOpt) &&
        ip->op[o]->extval[0]==0) {
      /* no registers in list - delete instruction */
      ip->code = -1;
      if (final && warn_opts>1)
        cpu_error(51,"fmovem deleted");
    }
  }

  else if (opt_gen && oc==0x4200 && ip->op[0]->mode==MODE_Dn && ext=='l') {
    /* CLR.L Dn --> MOVEQ #0,Dn */
    ip->code = OC_MOVEQ;
    ip->qualifiers[0] = l_str;
    if (final) {
      ip->op[1] = ip->op[0];
      ip->op[0] = new_operand();
      ip->op[0]->mode = MODE_Extended;
      ip->op[0]->reg = REG_Immediate;
      ip->op[0]->value[0] = number_expr(0);
      if (warn_opts>1)
        cpu_error(51,"clr.l Dn -> moveq #0,Dn");
    }
    else
      ip->op[0] = NULL;
  }

  else if (opt_gen && abs && (oc==0xc000 || oc==0x0200) &&
           ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_Immediate) {
    /* ANDI/AND #x,<ea> */

    if ((cpu_type & (mcfb|mcfc)) && ip->op[1]->mode==MODE_Dn && ext=='l' &&
        (val==0xff || val==0xffff)) {
      /* ColdFire ISA_B/ISA_C: andi.l #$ff/$ffff,Dn -> mvz.b/w Dn,Dn */
      ip->code = OC_MVZ;
      ip->qualifiers[0] = val==0xff ? b_str : w_str;
      if (final) {
        free_operand(ip->op[0]);
        if (warn_opts>1)
          cpu_error(51,"andi.l #$ff/$ffff,Dn -> mvz.b/w Dn,Dn");
      }
      ip->op[0] = ip->op[1];
    }
    else if ((val==0xff && ext=='b') || (val==0xffff && ext=='w') ||
             (val==0xffffffff && ext=='l')) {
      /* andi.b/w/l #$ff/$ffff/$ffffffff,<ea> -> tst.b/w/l <ea> */
      ip->code = OC_TST;
      if (final) {
        free_operand(ip->op[0]);
        if (warn_opts>1)
          cpu_error(51,"andi.b/w/l #$ff/$ffff/$fffffff -> tst");
      }
      ip->op[0] = ip->op[1];
      ip->op[1] = NULL;
    }
    else if (val==0 && ((cpu_type & (m68010up|mcf|cpu32)) || opt_clr)) {
      /* andi.x #0,<ea> -> clr.x <ea> */
      ip->code = OC_CLR;
      if (final) {
        free_operand(ip->op[0]);
        if (warn_opts>1)
          cpu_error(51,"and #0 -> clr");
      }
      ip->op[0] = ip->op[1];
      ip->op[1] = NULL;
    }
  }

  else if (opt_gen && abs && val==0 &&
           (oc==0x8000 || oc==0x0000 || oc==0x0a00) &&
           ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_Immediate) {
    /* ORI/OR/EORI #0,<ea> --> TST <ea> */
    ip->code = OC_TST;
    if (final) {
      free_operand(ip->op[0]);
      if (warn_opts>1)
        cpu_error(51,"ori #0 -> tst");
    }
    ip->op[0] = ip->op[1];
    ip->op[1] = NULL;
  }

  else if (opt_gen && abs && oc==0x0a00) {
    if ((ext=='b' && (val&0xff)==0xff) ||
        (ext=='w' && (val&0xffff)==0xffff) || (ext=='l' && val==-1)) {
      /* EORI #-1,<ea> --> NOT <ea> */
      ip->code = OC_NOT;
      if (final)
        free_operand(ip->op[0]);
      ip->op[0] = ip->op[1];
      ip->op[1] = NULL;
      if (final && warn_opts>1)
        cpu_error(51,"eori #-1 -> not");
    }
  }

  else if ((oc==0x0600 || oc==0xd000 || oc==0xd0c0 ||
            oc==0x0400 || oc==0x9000 || oc==0x90c0)) {
    if (ip->op[0]->mode==MODE_Extended &&
        ip->op[0]->reg==REG_Immediate && abs) {
      /* ADD/ADDI/ADDA/SUB/SUBI/SUBA Immediate --> ADDQ/SUBQ */
      if (opt_quick && val>=1 && val<=8) {
        ip->code = (oc&0x4200) ? OC_ADDQ : OC_SUBQ;
        if (final && warn_opts>1)
          cpu_error(51,"add/sub #x -> addq/subq #x");
      }
      else if ((oc&0x90c0) == 0x90c0) {  /* ADDA/SUBA */
        if (!(oc & 0x4000))
          val = -val;
        if (opt_gen && val == 0) {
          ip->code = -1;  /* delete ADDA/SUBA #0,An */
          if (final && warn_opts>1)
            cpu_error(51,"adda/suba #0,An deleted");
        }
        else if (opt_lea && val>=-0x8000 && val<=0x7fff) {
          /* ADDA/SUBA Immediate --> LEA d(An),An */
          ip->qualifiers[0] = l_str;
          ip->code = OC_LEA;
          ip->op[0]->mode = MODE_An16Disp;
          ip->op[0]->reg = ip->op[1]->reg;
          if (!(oc&0x4000) && final) {
            free_expr(ip->op[0]->value[0]);
            ip->op[0]->value[0] = number_expr(val);
          }
          else
            ip->op[0]->flags |= FL_DoNotEval;
          ip->op[0]->extval[0] = val;
          if (final && warn_opts>1)
            cpu_error(51,"adda/suba #x,An -> lea (d,An),An");
        }
      }
    }
  }

  else if (oc==0x41c0) {
    if (ip->op[0]->mode==MODE_An16Disp && abs) {
      if (opt_gen && ip->op[0]->reg==ip->op[1]->reg && val==0) {
        /* delete LEA (0,An),An */
        ip->code = -1;
        if (final && warn_opts>1)
          cpu_error(51,"lea (0,An),An deleted");
      }
      else if (opt_lquick && ip->op[0]->reg==ip->op[1]->reg &&
               val!=0 && val>=-8 && val<=8) {
        /* LEA (d,An),An --> ADDQ/SUBQ #d,An */
        if (val < 0) {
          ip->code = OC_SUBQ;
          val = -val;
          if (final) {
            free_op_exp(ip->op[0]);
            ip->op[0]->value[0] = number_expr(val);
          }
        }
        else
          ip->code = OC_ADDQ;
        ip->qualifiers[0] = l_str;
        ip->op[0]->mode = MODE_Extended;
        ip->op[0]->reg = REG_Immediate;
        if (final && warn_opts>1)
          cpu_error(51,"lea (d,An),An -> addq/subq #d,An");
      }
      else if (opt_gen && (val<-0x8000 || val>0x7fff) &&
               !(cpu_type & (m68020up|cpu32))) {
        /* 68000/010: LEA (d32,Am),An --> MOVEA.L Am,An ; ADDA.L #d32,An */
        if (ip->op[0]->reg == ip->op[1]->reg) {
          /* special case: LEA (d32,An),An -> ADDA.L #d32,An */
          ip->code = OC_ADDA;
          ip->op[0]->mode = MODE_Extended;
          ip->op[0]->reg = REG_Immediate;
          ip->op[0]->flags |= FL_NoOpt;
        }
        else {
          instruction *ip2 = ip_dualop(OC_ADDA,l_str,
                                       MODE_Extended,REG_Immediate,
                                       FL_NoOpt,0,ip->op[0]->value[0],
                                       MODE_An,ip->op[1]->reg,
                                       FL_NoOpt,0,NULL);
          ip->code = OC_MOVEA;
          ip->op[0]->mode = MODE_An;
          ip->ext.un.copy.next = ip2;  /* append the ADDA */
        }
        ip->qualifiers[0] = l_str;
        if (final)
          cpu_error(47); /* lea-displacement out of range, changed */
      }
    }
    else if (opt_gen && ip->op[0]->mode==MODE_AnIndir &&
             ip->op[0]->reg==ip->op[1]->reg) {
      /* delete LEA (An),An */
      ip->code = -1;
      if (final && warn_opts>1)
        cpu_error(51,"lea (An),An deleted");
    }
    else if (opt_gen && abs && val==0 && ip->op[0]->mode==MODE_Extended &&
             (ip->op[0]->reg==REG_AbsShort || ip->op[0]->reg==REG_AbsLong)) {
      /* LEA 0,An -> SUBA.L An,An */
      ip->code = OC_SUBA;
      ip->qualifiers[0] = l_str;
      ip->op[0]->mode = MODE_An;
      ip->op[0]->reg = ip->op[1]->reg;
      if (final && warn_opts>1)
        cpu_error(51,"lea 0,An -> suba.l An,An");
    }
  }

  else if (opt_gen && oc==0x4808 && ip->op[1]->base[0]==NULL) {
    /* LINK.L --> LINK.W */
    taddr val = ip->op[1]->extval[0];

    if (val>=-0x8000 && val<=0x7fff) {
      ip->qualifiers[0] = w_str;
      ip->code--;
      if (final && warn_opts>1)
        cpu_error(51,"link.l -> link.w");
    }
  }

  else if (opt_gen && oc==0x4e50 && ip->op[1]->base[0]==NULL &&
           (cpu_type & (m68020up|cpu32))) {
    /* LINK.W --> LINK.L */
    taddr val = ip->op[1]->extval[0];

    if (val<-0x8000 || val>0x7fff) {
      ip->qualifiers[0] = l_str;
      ip->code++;
      if (final)
        cpu_error(45);  /* link.w changed to link.l */
    }
  }

  else if (oc==0x0c00 || oc==0xb000 || oc==0xb0c0) {
    if (opt_gen && abs && val==0 &&
        ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_Immediate) {
      /* CMP/CMPI/CMPA #0 --> TST */
      if (oc!=0xb0c0 || (cpu_type & (m68020up|cpu32|mcf))) {
        if (oc == 0xb0c0) {
          /* optimize both CMP.W #0,An and CMP.L #0,An to TST.L An */
          ip->code = OC_TST + 1;
          ip->qualifiers[0] = l_str;
        }
        else
          ip->code = OC_TST;
        if (final)
          free_operand(ip->op[0]);
        ip->op[0] = ip->op[1];
        ip->op[1] = NULL;
        if (final && warn_opts>1)
          cpu_error(51,"cmp #0 -> tst");
      }
    }
  }

  else if ((((oc&0xf1ff)==0xe100 && opt_gen) ||
            ((oc&0xf1ff)==0xe108 && opt_lsl)) && !(cpu_type&(m68060|mcf))) {
    /* LSL is only optimized with opt_lsl */
    if ((oc&0x0e00) == 0x0200)
      val = 1;  /* ASL/LSL Dn (missing immediate operand assumed as 1) */
    if (val == 1) {
      /* ASL/LSL #1,Dn --> ADD Dn,Dn */
      ip->code = OC_ADD;
      ip->op[0]->mode = MODE_Dn;
      if (!(oc&0x0e00))
        ip->op[0]->reg = ip->op[1]->reg;
      if (final && (warn_opts>1 || (warn_opts && (oc&0x0008))))
        cpu_error(51,"asl/lsl #1 -> add");
    }
    else if (opt_speed && opt_lsl && val==2 && (ext=='b' || ext=='w')) {
      /* ASL/LSL #2,Dn --> ADD Dn,Dn + ADD Dn,Dn (just .B and .W) */
      ip->code = OC_ADD;
      ip->op[0]->mode = MODE_Dn;
      ip->op[0]->reg = ip->op[1]->reg;
      ip->ext.un.copy.next = copy_instruction(ip);
      if (final && (warn_opts>1 || (warn_opts && (oc&0x0008))))
        cpu_error(51,"asl/lsl #2 -> add add");
    }
  }

  else if (opt_fconst && (mnemo->ext.available & (mfpu|m68040up)) &&
           abs && mnemo->operand_type[0]==FA && mnemo->operand_type[1]==F_ &&
           ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_Immediate) {
    unsigned char buf[12];
    uint64_t man;
    int exp;

    if (ext == 'x') {
      if (copy_float_exp(buf,ip->op[0],EXT_EXTENDED) != 0)
        ext = 0;
    }
    else if (ext == 'd') {
      if (copy_float_exp(buf,ip->op[0],EXT_DOUBLE) != 0)
        ext = 0;
    }
    else if (ext == 's') {
      if (copy_float_exp(buf,ip->op[0],EXT_SINGLE) != 0)
        ext = 0;
    }
    else
      ext = 0;

    if (ext == 'x') {
      /* Fxxx.X #m,FPn */
      int i;

      for (i=1,exp=0; i<12; i++)
        exp |= buf[i];
      if (!exp && (buf[0]==0x00 || buf[0]==0x80)) {
        /* Special case: 0.0 or -0.0 -> convert to double.
           We do not need to handle the "final" case here, because
           0.0 is always translated to single-precision in the next step. */
        ip->qualifiers[0] = d_str;
        ext = 'd';
      }
      else {
        exp = ((((int)buf[0]&0x7f)<<8) | (int)buf[1]) - 0x3fff;
        man = readval(1,buf+4,8) & 0x7fffffffffffffffLL;
        if (exp>=-0x3ff && exp<=0x400 && (man&0x7ff)==0) {
          /* double precision would be sufficient, so convert */
          int64_t v = (buf[0] & 0x80) ? -0x8000000000000000LL : 0;

          v = v | ((int64_t)(exp+0x3ff) << 52) | (man >> 11);
          if (final) {
            free_op_exp(ip->op[0]);
            ip->op[0]->value[0] = huge_expr(huge_from_int(v));
            if (warn_opts>1)
              cpu_error(51,"f<op>.x #m,FPn -> f<op>.d #m,FPn");
          }
          ip->qualifiers[0] = d_str;
          ext = 'd';
          setval(1,buf,8,v);
        }
      }
    }

    if (ext == 'd') {
      /* Fxxx.D #m,FPn */
      exp = ((((int)buf[0]&0x7f)<<4) | (((int)buf[1]&0xf0)>>4)) - 0x3ff;
      man = readval(1,buf,8) & 0xfffffffffffffLL;
      if ((exp>=-0x7f && exp<=0x80 && (man&0x1fffffff)==0) ||
          (exp==-0x3ff && man==0)) {  /* also allow all zeros for 0.0 */
        /* single precision would be sufficient, so convert */
        uint32_t v = (buf[0]&0x80) ? 0x80000000 : 0;  /* m. sign */

        if (exp != -0x3ff)
          v |= (uint32_t)(exp+0x7f) << 23;  /* exponent */
        v |= (uint32_t)(man >> 29);         /* mantissa */
        if (final) {
          free_op_exp(ip->op[0]);
          ip->op[0]->value[0] = number_expr((taddr)v);
          if (warn_opts>1)
            cpu_error(51,"f<op>.d #m,FPn -> f<op>.s #m,FPn");
        }
        ip->qualifiers[0] = s_str;
        ext = 's';
        setval(1,buf,4,v);
      }
    }

    if (final && (!strcmp(mnemo->name,"fdiv") || !strcmp(mnemo->name,"fsdiv")
        || !strcmp(mnemo->name,"fddiv") || !strcmp(mnemo->name,"fsgldiv"))) {
      /* FxDIV.s #m,FPn and FxDIV.d #m,FPn
         Can be optimized to FxMUL.s/FxMUL.d #1/m,FPn when m is a power of 2,
         which is the case when the mantissa is zero. */
      int optok = 0;

      if (ext == 's') {
        exp = ((((int)buf[0]&0x7f)<<1) | (((int)buf[1]&0x80)>>7)) - 0x7f;
        if ((readval(1,buf,4) & 0x007fffffLL) == 0
            && exp!=-0x7f) {
          setbits(1,buf,16,1,8,0x7f-exp);  /* 8-bit exponent to offset 1 */
          free_op_exp(ip->op[0]);
          ip->op[0]->value[0] = number_expr(readval(1,buf,4));
          optok = 1;
        }
      }
      else if (ext == 'd') {
        exp = ((((int)buf[0]&0x7f)<<4) | (((int)buf[1]&0xf0)>>4)) - 0x3ff;
        if ((readval(1,buf,8) & 0xfffffffffffffLL) == 0 && exp!=-0x3ff) {
          setbits(1,buf,16,1,11,0x3ff-exp);  /* 11-bit exponent to offset 1 */
          free_op_exp(ip->op[0]);
          ip->op[0]->value[0] = huge_expr(huge_from_mem(1,buf,8));
          optok = 1;
        }
      }
      else if (ext == 'x') {
        exp = ((((int)buf[0]&0x7f)<<8) | (int)buf[1]) - 0x3fff;
        if ((readval(1,buf+4,8) & 0x7fffffffffffffffLL) == 0) {
          setbits(1,buf,16,1,15,0x3fff-exp);  /* 15-bit exponent to offset 1 */
          free_op_exp(ip->op[0]);
          ip->op[0]->value[0] = huge_expr(huge_from_mem(1,buf,12));
          optok = 1;
        }
      }
      if (optok) {
        if (!strcmp(mnemo->name,"fdiv")) {
          ip->code = OC_FMUL;
          if (warn_opts>1)
            cpu_error(51,"fdiv #m,FPn -> fmul #1/m,FPn");
        }
        else if (!strcmp(mnemo->name,"fsdiv")) {
          ip->code = OC_FSMUL;
          if (warn_opts>1)
            cpu_error(51,"fsdiv #m,FPn -> fsmul #1/m,FPn");
        }
        else if (!strcmp(mnemo->name,"fddiv")) {
          ip->code = OC_FDMUL;
          if (warn_opts>1)
            cpu_error(51,"fddiv #m,FPn -> fdmul #1/m,FPn");
        }
        else if (!strcmp(mnemo->name,"fsgldiv")) {
          ip->code = OC_FSGLMUL;
          if (warn_opts>1)
            cpu_error(51,"fsgldiv #m,FPn -> fsglmul #1/m,FPn");
        }
      }
    }
  }

  else if ((oc&0xfeff) == 0x80c0 && abs &&
           ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_Immediate) {
    /* DIVU.W/DIVS.W #x,Dn */

    if (val == 0) {
      /* divu.w/divs.w #0,Dn */
      if (final)
        cpu_error(60);  /* division by zero */
    }
    else if (opt_div && val == 1 && (cpu_type & (mcfb|mcfc))) {
      /* ColdFire ISA_B/ISA_C: divu.w/divs.w #1,Dn -> mvz.w Dn,Dn */
      ip->code = OC_MVZ;
      if (final) {
        free_operand(ip->op[0]);
        if (warn_opts)
          cpu_error(51,"divu/divs.w #1,Dn -> mvz.w Dn,Dn");
      }
      ip->op[0] = ip->op[1];
    }
    else if (opt_div && opt_speed && val ==-1 && (oc&0x0100) &&
             (cpu_type & (mcfb|mcfc))) {
      /* ColdFire ISA_B/ISA_C: divs.w #-1,Dn -> neg.w Dn + mvz.w Dn,Dn */
      ip->code = OC_NEG;
      if (final) {
        free_operand(ip->op[0]);
        if (warn_opts)
          cpu_error(51,"divs.w #-1,Dn -> neg.w Dn + mvz.w Dn,Dn");
      }
      ip->op[0] = ip->op[1];
      ip->op[1] = NULL;
      ip->ext.un.copy.next = ip_dualop(OC_MVZ,w_str,
                                       MODE_Dn,ip->op[0]->reg,0,0,NULL,
                                       MODE_Dn,ip->op[0]->reg,0,0,NULL);
    }
  }

  else if (oc == 0x4c40) {
    if (abs &&
        ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_Immediate) {
      /* DIVU.L/DIVS.L #x,Dn */

      if (val == 0) {
        /* divu.l/divs.l #0,Dn */
        if (final)
          cpu_error(60);  /* division by zero */
      }
      else if (opt_div && mnemo->operand_type[1]==D_) {
        if (val == 1) {
          /* divu.l/divs.l #1,Dn -> tst.l Dn */
          ip->code = OC_TST;
          if (final) {
            free_operand(ip->op[0]);
            if (warn_opts)
              cpu_error(51,"divu/divs.l #1,Dn -> tst.l Dn");
          }
          ip->op[0] = ip->op[1];
          ip->op[1] = NULL;
        }
        else if (val==-1 && (mnemo->ext.opcode[1] & 0x0800)) {
          /* divs.l #-1,Dn -> neg.l Dn */
          ip->code = OC_NEG;
          if (final) {
            free_operand(ip->op[0]);
            if (warn_opts)
              cpu_error(51,"divs.l #-1,Dn -> neg.l Dn");
          }
          ip->op[0] = ip->op[1];
          ip->op[1] = NULL;
        }
        else if (val>=2 && val<=0x100 && (mnemo->ext.opcode[1] & 0x0800)==0 &&
                 cntones(val,9)==1) {
          /* divu.l #x,Dn -> lsr.l #x,Dn */
          ip->code = OC_LSRI;
          val = lsbit(val,1,9);
          if (final) {
            free_expr(ip->op[0]->value[0]);
            ip->op[0]->value[0] = number_expr(val);
            if (warn_opts)
              cpu_error(51,"divu.l #x,Dn -> lsr.l #x,Dn");
          }
          else
            ip->op[0]->flags |= FL_DoNotEval;
          ip->op[0]->extval[0] = val;
        }
      }
    }
    else if (mnemo->operand_type[1]==DD && !(mnemo->ext.opcode[1]&0x0400) &&
             ((ip->op[1]->reg>>4) & 0xf) == (ip->op[1]->reg & 0xf))
      /* DIVxL.L <ea>,Dn:Dn is DIVx.L <ea>,Dn */
      cpu_error(65);  /* Dr and Dq are identical! */
  }

  else if (oc==0x4ec0 || oc==0x4e80) {
    if (pcrelok && opt_pc && !(ip->op[0]->flags & FL_NoOpt) &&
        ip->op[0]->mode==MODE_Extended &&
        (ip->op[0]->reg==REG_AbsLong || ip->op[0]->reg==REG_PC16Disp)) {
      /* JMP/JSR label --> BRA/BSR label */
      taddr diff = val - cpc;

      if (lastsize==0 || (diff==0 && (oc & 0x40))) {
        ip->code = -1;  /* delete a JMP to following location */
        if (final && warn_opts>1)
          cpu_error(51,"jmp deleted");
      }
      else if (diff>=-0x8000 && diff<=0x7fff) {
        if (diff>=-0x80 && diff<=0x7f) {
          if ((lastsize==2 && diff==0) ||
              (lastsize==4 && diff==2))
            ip->qualifiers[0] = w_str;
          else
            ip->qualifiers[0] = b_str;
          ip->code = (oc & 0x40) ? OC_BRA : OC_BSR;
          ip->op[0]->reg = REG_AbsLong;
        }
        else {
          ip->qualifiers[0] = w_str;
          ip->code = (oc & 0x40) ? OC_BRA : OC_BSR;
          ip->op[0]->reg = REG_AbsLong;
        }
        if (final && warn_opts>1)
          cpu_error(51,"jmp/jsr -> bra/bsr");
      }
    }
    else if (!(ip->op[0]->flags & FL_NoOpt) &&
             ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_AbsLong &&
             !abs && EXTREF(ip->op[0]->base[0])) {
      if (opt_sc) {
        /* JMP/JSR extlabel --> JMP/JSR extlabel(PC) */
        ip->op[0]->reg = REG_PC16Disp;
        if (final && warn_opts>1)
          cpu_error(51,"jmp/jsr -> jmp/jsr (PC)");
      }
      else if (opt_jbra && !kick1hunks && (cpu_type & (m68020up|cpu32))) {
        /* JMP/JSR extlabel -> BRA.L/BSR.L extlabel (68020+, CPU32) */
        ip->qualifiers[0] = l_str;
        ip->code = (oc & 0x40) ? OC_BRA+1 : OC_BSR+1;  /* +1 for .L form */
        if (final && warn_opts>1)
          cpu_error(51,"jmp/jsr -> bra.l/bsr.l");
      }
    }
  }

  else if ((oc & 0xf000)==0x6000) {
    /* Bcc label */
    if (opt_bra && ((ipflags&IFL_UNSIZED) || opt_allbra) && pcrelok) {
      taddr diff = val - cpc;
      int resolvewarn = (sec->flags&RESOLVE_WARN)!=0;

      switch (lastsize) {
        case 0:
#if 0
          /* keep branch deleted until no more optimizations took place */
          if (diff!=-2 && done)
#else
          if (diff != -2)
#endif
            ip->qualifiers[0] = b_str;
          else
            ip->code = -1;
          break;
        case 2:
          if (diff==0 && oc!=0x6100 && !resolvewarn)
            ip->code = -1;
          else if (diff<-0x80 || diff>0x7f || diff==0)
            ip->qualifiers[0] = w_str;
          else
            ip->qualifiers[0] = b_str;
          break;
        case 4:
          if (diff==2) {
            if (oc!=0x6100 && !resolvewarn)
              ip->code = -1;
            else
              ip->qualifiers[0] = w_str;
          }
          else if (diff>=-0x80 && diff<=0x80 && !resolvewarn) {
            ip->qualifiers[0] = b_str;
          }
          else if (diff<-0x8000 || diff>0x7fff) {
            if (cpu_type & (m68020up|cpu32|mcfb|mcfc)) {
              ip->qualifiers[0] = l_str;
            }
            else {
              ip->qualifiers[0] = emptystr;
              ipflags |= IFL_RETAINLASTSIZE;
              if (oc < 0x6200) {
                /* BRA/BSR label --> JMP/JSR label */
                ip->code = (oc==0x6000) ? OC_JMP : OC_JSR;
                if (final)
                  cpu_error(46);  /* branch out of range changed to jmp */
              }
              else {
                /* Bcc label --> B!cc *+8, JMP label */
                instruction *ip2;

                /* make a new absolute JMP to the Bcc's destination */
                ip2 = ip_singleop(OC_JMP,emptystr,
                                  MODE_Extended,REG_AbsLong,
                                  FL_NoOpt,0,ip->op[0]->value[0]);
                ip->code += (oc&0x0100) ? -2 : 2; /* negate branch condition */
                ip->qualifiers[0] = b_str;
                ip->op[0]->flags |= FL_NoOpt;
                ip->ext.un.copy.next = ip2;  /* append the JMP */
                if (final) {
                  /* assign "*+8" as the Bcc's expression */
                  ip->op[0]->value[0] = make_expr(ADD,curpc_expr(),
                          number_expr(phxass_compat ? 6 : 8));
                  cpu_error(46);  /* branch out of range changed to jmp */
                }
              }
            }
          }
          else
            ip->qualifiers[0] = w_str;
          break;
        case 6:
          if (diff>=-0x8000 && diff<=0x7fff && !resolvewarn)
            ip->qualifiers[0] = w_str;
          else
            ip->qualifiers[0] = l_str;
          break;
        default:
          if (ext == '\0')
            ip->qualifiers[0] = w_str;
          break;
      }
      if (final && warn_opts>1) {
        /* print the finally performed kind of optimization */
        if (ip->code == -1)
          cpu_error(51,"branch deleted");
        else {
          char new_ext = tolower((unsigned char)ip->qualifiers[0][0]);
          int oldsize = branch_size(orig_ext);
          int newsize = branch_size(new_ext);

          if (newsize != oldsize)
            cpu_error(newsize<oldsize?53:54,new_ext);
        }        
      }
    }
    else if (opt_branop && oc!=0x6100 && pcrelok && val-cpc==0 &&
             (ext=='b' || ext=='s')) {
      /* short-branch with zero-distance which cannot be optimized
         is turned into a NOP */
      ip->qualifiers[0] = emptystr;
      ip->code = OC_NOOP;
      if (final)
        free_operand(ip->op[0]);
      ip->op[0] = NULL;
      if (final)
        cpu_error(57);  /* bra.b *+2 turned into a nop */
    }
    else if (opt_brajmp && !abs && ip->op[0]->base[0]->sec!=sec &&
             LOCREF(ip->op[0]->base[0])) {
      /* reference to label from different section */
      ip->qualifiers[0] = emptystr;
      if (oc < 0x6200) {
        /* BRA/BSR label --> JMP/JSR label */
        ip->code = (oc==0x6000) ? OC_JMP : OC_JSR;
        if (final && warn_opts>1)
          cpu_error(52,"bra/bsr -> jmp/jsr");
      }
      else {
        /* Bcc label --> B!cc *+8, JMP label */
        instruction *ip2;

        /* make a new absolute JMP to the Bcc's destination */
        ip2 = ip_singleop(OC_JMP,emptystr,
                          MODE_Extended,REG_AbsLong,
                          FL_NoOpt,0,ip->op[0]->value[0]);
        ip->code += (oc&0x0100) ? -2 : 2; /* negate branch condition */
        ip->qualifiers[0] = b_str;
        ip->op[0]->flags |= FL_NoOpt;
        ip->ext.un.copy.next = ip2;  /* append the JMP */
        if (final) {
          /* assign "*+8" as the Bcc's expression */
          ip->op[0]->value[0] = make_expr(ADD,curpc_expr(),
                  number_expr(phxass_compat ? 6 : 8));
          if (warn_opts>1)
            cpu_error(52,"b<cc> label -> b<!cc> *+8, jmp label");
        }
      }
    }
  }

  else if ((oc & 0xff80)==0xf080 && ip->code!=OC_FNOP) {
    /* cpBcc label */
    if (opt_bra && ((ipflags&IFL_UNSIZED) || opt_allbra) && pcrelok) {
      taddr diff = val - cpc;

      switch (lastsize) {
        case 4:
          if (diff<-0x8000 || diff>0x7fff)
            ip->qualifiers[0] = l_str;
          else
            ip->qualifiers[0] = w_str;
          break;
        case 6:
          if (diff>=-0x8000 && diff<=0x7fff)
            ip->qualifiers[0] = w_str;
          else
            ip->qualifiers[0] = l_str;
          break;
        default:
          if (ext == '\0')
            ip->qualifiers[0] = w_str;
          break;
      }
      if (final && warn_opts>1 && (ipflags&IFL_UNSIZED))
        cpu_error(53,*(ip->qualifiers[0]));
    }
  }
  else if (oc==0xf000 && !(mnemo->ext.available & mfloat)
           && (mnemo->ext.opcode[1] & 0xe000) == 0x8000) {
    if (final && ip->op[2]!=NULL && ip->op[2]->base[0]==NULL
        && ip->op[2]->extval[0]==0 && ip->op[3]!=NULL)
      cpu_error(64);  /* An operand at level #0 causes F-line Exception */
  }

  if (opt_immaddr && abs && ext=='l' && ip->op[0]!=NULL &&
      ip->op[0]->mode==MODE_Extended && ip->op[0]->reg==REG_Immediate &&
      ip->op[1]!=NULL && ip->op[1]->mode==MODE_An &&
      val>=-0x8000 && val<=0x7fff &&
      !(cpu_type & mcf) && (mnemonics[ip->code].ext.size & SIZE_WORD) &&
      (mnemonics[ip->code].ext.opcode[0] & 0xfeff) != 0x5000) {
    /* op.L #x,An --> op.W #x,An (if not ColdFire and not ADDQ/SUBQ) */
    ip->qualifiers[0] = w_str;
    if (final && warn_opts>1)
      cpu_error(51,"<op>.L #x,An -> <op>.W #x,An");
  }

  /* Try to optimize operands again, in case an instruction was optimized. */
  /* WARNING: 'ext' may be trashed at this point! */

  for (ip=iplist; ip; ip=ip->ext.un.copy.next) {
    if (ip->code >= 0) {
      for (i=0; i<MAX_OPERANDS && ip->op[i]!=NULL; i++)
        optimize_oper(ip->op[i],&optypes[mnemonics[ip->code].operand_type[i]],
                      sec,pc,cpc,final);
    }
  }

  return ipflags;
}


static size_t oper_size(instruction *ip,operand *op,struct optype *ot)
/* returns number of bytes for a single operand */
{
  int mode = op->mode;
  int reg = op->reg;

  if (ot->flags & OTF_NOSIZE) {
    return 0;
  }
  else if (mode==MODE_An16Disp || (mode==MODE_Extended &&
           (reg==REG_PC16Disp || reg==REG_AbsShort))) {
    return 2;
  }
  else if (mode==MODE_Extended && reg==REG_AbsLong) {
    if (ot->flags & OTF_BRANCH)
      return (taddr)branch_size(ip->qualifiers[0] ?
                                tolower((unsigned char)ip->qualifiers[0][0]) :
                                '\0');
    else
      return 4;
  }
  else if (mode==MODE_Extended && reg==REG_Immediate) {
    switch (ip->qualifiers[0] ?
            tolower((unsigned char)ip->qualifiers[0][0]) : '\0') {
      case 'b':
      case 'w':
        return 2;
      case 'l':
      case 's':
        return 4;
      case 'q':
      case 'd':
        return 8;
      case 'x':
      case 'p':
        return 12;
    }
  }
  else if (mode==MODE_An8Format ||
           (mode==MODE_Extended && reg==REG_PC8Format)) {
    if (op->flags & FL_UsesFormat) {
      size_t n = 2;

      if (op->format & FW_FullFormat) {
        if (FW_getBDSize(op->format) == FW_Word)
          n += 2;
        else if (FW_getBDSize(op->format) == FW_Long)
          n += 4;
        if (FW_getIndSize(op->format) == FW_Word)
          n += 2;
        else if (FW_getIndSize(op->format) == FW_Long)
          n += 4;
      }
      return n;
    }
    else
      ierror(0);
  }

  return 0;
}


static size_t ip_size(instruction *ip)
{
  if (ip->code >= 0) {
    mnemonic *mnemo = &mnemonics[ip->code];
    size_t size = S_OPCODE_SIZE(mnemo->ext.size) << 1;
    int i;
    
    for (i=0; i<MAX_OPERANDS && ip->op[i]!=NULL; i++)
      size += oper_size(ip,ip->op[i],&optypes[mnemo->operand_type[i]]);
    return size;
  }
  return 0;
}


static size_t iplist_size(instruction *ip)
{
  size_t size = 0;

  do {
    size += ip_size(ip);
  }
  while ((ip = ip->ext.un.copy.next) != NULL);
  return size;
}


size_t instruction_size(instruction *realip,section *sec,taddr pc)
/* Calculate the size of the current instruction; must be identical
   to the data created by eval_instruction. */
{
  mnemonic *mnemo = &mnemonics[realip->code];
  char ext = realip->qualifiers[0] ?
             tolower((unsigned char)realip->qualifiers[0][0]) : '\0';
  int i;
  size_t size;
  instruction *ip;
  unsigned char extflags;
  uint16_t extsize;

  /* check if current mnemonic is valid for selected cpu-type */
  while (!(mnemo->ext.available & cpu_type)) {
    /* try next mnemonic from table, when it has still the same
       name and all operand-types */
    mnemonic *lastm = mnemo;

    mnemo++;
    if (strcmp(lastm->name,mnemo->name) || !optypes_subset(lastm,mnemo))
      cpu_error(0);  /* instruction not supported */
    realip->code++;
  }

  extsize = mnemo->ext.size;

  /* remember the instruction's original extension, before optimizations */
  if (realip->ext.un.real.orig_ext < 0)
    realip->ext.un.real.orig_ext = (signed char)ext;

  if (opt_allbra && ign_unambig_ext) {
    /* Strip the size extension from branch instructions, no matter if
       illegal or not. The optimizer will find the best size. */
    for (i=0; i<MAX_OPERANDS; i++) {
      if (mnemonics[realip->code].operand_type[i] == BR) {
        ext = '\0';
        realip->qualifiers[0] = emptystr;
        break;
      }
    }
  }

  if (ext == '\0') {
    /* remember when developer didn't specify a size extension */
    realip->ext.un.real.flags |= IFL_UNSIZED;

    /* assign a default extension for sized instructions, when missing */
    if ((extsize & SIZE_MASK) != 0) {
      if ((extsize & S_CFCHECK) && (cpu_type & mcf))
        extsize &= ~(SIZE_BYTE|SIZE_WORD);  /* SIZE_LONG for ColdFire only */
      if ((extsize & SIZE_LONG) && (cpu_type & mcf))  /* ColdFire prefers .l */
        realip->qualifiers[0] = l_str;
      else if (extsize & SIZE_WORD)
        realip->qualifiers[0] = w_str;
      else if (extsize & SIZE_BYTE)
        realip->qualifiers[0] = b_str;
      else if (extsize & SIZE_LONG)
        realip->qualifiers[0] = l_str;
      else if (extsize & SIZE_EXTENDED)
        realip->qualifiers[0] = x_str;
      else if (extsize & SIZE_SINGLE)
        realip->qualifiers[0] = s_str;
      else if (extsize & SIZE_DOUBLE)
        realip->qualifiers[0] = d_str;
      else if (extsize & SIZE_PACKED)
        realip->qualifiers[0] = p_str;
      else
        ierror(0);
      ext = realip->qualifiers[0][0];
    }
  }

  /* check if opcode extension is valid */
  if ((mnemo->ext.size & SIZE_MASK) == SIZE_UNSIZED) {
    if (ext != '\0') {
      if (!ign_unsized_ext)
        cpu_error(35);  /* extension for unsized instruction ignored */
      ext = '\0';
      realip->qualifiers[0] = emptystr;
    }
  }
  else {
    int uacode;
    int err = 0;
    uint16_t extsize = mnemo->ext.size;
    uint16_t sz  = lc_ext_to_size(ext);  /* convert ext. to SIZE-code */

    if ((mnemo->ext.size&SIZE_UNAMBIG) && (mnemo->ext.available&cpu_type))
      uacode = realip->code;  /* last mnemonic with unambiguous size ext. */
    else
      uacode = -1;

    /* Find a mnemonic with same name and operands which matches the
       given size extension. */
    while (!((((extsize&S_CFCHECK) && (cpu_type&mcf)) ?
              (extsize & ~(SIZE_BYTE|SIZE_WORD)) : extsize) & sz)) {
      mnemo++;
      if ((err = strcmp(mnemonics[realip->code].name,mnemo->name)) != 0)
        break;
      if ((err = !optypes_subset(&mnemonics[realip->code],mnemo)) != 0)
        break;

      realip->code++;
      extsize = mnemo->ext.size;
      if ((mnemo->ext.size&SIZE_UNAMBIG) && (mnemo->ext.available&cpu_type))
        uacode = realip->code;
    }

    if (err) {
      if (ign_unambig_ext && uacode>=0) {
        /* size extension is wrong, but we can guess the right one */
        realip->code = uacode;
        mnemo = &mnemonics[uacode];
        extsize = mnemo->ext.size;
        ext = '\0';
        realip->qualifiers[0] = emptystr;
      }
      else
        cpu_error(34);  /* illegal opcode extension */
    }

    if (!(mnemo->ext.available & cpu_type))
      cpu_error(0);  /* instruction not supported */
  }

  /* check if we are uncertain about the side of a register list operand */
#if 0
  /* @@@ Why should we forbid optimizations here? FL_PossRegList is useless? */
  if (realip->op[0]!=NULL && realip->op[1]!=NULL) {
    if ((realip->op[0]->flags & FL_PossRegList) &&
        realip->op[1]->mode==MODE_Extended &&
        realip->op[1]->reg==REG_AbsLong) {
      realip->op[0]->flags &= ~FL_PossRegList;
      realip->op[0]->flags |= FL_NoOpt;
      realip->op[1]->flags |= FL_NoOpt;
    }
    else if ((realip->op[1]->flags & FL_PossRegList) &&
             realip->op[0]->mode==MODE_Extended &&
             realip->op[0]->reg==REG_AbsLong) {
      realip->op[1]->flags &= ~FL_PossRegList;
      realip->op[1]->flags |= FL_NoOpt;
      realip->op[0]->flags |= FL_NoOpt;
    }
  }
#endif

  /* fix instructions, which were not correctly recognized through
     parse_operand() due to missing information. */
  if (mnemo->ext.opcode[0]==0xf518 && !(cpu_type & m68040up)) {
    /* try 68030/68851 PFLUSHA instead */
    realip->code++;
  }

  /* do optimizations on a copy of the current instruction */
  ipslot = 0;
  ip = copy_instruction(realip);
  extflags = optimize_instruction(ip,sec,pc,0);

  /* and determine current size (from optimized copy) */
  size = iplist_size(ip);
  if (!(extflags & IFL_RETAINLASTSIZE))
    realip->ext.un.real.last_size = size;  /* remember size for next pass */

  return size;
}


static void write_val(unsigned char *d,int pos,int size,taddr val,int sign)
/* insert value 'val' with 'size' bits at bit-position 'pos' */
{
  if (typechk) {
    if (sign) {
      if ((val > (1L << (size-1)) - 1) || (val < -(1L << (size-1)))) {
        if (val > 0 && val < (1L << size))
          cpu_error(27,val,-(1L<<(size-1)),
                    (1L<<(size-1))-1,
                    val-(1L<<size));    /* using signed operand as unsigned */
        else
          cpu_error(25,val,-(1L<<(size-1)),
                    (1L<<(size-1))-1);  /* operand value out of range */
      }
    }
    else {
      if ((utaddr)val > (1L << size) - 1)
        cpu_error(25,val,0,(1L<<size)-1);  /* operand value out of range */
    }
  }

  d += pos>>3;
  pos &= 7;

  while (size > 0) {
    int shift = 8-pos-size;
    unsigned char v;

    if (shift > 0)
      v = (val << shift) & 0xff;
    else if (shift < 0)
      v = (val >> -shift) & 0xff;
    else
      v = val & 0xff;
    *d++ |= v;
    size -= 8-pos;
    pos = 0;
  }
}


static unsigned char *write_branch(dblock *db,unsigned char *d,operand *op,
                                   char ext,section *sec,taddr pc,
                                   mnemonic *mnemo,int bcc)
/* calculate and write branch displacement of desired size, handle relocs */
{
  int lbra = (mnemo->ext.size & SIZE_LONG) != 0;

  if (!bcc && ext!='w' && ext!='l')
    ierror(0);

  if (op->base[0] && is_pc_reloc(op->base[0],sec)) {
    /* external branch label, or label from different section */
    taddr addend = op->extval[0];
    int size,offset;

    switch (ext) {
      case 'b':
      case 's':
        addend--;   /* reloc-offset is stored 1 byte before PC-location */
        *(d-1) = addend & 0xff;
        size = 8;
        offset = 1;
        break;
      case 'l':
        if (lbra) {
          if (bcc)
            *(d-1) = 0xff;
          offset = d - (unsigned char *)db->data;
          d = setval(1,d,4,addend);
          size = 32;
        }
        else
          cpu_error(0);  /* instruction not supported */
        break;
      case 'w':
        if (bcc)
          *(d-1) = 0;
        offset = d - (unsigned char *)db->data;
        d = setval(1,d,2,addend);
        size = 16;
        break;
      default:
        cpu_error(34);  /* illegal opcode extension */
        break;
    }
    add_extnreloc(&db->relocs,op->base[0],addend,REL_PC,0,size,offset);
  }
  else {
    /* known label from the same section - can be resolved immediately */
    taddr diff = op->extval[0] - pc;

    switch (ext) {
      case 'b':
      case 's':
        if (diff>=-0x80 && diff<=0x7f && diff!=0 && (diff!=-1 || !lbra))
          *(d-1) = diff & 0xff;
        else
          cpu_error(28);  /* branch destination out of range */
        break;
      case 'l':
        if (lbra) {
          if (bcc)
            *(d-1) = 0xff;
          d = setval(1,d,4,diff);
        }
        else
          cpu_error(0);  /* instruction not supported */
        break;
      case 'w':
        if (diff>=-0x8000 && diff<=0x7fff) {
          if (bcc)
            *(d-1) = 0;
          d = setval(1,d,2,diff);
        }
        else
          cpu_error(28);  /* branch destination out of range */
        break;
      default:
        cpu_error(34);  /* illegal opcode extension */
        break;
    }
  }

  return d;
}


static unsigned char *write_extval(int num,size_t size,dblock *db,
                                   unsigned char *d,operand *op,int rtype)
{
  if (rtype==REL_ABS && op->basetype[num]==BASE_PCREL)
    op->extval[num] += d - db->data;  /* fix addend for label differences */

  return setval(1,d,size,op->extval[num]);
}


static unsigned char *write_ea_ext(dblock *db,unsigned char *d,operand *op,
                                   char ext,section *sec,taddr pc)
/* write effective address extension words, handle relocs */
{
  if (op->mode>MODE_Extended ||
      (op->mode==MODE_Extended && op->reg>REG_Immediate)) {
    ierror(0);
  }
  else if (op->mode >= MODE_An16Disp) {
    int rtype = REL_NONE;
    int roffs = d - (unsigned char *)db->data;
    int rsize = 0;
    int ortype = REL_NONE;
    int orsize = 0;

    if (op->flags & FL_020up) {
      if (!(cpu_type & (m68020up|cpu32))) {
        cpu_error(0);  /* instruction not supported */
      }
      else if (op->flags & FL_noCPU32) {
        if (cpu_type & cpu32)
          cpu_error(0);  /* instruction not supported */
      }
    }

    if (op->mode == MODE_An16Disp) {
      /* d16(An) needs one extension word */
      if (op->base[0]) {
        rsize = 16;
        if ((EXTREF(op->base[0]) && op->reg!=sdreg) ||
            op->basetype[0]==BASE_PCREL)
          rtype = REL_ABS;
        else if (op->basetype[0]==BASE_OK)
          rtype = REL_SD;
        else
          general_error(38);  /* illegal relocation */
      }
      if (rtype==REL_SD && LOCREF(op->base[0])) {
        if (typechk && (op->extval[0]<0 || op->extval[0]>0xffff))
          cpu_error(29);  /* displacement out of range */
      }
      else {
        if (typechk && (op->extval[0]<-0x8000 || op->extval[0]>0x7fff))
          cpu_error(29);  /* displacement out of range */
      }
      d = write_extval(0,2,db,d,op,rtype);
    }

    else if (op->mode == MODE_An8Format) {
      if (!(op->flags & FL_UsesFormat))
        ierror(0);
      if (op->format & FW_FullFormat) {
        /* ([bd,An,Rn.x*s],od): format word + base- and outer-displacement */
        d = setval(1,d,2,op->format);
        roffs += 2;
        if (FW_getBDSize(op->format) == FW_Word) {
          if (op->base[0]) {
            rsize = 16;
            if ((EXTREF(op->base[0]) && (!extsd || op->reg!=sdreg)) ||
                op->basetype[0]==BASE_PCREL)
              rtype = REL_ABS;
            else if (extsd && op->reg==sdreg && op->basetype[0]==BASE_OK)
              rtype = REL_SD;
            else
              general_error(38);  /* illegal relocation */
          }
          if (rtype==REL_SD && LOCREF(op->base[0])) {
            if (typechk && (op->extval[0]<0 || op->extval[0]>0xffff))
              cpu_error(29);  /* displacement out of range */
          }
          else {
            if (typechk && (op->extval[0]<-0x8000 || op->extval[0]>0x7fff))
              cpu_error(29);  /* displacement out of range */
          }
          d = write_extval(0,2,db,d,op,rtype);
        }
        else if (FW_getBDSize(op->format) == FW_Long) {
          if (op->base[0]) {
            rtype = REL_ABS;
            rsize = 32;
          }
          d = write_extval(0,4,db,d,op,rtype);
        }
        if (FW_getIndSize(op->format) == FW_Word) {
          if (typechk && (op->extval[1]<-0x8000 || op->extval[1]>0x7fff))
            cpu_error(29);  /* displacement out of range */
          if (op->base[1]) {
            orsize = 16;
            if (EXTREF(op->base[1]) ||
                (LOCREF(op->base[1]) && op->basetype[1]==BASE_PCREL))
              ortype = REL_ABS;
            else
              cpu_error(30);  /* absolute displacement expected */
          }
          d = write_extval(1,2,db,d,op,ortype);
        }
        else if (FW_getIndSize(op->format) == FW_Long) {
          if (op->base[1]) {
            ortype = REL_ABS;
            orsize = 32;
          }
          d = write_extval(1,4,db,d,op,ortype);
        }
      }
      else {
        /* (d8,An,Rn.x*s) needs one format word as extension */
        if (typechk && (op->extval[0]<-0x80 || op->extval[0]>0x7f))
          cpu_error(29);  /* displacement out of range */
        if (op->base[0]) {
          rsize = 8;
          if (EXTREF(op->base[0]) ||
              (LOCREF(op->base[0]) && op->basetype[0]==BASE_PCREL))
            rtype = REL_ABS;
          else
            cpu_error(30);  /* absolute displacement expected */
        }
        *d++ = (op->format>>8) & 0xff;
        *d++ = op->extval[0] & 0xff;
      }
    }

    else if (op->mode == MODE_Extended) {

      if (op->reg == REG_PC16Disp) {
        /* d16(PC) needs one extension word */
        taddr disp = op->extval[0];

        if (op->base[0]!=NULL && is_pc_reloc(op->base[0],sec)) {
          rtype = REL_PC;
          rsize = 16;
        }
        else if (op->base[0]!=NULL || (sec->flags&ABSOLUTE))
          disp = op->extval[0] - pc;
        else
          cpu_error(no_dpc ? 26 : 68);  /* absolute PC-displacement */
        if (typechk && (disp<-0x8000 || disp>0x7fff))
          cpu_error(29);  /* displacement out of range */
        d = setval(1,d,2,disp);
      }

      else if (op->reg == REG_PC8Format) {
        taddr disp = op->extval[0];

        if (!(op->flags & FL_UsesFormat))
          ierror(0);
        if (op->format & FW_FullFormat) {
          /* ([bd,PC,Rn.x*s],od): format word + base- and outer-displacement */
          d = setval(1,d,2,op->format);
          roffs += 2;
          if (FW_getBDSize(op->format) == FW_Word) {
            if (op->base[0]!=NULL && is_pc_reloc(op->base[0],sec)) {
              rtype = REL_PC;
              rsize = 16;
              op->extval[0] += 2;  /* pc-relative xref fix */
              disp += 2;
            }
            else if (op->base[0]!=NULL || (sec->flags&ABSOLUTE))
              disp = op->extval[0] - pc;
            else
              cpu_error(no_dpc ? 26 : 68);  /* absolute PC-displacement */
            if (typechk && (disp<-0x8000 || disp>0x7fff))
              cpu_error(29);  /* displacement out of range */
            d = setval(1,d,2,disp);
          }
          else if (FW_getBDSize(op->format) == FW_Long) {
            if (op->base[0]!=NULL && is_pc_reloc(op->base[0],sec)) {
              rtype = REL_PC;
              rsize = 32;
              op->extval[0] += 2;  /* pc-relative xref fix */
              disp += 2;
            }
            else if (op->base[0]!=NULL || (sec->flags&ABSOLUTE))
              disp = op->extval[0] - pc;
            else
              cpu_error(no_dpc ? 26 : 68);  /* absolute PC-displacement */
            d = setval(1,d,4,disp);
          }
          if (FW_getIndSize(op->format) == FW_Word) {
            if (typechk && (op->extval[1]<-0x8000 || op->extval[1]>0x7fff))
              cpu_error(29);  /* displacement out of range */
            if (op->base[1]) {
              if (EXTREF(op->base[1])) {
                ortype = REL_ABS;
                orsize = 16;
              }
              else
                cpu_error(30);  /* absolute displacement expected */
            }
            d = write_extval(1,2,db,d,op,ortype);
          }
          else if (FW_getIndSize(op->format) == FW_Long) {
            if (op->base[1]) {
              ortype = REL_ABS;
              orsize = 32;
            }
            d = write_extval(1,4,db,d,op,ortype);
          }
        }
        else {
          /* (d8,PC,Rn.x*s) needs one format word as extension */
          if (op->base[0]!=NULL && is_pc_reloc(op->base[0],sec)) {
            rtype = REL_PC;
            rsize = 8;
            roffs++;
            op->extval[0] += 1;  /* pc-relative xref fix */
            disp += 1;
          }
          else if (op->base[0]!=NULL || (sec->flags&ABSOLUTE))
            disp = op->extval[0] - pc;
          else
            cpu_error(no_dpc ? 26 : 68);  /* absolute PC-displacement */
          if (typechk && (disp<-0x80 || disp>0x7f))
            cpu_error(29);  /* displacement out of range */
          *d++ = (op->format>>8) & 0xff;
          *d++ = disp & 0xff;
        }
      }

      else if (op->reg == REG_AbsShort) {
        /* label.w */
        if (typechk && (op->extval[0]<-0x8000 || op->extval[0]>0x7fff))
          cpu_error(32);  /* absolute short address out of range */
        if (op->base[0]) {
          rtype = REL_ABS;
          rsize = 16;
        }
        d = write_extval(0,2,db,d,op,rtype);
      }

      else if (op->reg == REG_AbsLong) {
        /* label.l */
        if (op->base[0]) {
          rtype = REL_ABS;
          rsize = 32;
        }
        d = write_extval(0,4,db,d,op,rtype);
      }

      else if (op->reg == REG_Immediate) {
        /* #immediate */
        int err;

        if (op->base[0] != NULL)
          rtype = REL_ABS;

        switch (ext) {
          case 'b':
            if (op->flags & FL_ExtVal0) {
              roffs++;
              rsize = 8;
              *d++ = 0;
              d = write_extval(0,1,db,d,op,rtype);
              if (typechk && (op->extval[0]<-0x80 || op->extval[0]>0xff))
                cpu_error(36);  /* immediate operand out of range */
            }
            else
              cpu_error(37);  /* immediate operand has illegal type */
            break;
          case 'w':
            if (op->flags & FL_ExtVal0) {
              rsize = 16;
              d = write_extval(0,2,db,d,op,rtype);
              if (typechk && (op->extval[0]<-0x8000 || op->extval[0]>0xffff))
                cpu_error(36);  /* immediate operand out of range */
            }
            else
              cpu_error(37);  /* immediate operand has illegal type */
            break;
          case 'l':
            if (op->flags & FL_ExtVal0) {
              rsize = 32;
              d = write_extval(0,4,db,d,op,rtype);
            }
            else if (type_of_expr(op->value[0]) == FLT) {
              if ((err = copy_float_exp(d,op,EXT_SINGLE)) != 0)
                cpu_error(err);
              d += 4;
            }
            else
              cpu_error(37);  /* immediate operand has illegal type */
            break;
          case 's':
            if ((err = copy_float_exp(d,op,EXT_SINGLE)) != 0)
              cpu_error(err);
            d += 4;
            break;
          case 'd':
          case 'q':
            if ((err = copy_float_exp(d,op,EXT_DOUBLE)) != 0)
              cpu_error(err);
            d += 8;
            break;
          case 'x':
            if ((err = copy_float_exp(d,op,EXT_EXTENDED)) != 0)
              cpu_error(err);
            d += 12;
            break;
          case 'p':
            if ((err = copy_float_exp(d,op,EXT_PACKED)) != 0)
              cpu_error(err);
            d += 12;
            break;
        }
      }
    }

    /* append relocations */
    if (rtype != REL_NONE) {
      if (rtype==REL_ABS && op->basetype[0]==BASE_PCREL)
        rtype = REL_PC;
      add_extnreloc(&db->relocs,op->base[0],op->extval[0],rtype,0,rsize,roffs);
    }
    if (ortype != REL_NONE) {
      if (ortype==REL_ABS && op->basetype[1]==BASE_PCREL)
        ortype = REL_PC;
      add_extnreloc(&db->relocs,op->base[1],op->extval[1],ortype,0,orsize,
                    (rtype==REL_NONE) ? roffs : roffs+rsize/8);
    }
  }
  return d;
}


static uint16_t apollo_bank_prefix(instruction *ip)
/* generate Apollo bank prefix */
{
  uint16_t bank = 0x7100;
  uint16_t ddddd = 0;

  /* calculate bank prefix */
  if (ip->op[0]->mode == MODE_SpecReg) {
    uint16_t aaReg = ip->op[0]->reg - REG_VX00;  /* e0 - e23 */
    bank |= (1 + (aaReg >> 3)) << 2;
  }

  if (ip->op[1] != NULL) {
    if (ip->op[1]->mode == MODE_SpecReg) {
      uint16_t bbReg = ip->op[1]->reg - REG_VX00;  /* e0 - e23 */
      ddddd |= ((bbReg % 8) << 2) | (1 + (bbReg >> 3));  /* ddd-dd==reg-bank */
      bank |= (1 + (bbReg >> 3));
    }
    else
      ddddd = ip->op[1]->reg << 2;
  }
  else {
    /* For a single operand instr, both AA and BB should be the same */
    uint16_t bbReg = ip->op[0]->reg - REG_VX00;  /* e0 - e23 */
    bank |= (1 + (bbReg >> 3));
  }

  switch (ip_size(ip)) {
    case 4:
      /* SS = 00 */
      break;
    case 6:
      /* SS = 01 */
      bank |= 0x40;
      break;
    case 8:
      /* SS = 10 */
      bank |= 0x80;
      break;
    case 10:
      /* SS = 11 */
      bank |= 0xc0;
      break;
    default:
      cpu_error(70); /* bank prefix not encodable due to size limit */
      break;
  }

  /* handle 3rd operand */
  if (ip->op[2] != NULL) {
    if (ip->op[2]->mode != -1) {
      /* optional operand was not omitted - refer to m68k_operand_optional() */
      uint16_t dbank;

      if (ip->op[2]->mode == MODE_FPn)
        dbank = ip->op[2]->reg << 2;
      else if (ip->op[2]->mode == MODE_SpecReg)
        dbank = (1 + ((ip->op[2]->reg - REG_VX00) >> 3)) |
                (((ip->op[2]->reg - REG_VX00) % 8) << 2);
      else
        ierror(0);

      ddddd ^= dbank;
      bank |= (((ddddd >> 2) & 7) << 9) | ((ddddd & 3) << 4);
    }
    free_operand(ip->op[2]);
    ip->op[2] = NULL;
  }

  return bank;
}


dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
/* Convert an instruction into a DATA atom, including relocations
   if necessary. */
{
  dblock *db = new_dblock();
  unsigned char ipflags = ip->ext.un.real.flags;
  signed char lastsize = ip->ext.un.real.last_size;
  instruction *realip = ip;
  uint8_t *d;

  /* really execute optimizations now */
  ipslot = 0;
  optimize_instruction(ip,sec,pc,1);

  /* determine instruction size and allocate data atom */
  if ((db->size = iplist_size(ip)) != 0) {
    d = db->data = mymalloc(db->size);
  }
  else {
    db->data = NULL;
    goto eval_done;
  }

  /* encode instructions */
  do {
    if (ip->code >= 0) {
      mnemonic *mnemo = &mnemonics[ip->code];
      char ext = ip->qualifiers[0] ?
                 tolower((unsigned char)ip->qualifiers[0][0]) : '\0';
      uint16_t sz = ((mnemo->ext.size & SIZE_MASK) == SIZE_UNSIZED) ?
                    SIZE_UNSIZED : lc_ext_to_size(ext);
      unsigned char *dbstart = d;
      int i;

      /* warn about a bad alias instruction mnemonic */
      if (mnemo->ext.available & malias)
          cpu_error(33);  /* deprecated instruction alias */

      if (mnemo->ext.available & mbanked) {
        /* write Apollo bank prefix */
        uint16_t bank = apollo_bank_prefix(ip);

        *d++ = bank >> 8;
        *d++ = bank & 0xff;
        pc += 2;
        dbstart += 2;

        /* reduce operands back to base types */
        for (i=0; i<MAX_OPERANDS && ip->op[i]!=NULL; i++) {
          operand *op = ip->op[i];

          if (op->mode == MODE_SpecReg) {
            op->mode = mnemo->ext.place[i] == SEA ? MODE_Dn : MODE_FPn;
            op->reg = (op->reg - REG_VX00) % 8;
            op->flags = 0;
          }
        }
      }

      /* copy opcode */
      if ((mnemo->ext.available & (mcffpu | mbanked)))
        *d++ = (mnemo->ext.opcode[0] >> 8) | 2;  /* fixed coprocessor id */
      else if (mnemo->ext.available & mfpu)
        *d++ = (mnemo->ext.opcode[0] >> 8) | (fpu_id << 1);
      else
        *d++ = mnemo->ext.opcode[0] >> 8;

      *d++ = mnemo->ext.opcode[0] & 0xff;
      pc += 2;

      if (S_OPCODE_SIZE(mnemo->ext.size) > 1) {
        *d++ = mnemo->ext.opcode[1] >> 8;
        *d++ = mnemo->ext.opcode[1] & 0xff;
        pc += 2;

        if (S_OPCODE_SIZE(mnemo->ext.size) > 2 &&
            !(mnemo->ext.available & mbanked)) {
          *d++ = 0;
          *d++ = 0;
          pc += 2;
        }
      }

      /* insert size-extension into opcode, when required */
      switch (S_SIZEMODE(mnemo->ext.size)) {
        case S_NONE:
          break;
        case S_STD:
          switch (sz) {
            case SIZE_WORD: *(dbstart+1) |= 0x40; break;
            case SIZE_LONG: *(dbstart+1) |= 0x80; break;
          }
          break;
        case S_STD1:
          switch (sz) {
            case SIZE_BYTE: *(dbstart+1) |= 0x40; break;
            case SIZE_WORD: *(dbstart+1) |= 0x80; break;
            case SIZE_LONG: *(dbstart+1) |= 0xc0; break;
          }
          break;
        case S_HI:
          switch (sz) {
            case SIZE_WORD: *dbstart |= 0x02; break;
            case SIZE_LONG: *dbstart |= 0x04; break;
          }
          break;
        case S_CAS:
          switch (sz) {
            case SIZE_BYTE: *dbstart |= 0x02; break;
            case SIZE_WORD: *dbstart |= 0x04; break;
            case SIZE_LONG: *dbstart |= 0x06; break;
          }
          break;
        case S_MOVE:
          switch (sz) {
            case SIZE_BYTE: *dbstart |= 0x10; break;
            case SIZE_WORD: *dbstart |= 0x30; break;
            case SIZE_LONG: *dbstart |= 0x20; break;
          }
          break;
        case S_WL8:
          if (sz == SIZE_LONG)
            *dbstart |= 1;
          break;
        case S_LW7:
          if (sz == SIZE_WORD)
            *(dbstart+1) |= 0x80;
          break;
        case S_WL6:
          if (sz == SIZE_LONG)
            *(dbstart+1) |= 0x40;
          break;
        case S_MAC:
          if (sz == SIZE_LONG)
            *(dbstart+2) |= 8;
          break;
        case S_TRAP:
          switch (sz) {
            case SIZE_WORD: *(dbstart+1) |= 0x02; break;
            case SIZE_LONG: *(dbstart+1) |= 0x03; break;
          }
          break;
        case S_EXT:
          switch (sz) {
            case SIZE_WORD: *(dbstart+3) |= 0x40; break;
            case SIZE_LONG: *(dbstart+3) |= 0x80; break;
          }
          break;
        case S_FP:
          switch (sz) {
            case SIZE_SINGLE:   *(dbstart+2) |= 0x04; break;
            case SIZE_EXTENDED: *(dbstart+2) |= 0x08; break;
            case SIZE_PACKED:   *(dbstart+2) |= 0x0c; break;
            case SIZE_WORD:     *(dbstart+2) |= 0x10; break;
            case SIZE_DOUBLE:   *(dbstart+2) |= 0x14; break;
            case SIZE_BYTE:     *(dbstart+2) |= 0x18; break;
          }
          break;
        default:
          ierror(0);
          break;
      }

      /* check illegal addressing mode combinations for ColdFire MOVE */
      if ((cpu_type & mcf) && ip->code==OC_MOVE) {
        operand *src = ip->op[0];
        operand *dst = ip->op[1];

        if (src->mode >= MODE_An16Disp) {
          if ((src->mode==MODE_An16Disp ||
               (src->mode==MODE_Extended && src->reg==REG_PC16Disp))) {
            if (dst->mode==MODE_An8Format ||
                (dst->mode==MODE_Extended && dst->reg<=REG_AbsLong))
              cpu_error(41);  /* illegal combination of CF addressing modes */
          }
          else {
            if (dst->mode==MODE_An16Disp) {
              if (!(cpu_type & (mcfb|mcfc)) ||
                  (src->mode!=MODE_Extended || src->reg!=REG_Immediate) ||
                  (sz!=SIZE_BYTE && sz!=SIZE_WORD))
                /* ISA_B also allows "#x,d(An)" for byte and word size */
                cpu_error(41);  /* illegal combination of CF addr. modes */
            }
            else if (dst->mode==MODE_An8Format ||
                     (dst->mode==MODE_Extended && dst->reg<=REG_AbsLong))
              cpu_error(41);  /* illegal combination of CF addressing modes */
          }
        }
      }

      /* write operands / insert addressing mode into opcode */
      for (i=0; i<MAX_OPERANDS && ip->op[i]!=NULL; i++) {
        operand *op = ip->op[i];
        struct oper_insert *oii = &insert_info[mnemo->ext.place[i]];
        unsigned char *newd = d;

        switch (oii->mode) {
          case M_bfea:
            if (op->flags & FL_BFoffsetDyn) {
              op->bf_offset |= 32;
            }
            else if (op->bf_offset>31) {
              cpu_error(19);  /* illegal bitfield width/offset */
            }
            if (op->flags & FL_BFwidthDyn) {
              op->bf_width |= 32;
            }
            else {
              if (op->bf_width > 32)
                cpu_error(19);  /* illegal bitfield width/offset */
              op->bf_width &= 31;
            }
            *(dbstart+2) |= (op->bf_offset>>2) & 15;
            *(dbstart+3) |= ((op->bf_offset&3)<<6) | (op->bf_width&63);
            /* fall through */

          case M_ea:
            if (oii->flags & IIF_MASK)
              *(dbstart+3) |= op->bf_width ? (1<<oii->pos) : 0;
            if (oii->flags & IIF_NOMODE)
              *(dbstart+1) |= REGget(op->reg);
            else
              *(dbstart+1) |= ((op->mode & 7) << 3) | REGget(op->reg);
            /* fall through */

          case M_noea:
            newd = write_ea_ext(db,d,op,ext,sec,pc);
            /* fall through */

          case M_nop:
            break;

          case M_kfea:
            *(dbstart+1) |= ((op->mode & 7) << 3) | REGget(op->reg);
            *(dbstart+2) |= (op->flags & FL_BFoffsetDyn) ? 0x10 : 0;
            *(dbstart+3) |= op->bf_offset & 0x7f;
            newd = write_ea_ext(db,d,op,ext,sec,pc);
            break;

          case M_high_ea:
            *(dbstart) |= (REGget(op->reg) << 1) | ((op->mode & 4) >> 2);
            *(dbstart+1) |= (op->mode & 3) << 6;
            newd = write_ea_ext(db,d,op,ext,sec,pc);
            break;

          case M_func:
            if (oii->flags & IIF_ABSVAL) {
              if (op->base[0] != NULL) {
                cpu_error(24);  /* absolute value expected */
                break;
              }
            }
            (oii->insert)(dbstart,oii,op);
            break;

          case M_branch:
            newd = write_branch(db,d,op,ext,sec,pc,mnemo,
                                (oii->flags&IIF_BCC)?1:0);
            break;

          case M_val0:
            if (op->base[0] == NULL) {
              taddr v = op->extval[0];

              if (oii->flags & IIF_MASK) {
                if (v == 0)
                  v = 1 << oii->size;
                else if (v == (1<<oii->size))
                  v = 0;
              }
              else if (oii->flags & IIF_3Q) {
                if (v == 0)
                  v = -1;
                else if (v == -1)
                  v = 0;
              }
              if (oii->flags & IIF_REVERSE)
                v = reverse(v,oii->size);
              write_val(dbstart,oii->pos,oii->size,v,
                        (oii->flags&IIF_SIGNED)!=0);
            }
            else
              cpu_error(24);  /* absolute value expected */
            break;

          case M_reg:
            if (op->mode<MODE_Extended || op->mode==MODE_FPn) {
              taddr r = (taddr)op->reg;
  
              if (oii->size>3 && op->mode==MODE_An)
                r += REGAn;
              write_val(dbstart,oii->pos,oii->size,r,0);
            }
            else
              ierror(0);
            break;

          default:
            ierror(0);
            break;
        }
        pc += newd - d;
        d = newd;
      }
    }
  }
  while ((ip = ip->ext.un.copy.next) != NULL);

eval_done:
  /* restore flags and last_size of real ip to allow instruction_size() */
  realip->ext.un.real.flags = ipflags;
  realip->ext.un.real.last_size = lastsize;

  return db;
}


dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
/* Create a dblock (with relocs, if necessary) for size bits of data. */
{
  dblock *db = new_dblock();
  symbol *base = NULL;
  int btype,etype;
  taddr val = 0;
  thuge hval;
  tfloat fval;

  db->size = bitsize >> 3;
  db->data = mymalloc(db->size);

  etype = type_of_expr(op->value[0]);
  if (etype == FLT) {
    if (!eval_expr_float(op->value[0],&fval))
      general_error(60);  /* cannot evaluate floating point */
  }
  else if (bitsize > 32) {
    if (!eval_expr_huge(op->value[0],&hval))
      general_error(59);  /* cannot evaluate huge integer */
    etype = HUG;
  }
  else {
    if (!eval_expr(op->value[0],&val,sec,pc)) {
      btype = find_base(op->value[0],&base,sec,pc);
      if (btype == BASE_ILLEGAL)
        general_error(38);  /* illegal relocation */
    }
    etype = NUM;
  }

  switch (bitsize) {
    case 8:
      if (etype == NUM) {
        if (typechk && (val<-0x80 || val>0xff))
          cpu_error(39);  /* data out of range */
        db->data[0] = val & 0xff;
      }
      else if (etype == FLT)
        cpu_error(40);  /* data has illegal type */
      else
        ierror(0);
      break;

    case 16:
      if (etype == NUM) {
        if (typechk && (val<-0x8000 || val>0xffff))
          cpu_error(39);  /* data out of range */
        setval(1,db->data,2,val);
      }
      else if (etype == FLT)
        cpu_error(40);  /* data has illegal type */
      else
        ierror(0);
      break;

    case 32:
      if (etype == NUM)
        setval(1,db->data,4,val);
      else if (etype == FLT)
        conv2ieee32(1,db->data,fval);
      else
        ierror(0);
      break;

    case 64:
      if (etype == HUG) {
        if (typechk && !huge_chkrange(hval,64))
          cpu_error(39);  /* data out of range */
        huge_to_mem(1,db->data,8,hval);
      }
      else if (etype == FLT)
        conv2ieee64(1,db->data,fval);
      else
        ierror(0);
      break;

    case 96:
      if (etype == HUG) {
        if (typechk && !huge_chkrange(hval,96))
          cpu_error(39);  /* data out of range */
        huge_to_mem(1,db->data,12,hval);
      }
      else if (etype == FLT)
        conv2ieee80(1,db->data,fval);
      else
        ierror(0);
      break;

    default:
      cpu_error(38,bitsize); /* data objects with x size are not supported */
      break;
  }

  if (base) {
    /* relocation required */
    add_extnreloc(&db->relocs,base,val,btype==BASE_PCREL?REL_PC:REL_ABS,
                  0,bitsize,0);
  }

  return db;
}


int init_cpu()
{
  int i,j,code_tab_cnt;

  if (!gas) {
    /* remove gas mnemonics from the hash table */
    for (i=0; i<mnemonic_cnt; i++) {
      if (mnemonics[i].ext.available & mgas) {
        rem_hashentry(mnemohash,mnemonics[i].name,0);
        while (i+1<mnemonic_cnt &&
               !strcmp(mnemonics[i].name,mnemonics[i+1].name))
          i++;
      }
    }
  }

  /* remember all mnemonic locations which we need */
  code_tab_cnt = sizeof(code_tab) / sizeof(code_tab[0]);
  for (i=0,j=0; i<mnemonic_cnt && j<code_tab_cnt; i++)
    if (!strcmp(mnemonics[i].name,code_tab[j].name))
      if ((code_tab[j].optype[0] == 0 ||
           mnemonics[i].operand_type[0] == (int)code_tab[j].optype[0]) &&
          (code_tab[j].optype[1] == 0 ||
           mnemonics[i].operand_type[1] == (int)code_tab[j].optype[1]))
        *code_tab[j++].var = i;
  if (j < code_tab_cnt)
    ierror(0);

  /* flag all mnemonics with an unambiguous size extension */
  for (i=0; i<mnemonic_cnt; i++) {
    if (countbits((taddr)mnemonics[i].ext.size & SIZE_MASK) == 1)
      mnemonics[i].ext.size |= SIZE_UNAMBIG;
  }

  /* predefine some register symbols */
  new_regsym(0,0,elfregs?"%sp":"sp",RSTYPE_An,0,7);
  new_regsym(0,0,elfregs?"%fp":"fp",RSTYPE_An,0,6);

  /* reset baseregs */
  for (i=0; i<7; i++)
    baseexp[i] = NULL;  /* disable basereg for A0-A7 */

  /* predefine cpu symbols */
  if (phxass_compat) {
    set_optc_symbol();
    set_internal_abs(cpu_name,phxass_cpu_num(cpu_type));
    set_internal_abs(mmu_name,(cpu_type & mmmu)!=0);
    set_internal_abs(fpu_name,(cpu_type & mfloat)?fpu_id:0);
  }
  if (devpac_compat) {
    taddr f = 99;

    /* __G2 contains host, version and cpu information */
    set_g2_symbol();

    /* __LK is 0 for TOS executables, 1 for GST-, 2 for DRI-linkable,
       3 for Amiga-linkable, 4 for Amiga executable.
       Set it to 99 when creating an object file of unknown format. */
    if (!strcmp(output_format,"tos"))
      f = 0;
    else if (!strcmp(output_format,"hunkexe"))
      f = 4;
    else if (!strcmp(output_format,"hunk"))
      f = 3;
    set_internal_abs(lk_name,f);
  }

  /* for BAsm compatibility */
  movemregs = internal_abs(movemregs_name);
  movemsize = internal_abs(movemsize_name);
  movembytes = internal_abs(movembytes_name);
  movembytes->expr = make_expr(MUL,new_sym_expr(movemsize),
                       make_expr(CNTONES,new_sym_expr(movemregs),NULL));

  /* __VASM */
  set_internal_abs(vasmsym_name,cpu_type&CPUMASK);

  return 1;
}


static void set_cpu_type(uint32_t type,int addatom)
{
  if (type & (m68k|cpu32|mcf_all|apollo)) {
    cpu_type = (cpu_type & ~(m68k|cpu32|mcf_all|apollo)) | type;
    if (gas) {
      if (!(type & (m68020|m68030|cpu32)))
        cpu_type &= ~(m68881|m68882);  /* no 88x FPU when not 020 or 030 */
      if (!no_fpu && (type & (m68020|m68030|cpu32)))
        cpu_type |= m68881|m68882;  /* gas compatibility: always have FPU */
    }
  }
  else if (type & (m68881|m68882))
    cpu_type = (cpu_type & ~(m68881|m68882)) | (no_fpu ? 0 : type);
  else if (type == m68851)
    cpu_type |= m68851;

  if (addatom)
    add_cpu_opt(0,OCMD_CPU,cpu_type);

  if (cpu_type & (m68020up | mcf))
    m68k_mid = 2;  /* need 68020+ */
}


static uint32_t get_cpu_type(char **str)
{
  char *s = *str;
  uint32_t type = 0;
  int len,i;

  while (ISIDCHAR(*s))
    s++;
  len = s - *str;

  for (i=0; i<model_cnt; i++) {
    if (strlen(models[i].name)==len && !strnicmp(*str,models[i].name,len)) {
      type = models[i].type;
      break;
    }
  }

  *str = s;
  return type;
}


static void clear_all_opts(void)
{
  opt_movem = opt_pea = opt_clr = opt_st = opt_lsl = opt_mul = opt_div = 0;
  opt_fconst = opt_brajmp = opt_pc = opt_bra = opt_allbra = opt_jbra = 0;
  opt_disp = opt_abs = opt_moveq = opt_nmovq = opt_quick = opt_branop = 0;
  opt_bdisp = opt_odisp = opt_lea = opt_lquick = opt_immaddr = 0;
  opt_gen = opt_speed = opt_size = 0;
}


int cpu_args(char *arg)
{
  char *p = arg;
  int i;

  if (!strcmp(p,"-phxass")) {
    phxass_compat = 1;
    opt_allbra = opt_brajmp = opt_sd = 1;
    opt_fconst = 0;
    ign_unambig_ext = ign_unsized_ext = 1;
    return 0;  /* leave option visible for syntax modules */
  }

  if (!strcmp(p,"-devpac")) {
    /* set all options to Devpac-compatible defaults */
    devpac_compat = 1;
#ifdef OUTTOS
    tos_hisoft_dri = 0;  /* no extended symbol names unless OPT X+ is given */
#endif
    clear_all_opts();
    no_symbols = 1;
    warn_opts = 2;
    ign_unsized_ext = 1;
    unsigned_shift = 1;
    no_dpc = 1;
    return 0;  /* leave option visible for syntax modules */
  }

  if (!strcmp(p,"-kick1hunks")) {
    kick1hunks = 1;
    return  0;  /* leave option visible for syntax modules */
  }

  if (!strncmp(p,"-m",2)) {
    uint32_t cpu;

    p += 2;
    if (!strcmp(p,"no-68881"))
      goto nofpu;
    if (!strncmp(p,"cf",2))
      p += 2;  /* allow -mcf for ColdFire models */
    cpu = get_cpu_type(&p);
    if (!cpu)
      return 0;
    set_cpu_type(cpu,0);
  }
  else if (!strncmp(p,"-sdreg=",7)) {
    i = atoi(p+7);
    if (i>=0 && i<=6)
      sdreg = i;
    else
      cpu_error(58);  /* not a valid small data register */
  }
  else if (!strcmp(p,"-no-opt")) {
    clear_all_opts();
    no_opt = 1;
  }
  else if (!strcmp(p,"-no-fpu")) {
nofpu:
    no_fpu = 1;
    cpu_type &= ~(m68881|m68882);
  }
  else if (!strcmp(p,"-gas")) {
    gas = 1;
    commentchar = '|';
    set_cpu_type(m68020,0);  /* gas compatibility defaults to 68020/68881 */
    opt_jbra = !no_opt;
  }
  else if (!strcmp(p,"-sgs"))
    sgs = 1;
  else if (!strcmp(p,"-extsd"))
    extsd = 1;
  else if (!strcmp(p,"-rangewarnings"))
    modify_cpu_err(WARNING,25,29,32,36,0);
  else if (!strcmp(p,"-conv-brackets"))
    convert_brackets = 1;
  else if (!strcmp(p,"-regsymredef"))
    regsymredef = 1;
  else if (!strcmp(p,"-elfregs"))
    elfregs = 1;
  else if (!strcmp(p,"-guess-ext"))
    ign_unambig_ext = ign_unsized_ext = 1;
  else if (!strcmp(p,"-nodpc"))
    no_dpc = 1;
  else if (!strcmp(p,"-showcrit"))
    warn_opts = 1;
  else if (!strcmp(p,"-showopt"))
    warn_opts = 2;
  else if (!strcmp(p,"-sc"))
    opt_sc = !no_opt;
  else if (!strcmp(p,"-sd"))
    opt_sd = !no_opt;
  else if (!strcmp(p,"-opt-movem"))
    opt_movem = !no_opt;
  else if (!strcmp(p,"-opt-pea"))
    opt_pea = !no_opt;
  else if (!strcmp(p,"-opt-clr"))
    opt_clr = !no_opt;
  else if (!strcmp(p,"-opt-st"))
    opt_st = !no_opt;
  else if (!strcmp(p,"-opt-lsl"))
    opt_lsl = !no_opt;
  else if (!strcmp(p,"-opt-mul"))
    opt_mul = !no_opt;
  else if (!strcmp(p,"-opt-div"))
    opt_div = !no_opt;
  else if (!strcmp(p,"-opt-fconst"))
    opt_fconst = !no_opt;
  else if (!strcmp(p,"-opt-nmoveq"))
    opt_nmovq = !no_opt;
  else if (!strcmp(p,"-opt-brajmp"))
    opt_brajmp = !no_opt;
  else if (!strcmp(p,"-opt-allbra"))
    opt_bra = opt_allbra = !no_opt;
  else if (!strcmp(p,"-opt-jbra"))
    opt_jbra = !no_opt;
  else if (!strcmp(p,"-opt-speed"))
    opt_speed = !no_opt;
  else if (!strcmp(p,"-opt-size"))
    opt_size = !no_opt;
  else
    return 0;

  return 1;
}


char *parse_instruction(char *s,int *inst_len,char **ext,int *ext_len,
                        int *ext_cnt)
/* parse instruction and save extension locations */
{
  char *inst = s;
  int cnt = *ext_cnt;

  if (*s == '.')  /* allow dot as first char */
    s++;
  while (*s && *s!='.' && !isspace((unsigned char)*s))
    s++;
  *inst_len = s - inst;

  while (*s=='.' && cnt<MAX_QUALIFIERS) {
    s++;
    ext[cnt] = s;
    while (*s && *s!='.' && !isspace((unsigned char)*s))
      s++;
    ext_len[cnt] = s - ext[cnt];
    if (ext_len[cnt] <= 0)
      cpu_error(34);  /* illegal opcode extension */
    else
      cnt++;
  }
  *ext_cnt = cnt;

  if (cnt > 0)
    current_ext = tolower((unsigned char)ext[0][0]);
  else
    current_ext = '\0';
  return s;
}


int set_default_qualifiers(char **q,int *q_len)
/* fill in pointers to default qualifiers, return number of qualifiers */
{
  q[0] = (cpu_type & mcf) ? l_str : w_str;
  q_len[0] = 1;
  return 1;
}


static char validchar(char *s)
{
  return ISEOL(s) ? 0 : *s;
}


static void phxass_optc(uint16_t optc)
/* set optimizations according to PhxAss OPTC flags */
{
  if (optc == 0) {
    no_opt = 1;  /* no optimizations */
    return;
  }
  if (optc & 0x001) { /* N */
    opt_gen = opt_disp = opt_abs = opt_moveq = opt_bdisp = opt_odisp = 1;
    opt_lea = opt_immaddr = 1;
  }
  if (optc & 0x002) /* R */
    opt_pc = 1;
  if (optc & 0x004) /* Q */
    opt_quick = opt_lquick = 1;
  if (optc & 0x008) /* B */
    opt_bra = opt_brajmp = 1;
  if (optc & 0x010) /* L */
    opt_lsl = 1;
  if (optc & 0x020) /* P */
    opt_pea = 1;
  if (optc & 0x040) /* S */
    opt_clr = opt_st = opt_fconst = opt_disp = opt_bdisp = opt_odisp = 1;
  if (optc & 0x080) /* M */
    opt_gen = 1;
  if (optc & 0x100) /* T */
    ;  /* ignored */
  if (optc & 0x200) /* D */
    opt_movem = 1;
}


static void phxass_option(char opt)
/* parse a phxass-style option */
{
  switch (toupper((unsigned char)opt)) {
    case '0':
      no_opt = 1;  /* no optimizations */
      break;
    case '3':  /* enable all */
    case '!':
      opt_lsl = opt_pea = opt_clr = opt_st = opt_fconst = 1;
      opt_disp = opt_bdisp = opt_odisp = opt_movem = 1;
      /* fall through */
    case '1':  /* enable standard */
    case '2':
    case '*':
      opt_pc = opt_quick = opt_lquick = opt_bra = opt_brajmp = 1;
      /* fall through */
    case 'N':  /* normal */
      opt_gen = opt_disp = opt_abs = opt_moveq = opt_bdisp = opt_odisp = 1;
      opt_lea = opt_immaddr = 1;
      break;
    case 'R':  /* relative */
      opt_pc = 1;
      break;
    case 'Q':  /* quick */
      opt_quick = opt_lquick = 1;
      break;
    case 'B':  /* branches */
      opt_bra = opt_brajmp = opt_allbra = 1;
      break;
    case 'L':  /* logical shifts */
      opt_lsl = 1;
      break;
    case 'P':  /* pea */
      opt_pea = 1;
      break;
    case 'S':  /* special */
      opt_clr = opt_st = opt_fconst = opt_disp = opt_bdisp = opt_odisp = 1;
      break;
    case 'D':  /* movem single */
      opt_movem = 1;
      /* fall through */
    case 'M':  /* delete movem */
      opt_gen = 1;
      break;
  }
}


static char *devpac_option(char *s)
/* parse a devpac-style option (default) */
{
  static int opt_map[] = {  /* maps OPT O<n> to a vasm option */
    OCMD_OPTBRA,OCMD_OPTDISP,OCMD_OPTABS,OCMD_OPTMOVEQ,OCMD_OPTQUICK,
    OCMD_NOP,OCMD_OPTBRANOP,OCMD_OPTBDISP,OCMD_OPTODISP,OCMD_OPTLEA,
    OCMD_OPTLQUICK,OCMD_OPTIMMADDR
  };
  char opt,ext,c;
  int flag = 1;
  int num;

  if (!strnicmp(s,"no",2)) {  /* no... prefix for option */
    flag = 0;
    s += 2;
  }

  if (!strnicmp(s,"autopc",6)) {
    add_cpu_opt(0,OCMD_OPTPC,flag);
    return s+6;
  }
  else if (!strnicmp(s,"case",4)) {
    nocase = !flag;
    return s+4;
  }
  else if (!strnicmp(s,"chkpc",5)) {
    add_cpu_opt(0,OCMD_CHKPIC,flag);
    return s+5;
  }
  else if (!strnicmp(s,"debug",5)) {
    no_symbols = !flag;
    return s+5;
  }
  else if (!strnicmp(s,"symtab",6)) {
    listnosyms = !flag;
    return s+6;
  }
  else if (!strnicmp(s,"type",4)) {
    add_cpu_opt(0,OCMD_CHKTYPE,flag);
    return s+4;
  }
  else if (!strnicmp(s,"warn",4)) {
    no_warn = !flag;  /* must also be recognized during parsing */
    add_cpu_opt(0,OCMD_NOWARN,!flag);
    return s+4;
  }
  else if (!strnicmp(s,"xdebug",6)) {
    if (flag)
      no_symbols = 0;
#ifdef OUTTOS
    tos_hisoft_dri = flag;  /* extended symbol names for Atari */
#endif
#ifdef OUTHUNK
    hunk_onlyglobal = flag; /* only xdef-symbols in objects for Amiga */
#endif
    return s+6;
  }
  else if (!flag)
    goto devpac_err;

  if (!strnicmp(s,"p=",2)) {
    uint32_t cpu;

    s++;
    do {
      s++;
      if ((cpu = get_cpu_type(&s)) != 0)
        set_cpu_type(cpu,1);
      else
        cpu_error(43);  /* unknown cpu type */
    }
    while (*s == '/');  /* another CPU/FPU/MMU specification? */
    return s;
  }

  if ((opt = tolower((unsigned char)validchar(s))) != 0) {
    ext = validchar(s+1);
    if (isdigit((unsigned char)ext))
      num = atoi(s+1);
    else
      num = -1;

    do
      c = validchar(++s);
    while (c!=0 && c!=',' && c!='+' && c!='-');

    if (c==0 || c==',') {
      switch (opt) {
        case 'l':  /* Ln : Atari only */
          if (num >= 0)
            exec_out = num == 0;
          break;
        default:
          cpu_error(31,toupper((unsigned char)opt),c);  /* unkn. opt ignored */
          break;
      }
      return s;
    }

    else if (c != 0) {
      flag = c=='+';
      if (ext == c)
        ext = 0;

      switch (opt) {
        case 'a':
          add_cpu_opt(0,OCMD_OPTPC,flag);
          break;
        case 'c':
          nocase = !flag;
          break;
        case 'd':
          no_symbols = !flag;
          break;
        case 'l':  /* L+/- : Amiga only */
          exec_out = !flag;
          break;
        case 'm':
          /* macro expansion in listing file */
          break;
        case 'o':
          if (isdigit((unsigned char)ext) && num>=1 && num<=12) {
            add_cpu_opt(0,opt_map[num-1],flag);
          }
          else {
            switch (tolower((unsigned char)ext)) {
              case '\0':
                if (!flag) {
                  /* clear all optimization flags, set no_opt */
                  clear_all_opts();
                  add_cpu_opt(0,OCMD_NOOPT,1);
                }
                else {
                  /* enable all safe optimizations, as in vasm-default */
                  add_cpu_opt(0,OCMD_OPTBRA,1);
                  add_cpu_opt(0,OCMD_OPTDISP,1);
                  add_cpu_opt(0,OCMD_OPTABS,1);
                  add_cpu_opt(0,OCMD_OPTMOVEQ,1);
                  add_cpu_opt(0,OCMD_OPTQUICK,1);
                  add_cpu_opt(0,OCMD_OPTBRANOP,1);
                  add_cpu_opt(0,OCMD_OPTBDISP,1);
                  add_cpu_opt(0,OCMD_OPTODISP,1);
                  add_cpu_opt(0,OCMD_OPTLEA,1);
                  add_cpu_opt(0,OCMD_OPTLQUICK,1);
                  add_cpu_opt(0,OCMD_OPTIMMADDR,1);
                  if (!devpac_compat) {
                    /* enable safe vasm-specific optimizations */
                    add_cpu_opt(0,OCMD_OPTGEN,1);
                    add_cpu_opt(0,OCMD_OPTPC,1);
                    add_cpu_opt(0,OCMD_OPTFCONST,1);
                  }
                }
                break;
              case 'b': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTJBRA,flag);
                break;
              case 'c': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTCLR,flag);
                break;
              case 'd': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTDIV,flag);
                break;
              case 'f': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTFCONST,flag);
                break;
              case 'g': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTGEN,flag);
                break;
              case 'j': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTBRAJMP,flag);
                break;
              case 'l': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTLSL,flag);
                break;
              case 'm': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTMOVEM,flag);
                break;
              case 'n': /* vasm-specific */
                add_cpu_opt(0,OCMD_SMALLDATA,flag);
                break;
              case 'p': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTPEA,flag);
                break;
              case 'q': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTNMOVQ,flag);
                break;
              case 's': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTSPEED,flag);
                break;
              case 't': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTST,flag);
                break;
              case 'w':
                add_cpu_opt(0,OCMD_OPTWARN,flag?2:0);
                break;
              case 'x': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTMUL,flag);
                break;
              case 'z': /* vasm-specific */
                add_cpu_opt(0,OCMD_OPTSIZE,flag);
                break;
              default:
                cpu_error(31,opt,ext);  /* unknown option ignored */
                break;
            }
          }
          break;
        case 'p':
          add_cpu_opt(0,OCMD_CHKPIC,flag);
          break;
        case 's':
          listnosyms = !flag;
          break;
        case 't':
          add_cpu_opt(0,OCMD_CHKTYPE,flag);
          break;
        case 'w':
          no_warn = !flag;  /* must also be recognized during parsing */
          add_cpu_opt(0,OCMD_NOWARN,!flag);
          break;
        case 'x':
          if (flag)
            no_symbols = 0;
#ifdef OUTTOS
          tos_hisoft_dri = flag;  /* extended symbol names for Atari */
#endif
#ifdef OUTHUNK
          hunk_onlyglobal = flag; /* only xdef-symbols in objects for Amiga */
#endif
          break;
        default:
          cpu_error(31,toupper((unsigned char)opt),c);  /* unkn. opt ignored */
          break;
      }
      return ++s;
    }
  }

  devpac_err:
  cpu_error(23);  /* option expected */
  return NULL;
}


char *parse_cpu_special(char *start)
/* parse cpu-specific directives; return pointer to end of
   cpu-specific text */
{
  char *name=start,*s=start;
  uint32_t cpu;

  if (ISIDSTART(*s)) {
    s++;
    while (ISIDCHAR(*s))
      s++;
    if (dotdirs && *name=='.')
      name++;

    if ((s-name==4 && !strnicmp(name,"near",4)) ||
        (s-name==5 && !strnicmp(name,"sdreg",5))) {
      /* NEAR <An> */
      signed char r;

      s = skip(s);
      r = getreg(&s,0);
      if (r >= 0) {
        if (r>=(REGAn+0) && r<=(REGAn+6))  /* a0-a6 */
          sdreg = REGget(r);
        else
          cpu_error(58);  /* not a valid small data register */
      }
      else if (!dotdirs && !strnicmp(s,"code",4)) {
        add_cpu_opt(0,OCMD_SMALLCODE,1);
        return skip_line(s);
      }
      else
        sdreg = last_sdreg;
      add_cpu_opt(0,OCMD_SDREG,sdreg);
      /* skip rest of line to be compatible to other assemblers */
      return skip_line(s);
    }

    else if (s-name==3 && !strnicmp(name,"far",3)) {
      /* FAR */
      if (sdreg >= 0) {
        last_sdreg = sdreg;
        sdreg = -1;
        add_cpu_opt(0,OCMD_SDREG,sdreg);
      }
      add_cpu_opt(0,OCMD_SMALLCODE,0);
      eol(s);
      return skip_line(s);
    }

    else if (s-name==8 && !strnicmp(name,"initnear",8)) {
      /* INITNEAR */
      if (sdreg >= 0) {
        dblock *db = new_dblock();

        db->size = 6;
        db->data = mymalloc(6);
        db->data[0] = 0x41 | (sdreg << 1);  /* LEA _LinkerDB,An */
        db->data[1] = 0xf9;
        memset(&db->data[2],0,4);
        add_extnreloc(&db->relocs,new_import("_LinkerDB"),0,REL_ABS,0,32,2);
        add_atom(0,new_data_atom(db,2));
      }
      else
        cpu_error(59);  /* small data mode is not enabled */
      return skip_line(s);
    }

    else if (s-name==7 && !strnicmp(name,"basereg",7)) {
      /* BASEREG <expression>,<An> */
      expr *exp;
      signed char reg;

      s = skip(s);
      if ((exp = parse_expr(&s)) != NULL) {
        s = skip(s);
        if (*s == ',') {
          s = skip(s+1);
          reg = getreg(&s,0);
          if (reg>=(REGAn+0) && reg<=(REGAn+6)) {
            if (baseexp[REGget(reg)]!=NULL && !phxass_compat) {
              cpu_error(55,REGget(reg));  /* basereg already in use */
            }
            else {
              baseexp[REGget(reg)] = exp;
              eol(s);
            }
          }
          else
            cpu_error(4);  /* address register required */
        }
        else
          cpu_error(15,',');  /* , expected */
      }
      return skip_line(s);
    }

    else if (s-name==4 && !strnicmp(name,"endb",4)) {
      signed char reg;

      s = skip(s);
      reg = getreg(&s,0);
      if (reg>=(REGAn+0) && reg<=(REGAn+6)) {
        if (baseexp[REGget(reg)] != NULL) {
          baseexp[REGget(reg)] = NULL;
          eol(s);
        }
        else
          cpu_error(56,REGget(reg));  /* basereg is already free */
      }
      else if (phxass_compat) {
        /* ignore register specification as long as it is unambigious */
        for (reg=0; reg<=6; reg++) {
          if (baseexp[reg] != NULL) {
            baseexp[reg] = NULL;
            break;
          }
        }
        for (; reg<=6; reg++) {
          if (baseexp[reg] != NULL)  /* more than one register in use? */
            cpu_error(4);            /* address register required */
        }
      }
      else
        cpu_error(4);  /* address register required */
      return skip_line(s);
    }

    else if (s-name==7 && !strnicmp(name,"machine",7)) {
      /* MACHINE <cpu-type> */
      int acflag = 0;

      s = skip(s);
      if (!strnicmp(s,"mc",2)) {
        s += 2;  /* Devpac-compatible MACHINE mc680x0 */
        acflag = -1;
      }
      else if (!strnicmp(s,"ac",2)) {
        s += 2;  /* Apollo Core ac680x0 */
        acflag = 1;
      }

      cpu = get_cpu_type(&s);

      if (cpu!=0 &&
          ((!(cpu&apollo) && acflag<=0) || ((cpu&apollo) && acflag>=0))) {
        set_cpu_type(cpu,1);
        eol(s);
      }
      else
        cpu_error(43);  /* unknown cpu type */
      return skip_line(s);
    }

    else if ((s-name)>=5 && (s-name)<=8 && !strnicmp(name,"mcf",3)) {
      /* MCF5xxx ColdFire */
      s = name + 3;
      if ((cpu = get_cpu_type(&s)) != 0) {
        set_cpu_type(cpu,1);
        eol(s);
        return skip_line(s);
      }
      return start;
    }

    else if (s-name==7 && !strnicmp(name,"mc",2)) {
      /* MC680x0 */
      s = name + 2;
      cpu = get_cpu_type(&s);
      if (cpu!=0 && !(cpu&apollo)) {
        set_cpu_type(cpu,1);
        eol(s);
        return skip_line(s);
      }
      return start;
    }

    else if (s-name==7 && !strnicmp(name,"ac",2)) {
      /* Apollo Core AC680x0 */
      s = name + 2;
      cpu = get_cpu_type(&s);
      if (cpu & apollo) {
        set_cpu_type(cpu,1);
        eol(s);
        return skip_line(s);
      }
      return start;
    }

    else if (s-name==5 && !strnicmp(name,"cpu32",5)) {
      /* CPU32 */
      set_cpu_type(cpu32,1);
      eol(s);
      return skip_line(s);
    }

    else if (s-name==3 && !strnicmp(name,"fpu",3)) {
      /* FPU [<cpID>] */
      taddr id = 1;

      s = skip(s);
      if (validchar(s))
        id = parse_constexpr(&s);
      if (id>0 && id<8 && !no_fpu) {
        fpu_id = (unsigned char)id;
        add_cpu_opt(0,OCMD_FPU,fpu_id);
        cpu_type |= m68881|m68882;
      }
      else
        cpu_type &= ~(m68881|m68882);
      add_cpu_opt(0,OCMD_CPU,cpu_type);
      eol(s);
      return skip_line(s);
    }

    else if (s-name==3 && !strnicmp(name,"opt",3)) {
      if (phxass_compat) {
        /* OPT [single-letter-options] - PhxAss options */
        s = skip(s);
        clear_all_opts();  /* first disable all */
        while (validchar(s))
          phxass_option(*s++);
        cpu_opts_optinit(NULL);  /* now create new opt atoms */
      }
      else {
        /* OPT <option>[,<option>...] - Devpac options */
        char *s2 = s;

        do {
          if (!(s2 = devpac_option(skip(s2))))
            break;
          s2 = skip(s2);
        }
        while (*s2++ == ',');  /* another option? */
      }
      return skip_line(s);
    }

    else if (phxass_compat && s-name==4 && !strnicmp(name,"optc",4)) {
      /* OPTC <expression> - set PhxAss optimization flags */
      taddr optc = 0;

      s = skip(s);
      clear_all_opts();  /* first disable all */
      if (validchar(s))
        optc = parse_constexpr(&s);
      phxass_optc((uint16_t)optc);
      cpu_opts_optinit(NULL);  /* now create new opt atoms */
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

    if ((s-dir==4 && !strnicmp(dir,"equr",4)) ||
        (s-dir==5 && !strnicmp(dir,"fequr",5))) {
      /* label EQUR Rn */
      signed char r;

      s = skip(s);
      if ((r = getreg(&s,0)) >= 0)
        new_regsym(regsymredef,0,labname,
                   REGisAn(r)?RSTYPE_An:RSTYPE_Dn,0,REGget(r));
      else if ((r = getfreg(&s)) >= 0)
        new_regsym(regsymredef,0,labname,RSTYPE_FPn,0,r);
      else if ((r = getbreg(&s)) >= 0)
        new_regsym(regsymredef,0,labname,RSTYPE_Bn,0,r);
      else
        cpu_error(44);  /* register expected */
      eol(s);
      *start = skip_line(s);
      return 1;
    }

    else if ((s-dir==3 && !strnicmp(dir,"reg",3)) ||
             (s-dir==5 && !strnicmp(dir,"equrl",5))) {
      /* label REG reglist */
      symbol *sym;

      s = skip(s);
      sym = new_equate(labname,number_expr((taddr)scan_Rnlist(&s)));
      sym->flags |= REGLIST;
      eol(s);
      *start = skip_line(s);
      return 1;
    }

    else if ((s-dir==4 && !strnicmp(dir,"freg",4)) ||
             (s-dir==6 && !strnicmp(dir,"fequrl",6))) {
      /* label FREG reglist */
      symbol *sym;

      s = skip(s);
      sym = new_equate(labname,number_expr((taddr)scan_FPnlist(&s)));
      sym->flags |= REGLIST;
      eol(s);
      *start = skip_line(s);
      return 1;
    }
  }

  return 0;
}
