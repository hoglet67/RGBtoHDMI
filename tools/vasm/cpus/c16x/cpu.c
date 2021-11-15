/* cpu.c example cpu-description file */
/* (c) in 2002 by Volker Barthelmann */

#include "vasm.h"

char *cpu_copyright="vasm c16x/st10 cpu backend 0.2c (c) in 2002-2005 Volker Barthelmann";
char *cpuname="c16x";

mnemonic mnemonics[]={
#include "opcodes.h"
};

int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);

int bitsperbyte=8;
int bytespertaddr=4;

static int JMPA,JMPR,JMPS,JNB,JB,JBC,JNBS,JMP;
static int notrans,tojmpa;

#define JMPCONV 256
#define INVCC(c) (((c)&1)?(c)-1:(c)+1)

#define ISBIT 1

typedef struct sfr {
  struct sfr *next;
  int flags;
  unsigned int laddr,saddr,boffset;
} sfr;


sfr *first_sfr;
#define SFRHTSIZE 1024
hashtable *sfrhash;

static char *skip_reg(char *s,int *reg)
{
  int r=-1;
  if(*s!='r'&&*s!='R'){
    cpu_error(1);
    return s;
  }
  s++;
  if(*s<'0'||*s>'9'){
    cpu_error(1);
    return s;
  }
  r=*s++-'0';
  if(*s>='0'&&*s<='5')
    r=10*r+*s++-'0';
  *reg=r;
  return s;
}

