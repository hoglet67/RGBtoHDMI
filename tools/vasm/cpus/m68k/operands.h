struct addrmode addrmodes[] = {
  MODE_Dn,-1,                   /* 0 */
  MODE_An,-1,
  MODE_AnIndir,-1,
  MODE_AnPostInc,-1,
  MODE_AnPreDec,-1,
  MODE_An16Disp,-1,
  MODE_An8Format,-1,            /* 6 */
  MODE_Extended,REG_AbsShort,   /* 7 */
  MODE_Extended,REG_AbsLong,
  MODE_Extended,REG_PC16Disp,
  MODE_Extended,REG_PC8Format,
  MODE_Extended,REG_Immediate,
  MODE_Extended,REG_RnList,
  MODE_Extended,REG_FPnList,    /* 13 */
  MODE_FPn,-1,
  MODE_SpecReg,-1               /* 15 */
};


/* specregs.h */
enum {
  REG_CCR=0,REG_SR,REG_NC,REG_DC,REG_IC,REG_BC,

  REG_ACC,REG_ACC0,REG_ACC1,REG_ACC2,REG_ACC3,REG_ACCX01,REG_ACCX23,
  REG_MACSR,REG_MASK,REG_SFLEFT,REG_SFRIGHT,

  REG_VX00,REG_VX01,REG_VX02,REG_VX03,REG_VX04,REG_VX05,REG_VX06,REG_VX07,
  REG_VX08,REG_VX09,REG_VX10,REG_VX11,REG_VX12,REG_VX13,REG_VX14,REG_VX15,
  REG_VX16,REG_VX17,REG_VX18,REG_VX19,REG_VX20,REG_VX21,REG_VX22,REG_VX23,

  REG_TC,REG_SRP,REG_CRP,REG_DRP,REG_CAL,REG_VAL,REG_SCC,REG_AC,
  REG_BAC0,REG_BAC1,REG_BAC2,REG_BAC3,REG_BAC4,REG_BAC5,REG_BAC6,REG_BAC7,
  REG_BAD0,REG_BAD1,REG_BAD2,REG_BAD3,REG_BAD4,REG_BAD5,REG_BAD6,REG_BAD7,
  REG_MMUSR,REG_PSR,REG_PCSR,REG_TT0,REG_TT1,
#if 0
  REG_ACUSR,REG_AC0,REG_AC1,
#endif

  /* MOVEC control registers _CTRL */
  REG_SFC,REG_DFC,REG_CACR,REG_ASID,REG_TC_,
  REG_ITT0,REG_ITT1,REG_DTT0,REG_DTT1,
  REG_IACR0,REG_IACR1,REG_DACR0,REG_DACR1,
  REG_ACR0,REG_ACR1,REG_ACR2,REG_ACR3,
  REG_BUSCR,REG_MMUBAR,
  REG_STR,REG_STC,REG_STH,REG_STB,REG_MWR,
  REG_USP,REG_VBR,REG_CAAR,REG_MSP,
  REG_ISP,REG_MMUSR_,REG_URP,REG_SRP_,REG_PCR,
  REG_CCC,REG_IEP1,REG_IEP2,REG_BPC,REG_BPW,REG_DCH,REG_DCM,
  REG_ROMBAR,REG_ROMBAR0,REG_ROMBAR1,
  REG_RAMBAR,REG_RAMBAR0,REG_RAMBAR1,
  REG_MPCR,REG_EDRAMBAR,REG_SECMBAR,REG_MBAR,
  REG_PCR1U0,REG_PCR1L0,REG_PCR2U0,REG_PCR2L0,REG_PCR3U0,REG_PCR3L0,
  REG_PCR1U1,REG_PCR1L1,REG_PCR2U1,REG_PCR2L1,REG_PCR3U1,REG_PCR3L1,
};


#define _(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) \
 ((a)|((b)<<1)|((c)<<2)|((d)<<3)|((e)<<4)|((f)<<5)|((g)<<6)|((h)<<7)| \
  ((i)<<8)|((j)<<9)|((k)<<10)|((l)<<11)|((m)<<12)|((n)<<13)| \
  ((o)<<14)|((p)<<15))