int parse_operand(char *p,int len,operand *op,int requires)
{
  op->type=-1;
  op->mod=-1;
  p=skip(p);
  if(requires==OP_REL){
    char *s=p;
    op->type=OP_REL;
    op->offset=parse_expr(&s);
    simplify_expr(op->offset);
    if(s==p)
      return 0;
    else
      return 1;
  }
  if(requires==OP_CC){
    op->type=OP_CC;
    if(len<4||len>6||p[0]!='c'||p[1]!='c'||p[2]!='_')
      return 0;
    if(len==4){
      if(p[3]=='z')
	op->cc=2;
      else if(p[3]=='v')
	op->cc=4;
      else if(p[3]=='n')
	op->cc=6;
      else if(p[3]=='c')
	op->cc=8;
      else
	return 0;
    }else if(len==5){
      if(p[3]=='u'&&p[4]=='c')
	op->cc=0;
      else if(p[3]=='n'&&p[4]=='z')
	op->cc=3;
      else if(p[3]=='n'&&p[4]=='v')
	op->cc=5;
      else if(p[3]=='n'&&p[4]=='n')
	op->cc=7;
      else if(p[3]=='n'&&p[4]=='c')
	op->cc=0;
      else if(p[3]=='e'&&p[4]=='q')
	op->cc=2;
      else if(p[3]=='n'&&p[4]=='e')
	op->cc=3;
      else
	return 0;
    }else if(len==6){
      if(!strncmp(p+3,"ult",3))
	op->cc=8;
      else if(!strncmp(p+3,"ule",3))
	op->cc=0xf;
      else if(!strncmp(p+3,"uge",3))
	op->cc=0x9;
      else if(!strncmp(p+3,"ugt",3))
	op->cc=0xe;
      else if(!strncmp(p+3,"slt",3))
	op->cc=0xc;
      else if(!strncmp(p+3,"sle",3))
	op->cc=0xb;
      else if(!strncmp(p+3,"sge",3))
	op->cc=0xd;
      else if(!strncmp(p+3,"sgt",3))
	op->cc=0xa;
      else if(!strncmp(p+3,"net",3))
	op->cc=0x1;
      else
	return 0;
    }
    return 1;
  }
  if((p[0]=='r'||p[0]=='R')&&p[1]>='0'&&p[1]<='9'&&(len==2||p[2]=='.')){
    op->type=OP_GPR;
    op->reg=p[1]-'0';
    op->regsfr=op->reg+0xf0;
    if(len>2){
      op->type=OP_BADDR;
      if(requires==OP_BADDR){
	p=skip(p+3);
	op->boffset=parse_expr(&p);
	op->offset=number_expr(op->regsfr);
      }
    }
  }else if((p[0]=='r'||p[0]=='R')&&p[1]=='1'&&p[2]>='0'&&p[2]<='5'&&(len==3||p[3]=='.')){
    op->type=OP_GPR;
    op->reg=(p[1]-'0')*10+p[2]-'0';
    op->regsfr=op->reg+0xf0;
    if(len>3){
      op->type=OP_BADDR;
      if(requires==OP_BADDR){
	p=skip(p+4);
	op->boffset=parse_expr(&p);
	op->offset=number_expr(op->regsfr);
      }
    }
  }else if(len==3&&(p[0]=='r'||p[0]=='R')&&(p[1]=='l'||p[1]=='L')&&p[2]>='0'&&p[2]<='7'){
    op->type=OP_BGPR;
    op->reg=(p[2]-'0')*2;
    op->regsfr=op->reg+0xf0;
  }else if(len==3&&(p[0]=='r'||p[0]=='R')&&(p[1]=='h'||p[1]=='H')&&p[2]>='0'&&p[2]<='7'){
    op->type=OP_BGPR;
    op->reg=(p[2]-'0')*2+1;
    op->regsfr=op->reg+0xf0;
  }else if(p[0]=='#'){
    op->type=OP_IMM16;
    p=skip(p+1);
    if((!strncmp("SOF",p,3)||!strncmp("sof",p,3))&&isspace((unsigned char)p[3])){op->mod=MOD_SOF;p=skip(p+3);}
    if((!strncmp("SEG",p,3)||!strncmp("seg",p,3))&&isspace((unsigned char)p[3])){op->mod=MOD_SEG;p=skip(p+3);}
    if((!strncmp("DPP0:",p,5)||!strncmp("dpp0:",p,5))){op->mod=MOD_DPP0;p=skip(p+5);}
    if((!strncmp("DPP1:",p,5)||!strncmp("dpp1:",p,5))){op->mod=MOD_DPP1;p=skip(p+5);}
    if((!strncmp("DPP2:",p,5)||!strncmp("dpp2:",p,5))){op->mod=MOD_DPP2;p=skip(p+5);}
    if((!strncmp("DPP3:",p,5)||!strncmp("dpp3:",p,5))){op->mod=MOD_DPP3;p=skip(p+5);}
    if((!strncmp("DPPX:",p,5)||!strncmp("dppx:",p,5))){op->mod=MOD_DPPX;p=skip(p+5);}
    op->offset=parse_expr(&p);
    simplify_expr(op->offset);
#if 0
    if(op->offset->type==NUM){
      taddr val=op->offset->c.val;
      if(val>=0&&val<=7)
	op->type=OP_IMM3;
      else if(val>=0&&val<=15)
	op->type=OP_IMM4;
      else if(val>=0&&val<=127)
	op->type=OP_IMM7;
      else if(val>=0&&val<=255)
	op->type=OP_IMM8;
    }
#endif
  }else if(*p=='['){
    p=skip(p+1);
    if(*p=='-'){
      p=skip(p+1);
      p=skip_reg(p,&op->reg);
      p=skip(p);
      if(*p!=']')
	cpu_error(0);
      if(op->reg<=3)
	op->type=OP_PREDEC03;
      else
	op->type=OP_PREDEC;
    }else{
      p=skip_reg(p,&op->reg);
      p=skip(p);
      if(*p=='+'){
	p=skip(p+1);
	if(*p==']'){
	  if(op->reg<=3)
	    op->type=OP_POSTINC03;
	  else
	    op->type=OP_POSTINC;
	}else{
	  if(*p!='#')
	    cpu_error(0);
	  p=skip(p+1);
	  op->offset=parse_expr(&p);
	  p=skip(p);
	  op->type=OP_REGDISP;
	}
      }else{
	if(op->reg<=3)
	  op->type=OP_REG03IND;
	else
	  op->type=OP_REGIND;
      }
      if(*p!=']')
	cpu_error(0);
    }
  }else{
    if(ISIDSTART(*p)){
      char *name=p;
      hashdata data;
      while((p==name||ISIDCHAR(*p))&&*p!='.')
	p++;
      if(find_namelen(sfrhash,name,p-name,&data)){
	sfr *sfr;
	sfr=data.ptr;
	if(sfr->flags&ISBIT){
	  op->offset=number_expr(sfr->saddr);
	  op->type=OP_BADDR;
	  op->boffset=number_expr(sfr->boffset);
	}else{
	  if(requires==OP_SFR||requires==OP_BSFR||requires==OP_BWORD){
	    op->offset=number_expr(sfr->saddr);
	    op->type=requires;
	  }else if(requires==OP_BADDR&&*p=='.'){
	    op->offset=number_expr(sfr->saddr);
	    p=skip(p+1);
	    op->boffset=parse_expr(&p);
	    op->type=OP_BADDR;
	  }else if(requires==OP_ABS||requires==OP_BABS){
	    op->type=requires;
	    op->offset=number_expr((2*sfr->saddr)+(sfr->laddr<<8));
	  }
	}
      }
      if(op->type==-1)
	p=name;
    }
    if(op->type==-1){
      if((!strncmp("SOF",p,3)||!strncmp("sof",p,3))&&isspace((unsigned char)p[3])){op->mod=MOD_SOF;p=skip(p+3);}
      if((!strncmp("SEG",p,3)||!strncmp("seg",p,3))&&isspace((unsigned char)p[3])){op->mod=MOD_SEG;p=skip(p+3);}
      if((!strncmp("DPP0:",p,5)||!strncmp("dpp0:",p,5))){op->mod=MOD_DPP0;p=skip(p+5);}
      if((!strncmp("DPP1:",p,5)||!strncmp("dpp1:",p,5))){op->mod=MOD_DPP1;p=skip(p+5);}
      if((!strncmp("DPP2:",p,5)||!strncmp("dpp2:",p,5))){op->mod=MOD_DPP2;p=skip(p+5);}
      if((!strncmp("DPP3:",p,5)||!strncmp("dpp3:",p,5))){op->mod=MOD_DPP3;p=skip(p+5);}
      if((!strncmp("DPPX:",p,5)||!strncmp("dppx:",p,5))){op->mod=MOD_DPPX;p=skip(p+5);}
      op->offset=parse_expr(&p);
      op->type=OP_ABS;
    }
  }
  if(requires==op->type)
    return 1;
  if(requires==OP_BWORD&&op->type==OP_SFR)
    return 1;
  if(op->type==OP_IMM16&&(requires>=OP_IMM2&&requires<=OP_IMM16))
    return 1;
  if(op->type==OP_PREDEC03&&requires==OP_PREDEC)
    return 1;
  if(op->type==OP_POSTINC03&&requires==OP_POSTINC)
    return 1;
  if(op->type==OP_REG03IND&&requires==OP_REGIND)
    return 1;
  if((requires==OP_SFR&&op->type==OP_GPR)||
     (requires==OP_BWORD&&op->type==OP_GPR)||
     (requires==OP_BWORD&&op->type==OP_BGPR)||
     (requires==OP_BSFR&&op->type==OP_BGPR)){
    op->offset=number_expr(op->regsfr);
    return 1;
  }
  if(requires==OP_BSFR&&op->type==OP_BGPR)
    return 1;
  if(requires==OP_JADDR&&op->type==OP_ABS)
    return 1;
  if(requires==OP_BABS&&op->type==OP_ABS)
    return 1;
  /*FIXME*/
  return 0;
}

static taddr reloffset(expr *tree,section *sec,taddr pc)
{
  symbol *sym;
  int btype;
  taddr val;
  simplify_expr(tree);
  if(tree->type==NUM){
    /* should we do it like this?? */
    val=tree->c.val;
  }else{
    btype=find_base(tree,&sym,sec,pc);
    if(btype!=BASE_OK||!LOCREF(sym)||sym->sec!=sec)
      val=0xffff;
    else{
      eval_expr(tree,&val,sec,pc);
      val=val-pc;
    }
  }
  return val;
}

static taddr absoffset2(expr *tree,int mod,section *sec,taddr pc,rlist **relocs,int roffset,int size,taddr mask)
{
  taddr val;
  if(mod==MOD_SOF){
    if(mask!=0xffffffff&&mask!=0xffff) cpu_error(5);
    mask=0xffff;
  }
  if(mod==MOD_SEG){
    if(mask!=0xff&&mask!=0xffff&&mask!=0xffffffff) cpu_error(6);
    mask<<=16;
  }
  if(mod==MOD_DPP0||mod==MOD_DPP1||mod==MOD_DPP2||mod==MOD_DPP3||mod==MOD_DPPX){
    if(mask!=0xffffffff&&mask!=0xffff) cpu_error(7);
    mask=0x3fff;
  }
  if(!eval_expr(tree,&val,sec,pc)){
    taddr addend=val;
    symbol *base;
    if(find_base(tree,&base,sec,pc)!=BASE_OK){
      general_error(38);
      return val;
    }
    if(mod==MOD_DPP1) val|=0x4000;
    if(mod==MOD_DPP2) val|=0x8000;
    if(mod==MOD_DPP3) val|=0xc000;
    if(mod==MOD_DPPX){
      static int dpplen;
      static char *dppname;
      char *id=base->name;
      symbol *dppsym;
      size-=2;
      if(strlen(id)+9>dpplen){
        myfree(dppname);
        dppname=mymalloc(dpplen=strlen(id)+9);
      }
      strcpy(dppname,"___DPP_");
      strcat(dppname,id);
      dppsym=new_import(dppname);
      if(dppsym->type==EXPRESSION){
        if(!eval_expr(dppsym->expr,&val,0,0))
          ierror(0);
        val<<=14;
      }else{
        add_nreloc_masked(relocs,dppsym,0,REL_ABS,2,roffset+14,0x3);
      }
    }
    add_nreloc_masked(relocs,base,addend,REL_ABS,size,roffset,mask);
    return val;
  }
  val&=mask;
  if(mod==MOD_DPPX) cpu_error(7);
  if(mod==MOD_DPP1) val|=0x4000;
  if(mod==MOD_DPP2) val|=0x8000;
  if(mod==MOD_DPP3) val|=0xc000;
  if(mod==MOD_SEG) val>>=16;
  /*FIXME: range check */
#if 1
  if(size==16)
    return val&0xffff;
  else
    return val&((1<<size)-1);
#else
  return val;
#endif
}