enum {
  OP_D8=1,OP_D16,OP_D32,OP_D64,OP_F32,OP_F64,OP_F96,
  D_,A_,B_,AI,IB,R_,RM,DD,CS,VDR2,VDR4,PA,AP,DP,
  F_,FF,FR,FPIAR,IM,QI,IR,BR,AB,VA,M6,RL,FL,FS,
  AY,AM,MA,MI,FA,CF,MAQ,CFAM,CM,AL,DA,DN,CFDA,CT,AC,AD,CFAD,
  BD,BS,AK,MS,MR,CFMM,CFMN,ND,NI,NJ,NK,BY,BI,BJ,OF_,
  _CCR,_SR,_USP,_CACHES,_ACC,_MACSR,_MASK,_CTRL,_ACCX,_AEXT,
  _VAL,_FC,_RP_030,_RP_851,_TC,_AC,_M1_B,_BAC,_BAD,_PSR,_PCSR,
  _TT,SH,VX,VXR2,VXR4,OVX
};

struct optype optypes[] = {
  0,0,0,0,

/* OP_D8     8-bit data */
  _(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),OTF_DATA,0,0,

/* OP_D16    16-bit data */
  _(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),OTF_DATA,0,0,

/* OP_D32    32-bit data */
  _(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),OTF_DATA,0,0,

/* OP_D64    64-bit data */
  _(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),OTF_DATA|OTF_QUADIMM,0,0,

/* OP_F32    32-bit data */
  _(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),OTF_DATA|OTF_FLTIMM,0,0,

/* OP_F64    64-bit data */
  _(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),OTF_DATA|OTF_FLTIMM,0,0,

/* OP_F96    96-bit data */
  _(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),OTF_DATA|OTF_FLTIMM,0,0,

/* D_        data register */
  _(1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),0,0,0,

/* A_        address register */
  _(0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0),0,0,0,

/* B_        (Apollo) base register */
  _(0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0),FL_BnReg,0,0,

/* AI        address register indirect */
  _(0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0),0,0,0,

/* IB        (Apollo) base register indirect */
  _(0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0),FL_BnReg,0,0,

/* R_        any data or address register */
  _(1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0),0,0,0,

/* RM        any data or address register with optional U/L extension (MAC) */
  _(1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0),FL_MAC,0,0,

/* DD        double data register */
  _(1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),FL_DoubleReg,0,0,

/* CS        any double data or address register indirect (cas2) */
  _(0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0),FL_DoubleReg,0,0,

/* VDR2      (Apollo) Dn:Dn+1 */
  _(1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),OTF_VXRNG2,0,0,

/* VDR4      (Apollo) Dn-Dn+3 */
  _(1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),OTF_VXRNG4,0,0,

/* PA        address register indirect with predecrement */
  _(0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0),0,0,0,

/* AP        address register indirect with postincrement */
  _(0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0),0,0,0,

/* DP        address register indirect with displacement (movep) */
  _(0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0),0,0,0,

/* F_        FPU register FPn */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0),0,0,0,

/* FF        double FPU register */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0),FL_DoubleReg,0,0,

/* FR        FPU special register FPCR/FPSR/FPIAR */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0),FL_FPSpec,0,0,

/* FPIAR     FPU special register FPIAR */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0),OTF_CHKREG|FL_FPSpec,1,1,

/* IM        immediate data */
  _(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),0,0,0,

/* QI        quick immediate data (moveq, addq, subq) */
  _(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),OTF_NOSIZE,0,0,

/* IR        immediate register list value (movem) */
  _(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),OTF_NOSIZE,0,0,

/* BR        branch destination */
  _(0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0),OTF_BRANCH,0,0,

/* AB        absolute long destination */
  _(0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0),0,0,0,

/* VA        absolute value */
  _(0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0),OTF_NOSIZE,0,0,

/* M6        mode 6 - addr. reg. indirect with index and displacement */
  _(0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0),0,0,0,

/* RL        An/Dn register list */
  _(0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0),OTF_REGLIST,0,0,

/* FL        FPn register list */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0),OTF_REGLIST,0,0,

/* FS        FPIAR/FPSR/FPCR register list */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0),OTF_REGLIST|FL_FPSpec,0,0,

/* ea addressing modes */
/* AY        all addressing modes 0-6,7.0-4 */
  _(1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0),0,0,0,

/* AM        alterable memory 2-6,7.0-1 */
  _(0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0),0,0,0,

/* MA        memory addressing modes 2-6,7.0-4 */
  _(0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0),0,0,0,

/* MI        memory addressing modes 2-6,7.0-3 without immediate */
  _(0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0),0,0,0,

/* FA        memory addressing modes 2-6,7.0-4 with float immediate */
  _(0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0),OTF_FLTIMM,0,0,