static taddr absoffset(expr *tree,int mod,section *sec,taddr pc,rlist **relocs,int roffset,int size)
{
  /* taddr mask=size==32?0xffffffff:((((taddr)1)<<size)-1);*/
  return absoffset2(tree,mod,sec,pc,relocs,roffset,size,0xffffffff);
}

static taddr absval(expr *tree,section *sec,taddr pc,int bits)
{
  taddr val;
  if(!eval_expr(tree,&val,sec,pc))
    cpu_error(0);
  if(bits==2){
    /* ext instructions */
    if(val<1||val>4)
      cpu_error(3,2);
    return val;
  }else if(val<0||val>=(1<<bits))
    cpu_error(3,bits);
  return val&((1<<bits)-1);
}

static int translate(instruction *p,section *sec,taddr pc)
{
  int c=p->code;
  taddr val;
  /* choose one of jmpr/jmpa */
  if(c==JMP||(!notrans&&(c==JMPA||c==JMPR||c==JB||c==JNB))){
    val=reloffset(p->op[1]->offset,sec,pc);
    if(val<-256||val>254||val%2){
      if(c==JB) return JNB|JMPCONV;
      if(c==JNB) return JB|JMPCONV;
      if(c==JMPA) return JMPA;
      if(tojmpa) return JMPA;
      if(p->op[0]->cc==0)
	return JMPS;
      else
	return JMPR|JMPCONV;
    }else{
      if(c==JB||c==JNB)
	return c;
      return JMPR;
    }
  }
  /* choose between gpr,#imm3 and reg,#imm16 */
  if(mnemonics[c].operand_type[1]==OP_IMM3){
    if(!eval_expr(p->op[1]->offset,&val,sec,pc)||val<0||val>7){
      if(!strcmp(mnemonics[c].name,mnemonics[c+1].name))
	return c+1;
    }
  }
  /* choose between gpr,#imm4 and reg,#imm16 */
  if(mnemonics[c].operand_type[1]==OP_IMM4){
    if(!eval_expr(p->op[1]->offset,&val,sec,pc)||val<0||val>7){
      if(!strcmp(mnemonics[c].name,mnemonics[c+1].name))
	return c+1;
    }
  }
  return c;
}

/* Convert an instruction into a DATA atom including relocations,
   if necessary. */
dblock *eval_instruction(instruction *p,section *sec,taddr pc)
{
  dblock *db=new_dblock();
  int opcode,c,jmpconv=0,osize;
  unsigned long code;
  unsigned char *d;
  taddr val;
  rlist *relocs=0;
  operand *jmpaddr;

  c=translate(p,sec,pc);
  if(c&JMPCONV){ jmpconv=1;c&=~JMPCONV;}
  if((mnemonics[p->code].operand_type[0]==OP_GPR&&mnemonics[c].operand_type[0]==OP_SFR)||
     (mnemonics[p->code].operand_type[0]==OP_BGPR&&mnemonics[c].operand_type[0]==OP_BSFR))
    p->op[0]->offset=number_expr(p->op[0]->regsfr);
  

  db->size=osize=mnemonics[c].ext.len*2;
  if(jmpconv) db->size+=4;
  db->data=mymalloc(db->size);

  opcode=mnemonics[c].ext.opcode;
  switch(mnemonics[c].ext.encoding){
  case 0:
    code=opcode<<16|(opcode>>8)<<8|opcode>>8;
    break;
  case 1:
    code=opcode;
    break;
  case 2:
    code=opcode|p->op[0]->reg<<4|p->op[1]->reg;
    break;
  case 3:
    code=opcode|p->op[0]->reg|p->op[1]->reg<<4;
    break;
  case 4:
    code=opcode|p->op[0]->reg<<4|p->op[1]->reg|8;
    break;
  case 5:
    code=opcode|p->op[0]->reg<<4|p->op[1]->reg|12;
    break;
  case 6:
    code=opcode|p->op[0]->reg<<4|absval(p->op[1]->offset,sec,pc,3);
    break;
  case 7:
    /* fall through */
  case 8:
    code=opcode<<16|absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,20,8)<<16|absoffset(p->op[1]->offset,p->op[1]->mod,sec,pc,&relocs,16,16);
    break;
  case 9:
    code=opcode|p->op[0]->reg|absval(p->op[1]->offset,sec,pc,4)<<4;
    break;
  case 10:
/* rfi: reorder bmov operands */
    code=opcode<<16|
      absoffset(p->op[1]->offset,p->op[1]->mod,sec,pc,&relocs,8,8)<<16|
      absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,16,8)<<0|
      absoffset(p->op[1]->boffset,0,sec,pc,&relocs,24,4)<<12|
      absoffset(p->op[0]->boffset,0,sec,pc,&relocs,28,4)<<8;
    break;
  case 11:
    code=opcode|absoffset(p->op[0]->boffset,0,sec,pc,&relocs,0,4)<<12|absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,8,8);
    break;
  case 12:
    code=opcode<<16|
      absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,8,8)<<16|
      absoffset(p->op[2]->offset,p->op[2]->mod,sec,pc,&relocs,16,8)<<8|
      absoffset(p->op[1]->offset,p->op[1]->mod,sec,pc,&relocs,24,8);
    break;
  case 13:
    code=opcode<<16|
      absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,8,8)<<16|
      absoffset(p->op[1]->offset,p->op[1]->mod,sec,pc,&relocs,16,8)<<8|
      absoffset(p->op[2]->offset,p->op[2]->mod,sec,pc,&relocs,24,8);
    break;    
  case 14:
    code=opcode<<16|p->op[0]->cc<<20|absoffset(p->op[1]->offset,p->op[1]->mod,sec,pc,&relocs,16,16);
    break;
  case 15:
    code=opcode|p->op[0]->cc<<4|p->op[1]->reg;
    break;
  case 16:
    val=((reloffset(p->op[0]->offset,sec,pc)-2)>>1)&255;
    code=opcode|val;
    break;
  case 17:
    if(p->op[0]->type==OP_CC){
      /* jmp cc_uc was converted to jmps */
      code=opcode<<16|absoffset2(p->op[1]->offset,0,sec,pc,&relocs,8,8,0xffff0000)<<16|absoffset2(p->op[1]->offset,0,sec,pc,&relocs,16,16,0xffff);
    }else{
      code=opcode<<16|absoffset2(p->op[0]->offset,0,sec,pc,&relocs,8,8,0xffff0000)<<16|absoffset2(p->op[0]->offset,0,sec,pc,&relocs,16,16,0xffff);
    }
    break;
  case 18:
    /* fall through */
  case 19:
    code=opcode<<16|0xf<<20|p->op[0]->reg<<16|absoffset(p->op[1]->offset,p->op[1]->mod,sec,pc,&relocs,16,16);
    break;
  case 20:
    code=opcode|p->op[0]->reg<<4;
    break;
  case 21:
    code=opcode|p->op[0]->reg<<4|p->op[0]->reg;
    break;
  case 22:
    if(!jmpconv){
      val=((reloffset(p->op[1]->offset,sec,pc)-4)>>1)&255;
      code=opcode<<16|
	absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,8,8)<<16|
	absoffset(p->op[0]->boffset,0,sec,pc,&relocs,24,4)<<12|
	val;
    }else{
      code=opcode<<16|
	absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,8,8)<<16|
	absoffset(p->op[0]->boffset,0,sec,pc,&relocs,24,4)<<12|
	2;
      jmpaddr=p->op[1];
    }
    break;
  case 23:
    if(!jmpconv){
      val=((reloffset(p->op[1]->offset,sec,pc)-2)>>1)&255;
      code=opcode|p->op[0]->cc<<12|val;
    }else{
      code=opcode|INVCC(p->op[0]->cc)<<12|2;
      jmpaddr=p->op[1];
    }
    break;
  case 24:
    code=opcode<<16|p->op[0]->reg<<20|p->op[1]->reg<<16|absoffset(p->op[1]->offset,p->op[1]->mod,sec,pc,&relocs,16,16);
    break;
  case 25:
    code=opcode<<16|p->op[0]->reg<<16|absoffset(p->op[1]->offset,p->op[1]->mod,sec,pc,&relocs,16,16);
    break;
  case 26:
    code=opcode|absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,8,8);
    break;
  case 27:
    code=opcode|absval(p->op[0]->offset,sec,pc,7)<<1;
    break;
  case 28:
    code=opcode<<16|p->op[0]->reg<<16|p->op[1]->reg<<20|absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,16,16);
    break;
  case 29:
    code=opcode<<16|absoffset(p->op[1]->offset,p->op[1]->mod,sec,pc,&relocs,8,8)<<16|absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,16,16);
    break;
  case 30:
    code=opcode<<16|p->op[1]->reg<<16|absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,16,16);
    break;
  case 31:
    code=opcode|((absval(p->op[0]->offset,sec,pc,2)-1)<<4);
    break;
  case 32:
    code=opcode|p->op[0]->reg|((absval(p->op[1]->offset,sec,pc,2)-1)<<4);
    break;
  case 34:
    code=opcode<<16|((absval(p->op[1]->offset,sec,pc,2)-1)<<20)|absoffset(p->op[0]->offset,p->op[0]->mod,sec,pc,&relocs,16,8);
    break;
  case 33:
  default:
    ierror(mnemonics[c].ext.encoding);
  }

  d=db->data;
  if(osize==4){
    *d++=code>>24;
    *d++=code>>16;
    *d++=code;
    *d++=code>>8;
  }else{
    *d++=code>>8;
    *d++=code;
  }
  if(jmpconv){
    *d++=0xfa;
    *d++=absoffset2(jmpaddr->offset,0,sec,pc,&relocs,8+8*osize,8,0xffff0000);
    val=absoffset2(jmpaddr->offset,0,sec,pc,&relocs,16+8*osize,16,0xffff);
    *d++=val>>8;
    *d++=val;
  }
  db->relocs=relocs;
  return db;
}