/* CF        (ColdFire) float addressing modes 2-5 and 7.2 */
  _(0,0,1,1,1,1,0,0,0,1,0,0,0,0,0,0),0,0,0,

/* MAQ       memory addressing modes 2-6,7.0-4 with 64-bit immediate */
  _(0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0),OTF_QUADIMM,0,0,

/* CFAM      (ColdFire) alterable memory 2-5 */
  _(0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0),0,0,0,

/* CM        (ColdFire) alterable memory 2-5 with MASK-flag (MAC) */
  _(0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0),FL_MAC,0,0,

/* AL        alterable 0-6,7.0-1 */
  _(1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0),0,0,0,

/* DA        data 0,2-6,7.0-4 */
  _(1,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0),0,0,0,

/* DN        data, but not immediate 0,2-6,7.0-3 */
  _(1,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0),0,0,0,

/* CFDA      (ColdFire) float data 0,2-6,7.0-4 (=CF + mode 0) */
  _(1,0,1,1,1,1,0,0,0,1,0,0,0,0,0,0),0,0,0,

/* CT        control, 2,5-6,7.0-3 */
  _(0,0,1,0,0,1,1,1,1,1,1,0,0,0,0,0),0,0,0,

/* AC        alterable control, 2,5-6,7.0-1 */
  _(0,0,1,0,0,1,1,1,1,0,0,0,0,0,0,0),0,0,0,

/* AD        alterable data, 0,2-6,7.0-1 */
  _(1,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0),0,0,0,

/* CFAD      (ColdFire) alterable data, 0,2-5 */
  _(1,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0),0,0,0,

/* BD        alterable control or data (bitfield), 0,2,5-6,7.0-1 */
  _(1,0,1,0,0,1,1,1,1,0,0,0,0,0,0,0),FL_Bitfield,0,0,

/* BS        control or data register (bitfield), 0,2,5-6,7.0-3 */
  _(1,0,1,0,0,1,1,1,1,1,1,0,0,0,0,0),FL_Bitfield,0,0,

/* AK        alterable memory (incl. k-factor) 2-6,7.0-1 */
  _(0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0),FL_KFactor,0,0,

/* MS        save operands, 2,4-6,7.0-1 */
  _(0,0,1,0,1,1,1,1,1,0,0,0,0,0,0,0),0,0,0,

/* MR        restore operands, 2-3,5-6,7.0-3 */
  _(0,0,1,1,0,1,1,1,1,1,1,0,0,0,0,0),0,0,0,

/* CFMM      (ColdFire) MOVEM, 2,5 */
  _(0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0),0,0,0,

/* CFMN      (ColdFire) FMOVEM src-ea, 2,5,7.2 */
  _(0,0,1,0,0,1,0,0,0,1,0,0,0,0,0,0),0,0,0,

/* ND        (Apollo) all except Dn, 1-6,7.0-4 */
  _(0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0),0,0,0,

/* NI        (Apollo) all except immediate, 0-6,7.0-3 */
  _(1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0),0,0,0,

/* NJ        (Apollo) all except Dn and immediate, 1-6,7.0-3 */
  _(0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0),0,0,0,

/* NK        (Apollo) all except An and immediate, 0,2-6,7.0-3 */
  _(1,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0),0,0,0,

/* BY        (Apollo) all addressing modes 0-6,7.0-4 with An replaced by Bn */
  _(1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0),FL_BnReg,0,0,

/* BI        (Apollo) all except immediate, 0-6,7.0-3 with An repl. by Bn */
  _(1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0),FL_BnReg,0,0,

/* BJ        (Apollo) all except Dn/An & immediate, 0-6,7.0-3, An -> Bn */
  _(0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0),FL_BnReg,0,0,

/* OF_       (Apollo) optional FPU register FPn */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0),OTF_OPT,0,0,

/* special registers */
/* _CCR */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_CCR,REG_CCR,
/* _SR */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_SR,REG_SR,
/* _USP */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_USP,REG_USP,
/* _CACHES */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_NC,REG_BC,
/* _ACC */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_ACC,REG_ACC,
/* _MACSR */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_MACSR,REG_MACSR,
/* _MASK */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_MASK,REG_MASK,
/* _CTRL */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_SRRANGE|OTF_CHKREG,
    REG_SFC,REG_PCR3L1,
/* _ACCX */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_SRRANGE|OTF_CHKREG,
    REG_ACC0,REG_ACC3,