/* Create a dblock (with relocs, if necessary) for size bits of data. */
dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
{
  dblock *new=new_dblock();
  taddr val;
  new->size=(bitsize+7)/8;
  new->data=mymalloc(new->size);
  if(op->type!=OP_ABS)
    ierror(0);
  if(bitsize!=8&&bitsize!=16&&bitsize!=32)
    cpu_error(4);
  val=absoffset(op->offset,op->mod,sec,pc,&new->relocs,0,bitsize);
  if(bitsize==32){
    new->data[3]=val>>24;
    new->data[2]=val>>16;    
    new->data[1]=val>>8;
    new->data[0]=val;        
  }else if(bitsize==16){
    new->data[1]=val>>8;
    new->data[0]=val;    
  }else
    new->data[0]=val;
  return new;
}                                     


/* Calculate the size of the current instruction; must be identical
   to the data created by eval_instruction. */
size_t instruction_size(instruction *p,section *sec,taddr pc)
{
  int c=translate(p,sec,pc),add=0;
  if(c&JMPCONV){ add=4;c&=~JMPCONV;}
  return mnemonics[c].ext.len*2+add;
}

operand *new_operand()
{
  operand *new=mymalloc(sizeof(*new));
  new->type=-1;
  return new;
}

/* return true, if initialization was successfull */
int init_cpu()
{
  int i;
  for(i=0;i<mnemonic_cnt;i++){
    if(!strcmp(mnemonics[i].name,"jmp"))
      JMP=i;
    if(!strcmp(mnemonics[i].name,"jmpr"))
      JMPR=i;
    if(!strcmp(mnemonics[i].name,"jmpa"))
      JMPA=i;
    if(!strcmp(mnemonics[i].name,"jmps"))
      JMPS=i;
    if(!strcmp(mnemonics[i].name,"jb"))
      JB=i;
    if(!strcmp(mnemonics[i].name,"jbc"))
      JBC=i;
    if(!strcmp(mnemonics[i].name,"jnb"))
      JNB=i;
    if(!strcmp(mnemonics[i].name,"jnbs"))
      JNBS=i;
  }
  sfrhash=new_hashtable(SFRHTSIZE);
  return 1;
}

/* return true, if the passed argument is understood */
int cpu_args(char *p)
{
  if(!strcmp(p,"-no-translations")){
    notrans=1;
    return 1;
  }
  if(!strcmp(p,"-jmpa")){
    tojmpa=1;
    return 1;
  }
  return 0;
}

/* parse cpu-specific directives; return pointer to end of
   cpu-specific text */
char *parse_cpu_special(char *s)
{
  char *name=s,*merk=s;
  if(ISIDSTART(*s)){
    s++;
    while(ISIDCHAR(*s))
      s++;
    if(dotdirs&&*name=='.')
      name++;
    if(s-name==3&&!strncmp(name,"sfr",3)){
      sfr *new;
      hashdata data;
      expr *tree;
      s=skip(s);
      if(!ISIDSTART(*s))
	cpu_error(0);
      name=s++;
      while(ISIDCHAR(*s))
	s++;
      if(find_namelen(sfrhash,name,s-name,&data))
	new=data.ptr;
      else{
	data.ptr=new=mymalloc(sizeof(*new));
	add_hashentry(sfrhash,cnvstr(name,s-name),data);
	new->next=first_sfr;
	first_sfr=new;
      }
      new->flags=new->laddr=new->saddr=0;
      new->boffset=0;
      s=skip(s);
      if(*s!=',')
	cpu_error(0);
      else
	s=skip(s+1);
      tree=parse_expr(&s);
      simplify_expr(tree);
      if(!tree||tree->type!=NUM)
	cpu_error(0);
      else
	new->laddr=tree->c.val;
      s=skip(s);
      if(tree->c.val==0xfe||tree->c.val==0xf0){
	if(*s!=',')
	  cpu_error(0);
	else
	  s=skip(s+1);
        free_expr(tree);
	tree=parse_expr(&s);
	simplify_expr(tree);
	if(!tree||tree->type!=NUM)
	  cpu_error(0);
	else
	  new->saddr=tree->c.val;
	free_expr(tree);      
	s=skip(s);
      }else{
	if(tree->c.val>=0xfe00)
	  new->laddr=0xfe;
	else
	  new->laddr=0xf0;
	new->saddr=(tree->c.val-(new->laddr<<8))/2;
	if((new->laddr<<8)+2*new->saddr!=tree->c.val) ierror(0);
        free_expr(tree);
      }	
      if(*s==','){
	s=skip(s+1);
	tree=parse_expr(&s);
	simplify_expr(tree);
	new->boffset=tree->c.val;
	new->flags|=ISBIT;
        free_expr(tree);
      }
      return skip(s);
    }
  }
  return merk;
}