/* _AEXT */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_SRRANGE|OTF_CHKREG,
    REG_ACCX01,REG_ACCX23,
/* _VAL */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_VAL,REG_VAL,
/* _FC */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_SFC,REG_DFC,
/* _RP_030 */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_SRP,REG_CRP,
/* _RP_851 */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_SRP,REG_DRP,
/* _TC */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_TC,REG_TC,
/* _AC */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_AC,REG_AC,
/* _M1_B */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_CAL,REG_SCC,
/* _BAC */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_BAC0,REG_BAC7,
/* _BAD */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_BAD0,REG_BAD7,
/* _PSR */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_MMUSR,REG_PSR,
/* _PCSR */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_MMUSR,REG_PCSR,
/* _TT */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_TT0,REG_TT1,
/* SH */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_CHKREG,REG_SFLEFT,REG_SFRIGHT,
/* VX (Apollo) */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_SRRANGE|OTF_CHKREG,REG_VX00,REG_VX23,
/* VXR2 (Apollo) En:En+1 */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_VXRNG2|OTF_SRRANGE|OTF_CHKREG,REG_VX00,REG_VX23,
/* VXR4 (Apollo) En-En+3 */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_SPECREG|OTF_VXRNG4|OTF_SRRANGE|OTF_CHKREG,REG_VX00,REG_VX23,
/* OVX (Apollo) optional En register */
  _(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1),OTF_OPT|OTF_SPECREG|OTF_SRRANGE|OTF_CHKREG,REG_VX00,REG_VX23,
};

#undef _


/* special operand insertion functions */

static void write_val(unsigned char *,int,int,taddr,int);

static void insert_cas2(unsigned char *d,struct oper_insert *i,operand *o)
{
  uint16_t w1 = (*(d+2)<<8) | *(d+3);
  uint16_t w2 = (*(d+4)<<8) | *(d+5);

  w1 |= (i->size==4 ? o->reg&15 : o->reg&7) << (16-((i->pos-16)+i->size));
  w2 |= (o->reg>>4) << (16-((i->pos-16)+i->size));
  *(d+2) = w1>>8;
  *(d+3) = w1&0xff;
  *(d+4) = w2>>8;
  *(d+5) = w2&0xff;
}

static void insert_macreg(unsigned char *d,struct oper_insert *i,operand *o)
{
  if (i->pos == 4) {
    /* special case: MSB is at bit-position 9 */
    write_val(d,4,3,o->reg,0);
    *(d+1) |= o->mode << 6;
  }
  else
    write_val(d,i->pos,4,(o->mode<<3)+o->reg,0);
  if (o->bf_offset)     /* .u extension selects upper word, else lower */
    *(d+3) |= i->size;  /* size holds the U/L mask for this register */
}

static void insert_muldivl(unsigned char *d,struct oper_insert *i,operand *o)
{
  unsigned char r = o->reg & 7;

  if (o->mode == MODE_An)
    r |= REGAn;
  *(d+2) |= r << 4;
  *(d+3) |= r;
}

static void insert_divl(unsigned char *d,struct oper_insert *i,operand *o)
{
  *(d+2) |= o->reg & 0xf0;
  *(d+3) |= o->reg & 0xf;
}

static void insert_tbl(unsigned char *d,struct oper_insert *i,operand *o)
{
  *(d+1) |= o->reg & 7;
  *(d+3) |= (o->reg & 0x70) >> 4;
}

static void insert_fp(unsigned char *d,struct oper_insert *i,operand *o)
{
  *(d+2) |= ((o->reg&7) << 2) | ((o->reg&7) >> 1);
  *(d+3) |= (o->reg&1) << 7;
}

static void insert_fpcs(unsigned char *d,struct oper_insert *i,operand *o)
{
  *(d+2) |= (o->reg&0x60) >> 5;
  *(d+3) |= ((o->reg&0x10) << 3) | (o->reg & 7);
}

static void insert_accx(unsigned char *d,struct oper_insert *i,operand *o)
{
  unsigned char v = o->extval[0];

  *(d+1) |= (v&1) << 7;
  *(d+3) |= (v&2) << 3;
}

static void insert_accx_rev(unsigned char *d,struct oper_insert *i,operand *o)
{
  unsigned char v = o->extval[0];

  *(d+1) |= ((v&1) ^ 1) << 7;
  *(d+3) |= (v&2) << 3;
}

static void insert_ammx(unsigned char *d,struct oper_insert *i,operand *o)
{
  unsigned char v = o->extval[0];

  write_val(d,i->pos,i->size,v&15,0);
  write_val(d,15-(i->flags&15),1,(v&16)!=0,0);
}

/* place to put an operand */
enum {
  NOP=0,NEA,SEA,MEA,BEA,KEA,REA,EAM,BRA,DBR,RHI,RLO,RL4,R2H,R2M,R2L,R2P,
  FPN,FPM,FMD,C2H,A2M,A2L,AXA,AXB,AXD,AX0,CS1,CS2,CS3,MDL,DVL,TBL,FPS,FPC,
  RMM,RMW,RMY,RMX,ACX,ACR,DL8,DL4,D3Q,DL3,CAC,D16,S16,D2R,ELC,EL8,E8R,
  EL3,EL4,EM3,EM4,EH3,BAX,FCR,F13,M3Q,MSF,ACW,AHI,ALO,LIN
};

struct oper_insert insert_info[] = {
/* NOP do nothing for this operand (usually a special reg.) */
  M_nop,0,0,0,0,

/* NEA don't store effective address, but extension words */
  M_noea,0,0,0,0,
  
/* SEA standard effective address */
  M_ea,0,0,0,0,

/* MEA high effective address for MOVE */
  M_high_ea,0,0,0,0,

/* BEA std. EA including bitfield offset/width */
  M_bfea,0,0,0,0,

/* KEA std. EA including K-factor */
  M_kfea,0,0,0,0,

/* REA store only register-part of effective address */
  M_ea,0,0,IIF_NOMODE,0,

/* EAM standard effective address with MAC MASK-flag in bit 5 of 2nd word */
  M_ea,0,5,IIF_MASK,0,

/* BRA pc-relative branch to label */
  M_branch,0,0,IIF_BCC,0,

/* DBR DBcc, FBcc or PBcc branch to label */
  M_branch,0,0,0,0,

/* RHI register 3 bits in bits 11-9 */
  M_reg,3,4,0,0,

/* RLO register 3 bits in bits 2-0 */
  M_reg,3,13,0,0,

/* RL4 register 4 bits in bits 3-0 */
  M_reg,4,12,0,0,

/* R2H register 3 bits in 2nd word bits 14-12 */
  M_reg,3,17,0,0,

/* R2M register 3 bits in 2nd word bits 8-6 */
  M_reg,3,23,0,0,

/* R2L register 3 bits in 2nd word bits 2-0 */
  M_reg,3,29,0,0,

/* R2P register 3 bits in 2nd word bits 7-5 */
  M_reg,3,24,0,0,

/* FPN register 3 bits in 2nd word bits 9-7 (FPn) */
  M_reg,3,22,0,0,

/* FPM register 3 bits in 2nd word bits 12-10 (FPm) */
  M_reg,3,19,0,0,

/* FMD register 3 bits in 2nd word bits 6-4 (FMOVE dynamic) */
  M_reg,3,25,0,0,

/* C2H register 4 bits in 2nd word bits 15-12 (cmp2,chk2,moves) */
  M_reg,4,16,0,0,

/* A2M register 4 bits in 2nd word bits 11-8 (Apollo AMMX) */
  M_reg,4,20,0,0,

/* A2L register 4 bits in 2nd word bits 3-0 (Apollo AMMX) */
  M_reg,4,28,0,0,

/* AXA vector register field A (Apollo AMMX) */
  M_func,4,28,IIF_A|IIF_ABSVAL,insert_ammx,

/* AXB vector register field B (Apollo AMMX) */
  M_func,4,16,IIF_B|IIF_ABSVAL,insert_ammx,

/* AXD vector register field D (Apollo AMMX) */
  M_func,4,20,IIF_D|IIF_ABSVAL,insert_ammx,

/* AX0 vector register field A in first word (Apollo AMMX) */
  M_func,4,12,IIF_A|IIF_ABSVAL,insert_ammx,

/* CS1 register 3 bits for CAS2 bits 2-0 */
  M_func,3,29,0,insert_cas2,

/* CS2 register 3 bits for CAS2 bits 8-6 */
  M_func,3,23,0,insert_cas2,

/* CS3 register 4 bits for CAS2 (CAS2) bits 15-12 */
  M_func,4,16,0,insert_cas2,

/* MDL insert 4 bit reg. Dq/Dl into 2nd word bits 15-12/3-0 */
  M_func,0,0,0,insert_muldivl,

/* DVL 4 bit Dq to 2nd word bits 15-12, Dr to bits 3-0 */
  M_func,0,0,0,insert_divl,

/* TBL 3 bit Dym to 1st w. bits 2-0, Dyn to 2nd w. 2-0 */
  M_func,0,0,0,insert_tbl,

/* FPS 3 bit FPn to 2nd word bits 12-10 (FPm) and 9-7 (FPn) */
  M_func,0,0,0,insert_fp,

/* FPC 3 bit FPc to 2nd word bits 2-0, FPs to bits 9-7 */
  M_func,0,0,0,insert_fpcs,

/* RMM register 4 bits in bits 3-0, MAC U/L flag in bit 6 of 2nd word */
  M_func,0x40,12,0,insert_macreg,

/* RMW register 4 bits in bits 6,11-9, MAC U/L flag in bit 7 of 2nd word */
  M_func,0x80,4,0,insert_macreg,

/* RMY register 4 bits in 2nd word bits 3-0, MAC U/L flag in bit 6 */
  M_func,0x40,28,0,insert_macreg,

/* RMX register 4 bits in 2nd word bits 15-12, MAC U/L flag in bit 7 */
  M_func,0x80,16,0,insert_macreg,

/* ACX 2 bits ACC: MSB in bit 4 of 2nd word, LSB in bit 7 of first word */
  M_func,0,0,IIF_ABSVAL,insert_accx,

/* ACR 2 bits ACC: MSB in bit 4 of 2nd word, reversed LSB in bit 7 of first */
  M_func,0,0,IIF_ABSVAL,insert_accx_rev,

/* DL8 8-bit data in lo-byte */
  M_val0,8,8,IIF_SIGNED,0,

/* DL4 4-bit value in bits 3-0 */
  M_val0,4,12,0,0,

/* D3Q addq/subq 3-bit quick data in bits 11-9 */
  M_val0,3,4,IIF_MASK,0,

/* DL3 3-bit value in bits 2-0 */
  M_val0,3,13,0,0,

/* CAC 2-bit cache field in bits 7-6 */
  M_val0,2,8,0,0,

/* D16 16-bit data in 2nd word */
  M_val0,16,16,0,0,

/* S16 signed 16-bit data in 2nd word */
  M_val0,16,16,IIF_SIGNED,0,

/* D2R 16-bit reversed data in 2nd word (movem predec.) */
  M_val0,16,16,IIF_REVERSE,0,
  
/* ELC 12-bit value in bits 11-0 of 2nd word */
  M_val0,12,20,0,0,

/* EL8 8-bit data in lo-byte of extension word (2nd word) */
  M_val0,8,24,0,0,

/* E8R 8-bit reversed data in lo-byte of 2nd word (fmovem) */
  M_val0,8,24,IIF_REVERSE,0,

/* EL3 3-bit value in bits 2-0 of 2nd word */
  M_val0,3,29,0,0,

/* EL4 4-bit value in bits 3-0 of 2nd word */
  M_val0,4,28,0,0,

/* EM3 3-bit value in bits 7-5 of 2nd word */
  M_val0,3,24,0,0,

/* EM4 4-bit value in bits 8-5 of 2nd word */
  M_val0,4,23,0,0,

/* EH3 3-bit value in bits 12-10 of 2nd word */
  M_val0,3,19,0,0,

/* BAX 3-bit number of BAC/BAD reg. in bits 4-2 of 2nd word */
  M_val0,3,27,0,0,

/* FCR 7-bit ROM offset in bits 6-0 of 2nd word (FMOVECR) */
  M_val0,7,25,0,0,

/* F13 13-bit special register mask in bits 12-0 of 2nd word (FMOVEM) */
  M_val0,13,19,0,0,

/* M3Q mov3q 3-bit quick data in bits 11-9, -1 is written as 0 */
  M_val0,3,4,IIF_3Q,0,

/* MSF 2-bit MAC scale factor in bits 10-9 of 2nd word */
  M_val0,2,21,0,0,

/* ACW 2-bit ACC in bits 3-2 of 2nd word */
  M_val0,2,28,0,0,

/* AHI 2-bit ACC in bits 10-9 */
  M_val0,2,5,0,0,

/* ALO 2-bit ACC in bits 1-0 */
  M_val0,2,14,0,0,

/* LIN 12-bit value in bits 0-11 (LINE-A, LINE-F) */
  M_val0,12,4,0,0,
};
