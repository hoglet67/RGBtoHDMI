/* cpu.c qnice cpu-description file */
/* (c) in 2016 by Volker Barthelmann */

#include "vasm.h"

char *cpu_copyright="vasm qnice cpu backend 0.1 (c) in 2016 Volker Barthelmann";
char *cpuname="qnice";

mnemonic mnemonics[]={
#include "opcodes.h"
};

int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);

int bitsperbyte=8;
int bytespertaddr=4;

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
    static const char ccodes[]="1xcznvim";
    int i;
    op->type=OP_CC;
    if((p[0]=='!'&&len!=2)||(p[0]!='!'&&len!=1))
      return 0;
    if(*p=='!'){
      op->cc=8;
      p++;
    }else
      op->cc=0;
    for(i=0;i<sizeof(ccodes);i++){
      if(*p==ccodes[i]||tolower(*p)==ccodes[i])
	break;
    }
    if(i>=sizeof(ccodes))
      return 0;
    op->cc|=i;
    return 1;
  }
  if((p[0]=='r'||p[0]=='R')&&p[1]>='0'&&p[1]<='9'&&len==2){
    op->type=OP_REG;
    op->reg=p[1]-'0';
  }else if((p[0]=='r'||p[0]=='R')&&p[1]=='1'&&p[2]>='0'&&p[2]<='5'&&len==3){
    op->type=OP_REG;
    op->reg=(p[1]-'0')*10+p[2]-'0';
  }else if(*p=='@'){
    p=skip(p+1);
    if(*p=='-'&&p[1]=='-'){
      p=skip(p+2);
      p=skip_reg(p,&op->reg);
      p=skip(p);
      op->type=OP_PREDEC;
    }else{
      p=skip_reg(p,&op->reg);
      p=skip(p);
      if(*p=='+'&&p[1]=='+'){
	p=skip(p+2);
	op->type=OP_POSTINC;
      }else{
	op->type=OP_REGIND;
      }
    }
  }else{
    if(*p=='#'){
      op->reg=1;
      p=skip(p+1);
    }else
      op->reg=0;
    op->offset=parse_expr(&p);
    op->type=OP_ABS;
  }
  if(requires==op->type)
    return 1;
  if(requires==OP_GEN&&op->type>=OP_REG&&op->type<=OP_ABS)
    return 1;
  if(requires==OP_DGEN&&op->type>=OP_REG&&op->type<OP_ABS)
    return 1;

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
      general_error(38);
    else{
      eval_expr(tree,&val,sec,pc);
      val=(val-pc-4)/2;
    }
  }
  return val;
}

static taddr absoffset(expr *tree,section *sec,taddr pc,rlist **relocs,int mode,int roffset,int size)
{
  taddr val;
  if(!eval_expr(tree,&val,sec,pc)){
    taddr addend=val;
    symbol *base;
    if(find_base(tree,&base,sec,pc)!=BASE_OK){
      general_error(38);
      return val;
    }
    add_nreloc_masked(relocs,base,addend,REL_ABS,size,roffset,mode?0x1FFFE:0xFFFF);
    return val;
  }
  if(mode!=0)
    val=(val>>1)&0xFFFF;
  else if(size==16)
    val&=0xffff;
  else
    val&=((1<<size)-1);
  return val;
}

static int translate(instruction *p,section *sec,taddr pc)
{
  int c=p->code;
 
  return c;
}

/* Convert an instruction into a DATA atom including relocations,
   if necessary. */
dblock *eval_instruction(instruction *p,section *sec,taddr pc)
{
  dblock *db=new_dblock();
  int opcode,c,osize;
  unsigned int code,addr1,addr2,aflag=0;
  unsigned char *d;
  rlist *relocs=0;

  c=translate(p,sec,pc);
  
  db->size=osize=instruction_size(p,sec,pc);
  db->data=mymalloc(db->size);

  opcode=mnemonics[c].ext.opcode;

  code=opcode<<12;
  if(p->op[0]){
    int of=6;
    if(opcode==14)
      of=0;
    if(p->op[0]->type==OP_ABS){
      code|=15<<(of+2);
      code|=(OP_POSTINC-1)<<of;
      addr1=absoffset(p->op[0]->offset,sec,pc,&relocs,p->op[0]->reg,16,16);
      aflag=1;
    }else if(p->op[0]->type==OP_REL){
      code|=15<<(of+2);
      code|=(OP_POSTINC-1)<<of;
      addr1=reloffset(p->op[0]->offset,sec,pc);
      aflag=1;
    }else{
      code|=p->op[0]->reg<<(of+2);
      code|=(p->op[0]->type-1)<<of;
    }
  }
  if(mnemonics[c].ext.encoding==0){
    if(p->op[1]){
      if(p->op[1]->type==OP_ABS){
	code|=15<<2;
	code|=(OP_POSTINC-1)<<0;
	addr2=absoffset(p->op[1]->offset,sec,pc,&relocs,p->op[1]->reg,16,16);
	aflag|=2;
      }else if(p->op[1]->type==OP_REL){
	/* case should not exist */
	code|=15<<2;
	code|=(OP_POSTINC-1)<<0;
	addr2=reloffset(p->op[1]->offset,sec,pc);
      }else{
	code|=p->op[1]->reg<<2;
	code|=(p->op[1]->type-1)<<0;
      }
    }
  }else{
    code|=(mnemonics[c].ext.encoding-1)<<4;
    if(p->op[1])
      code|=p->op[1]->cc<<0;
  }

  d=db->data;
  *d++=code;
  *d++=code>>8;

  if(aflag&1){
    *d++=addr1;
    *d++=addr1>>8;
  }
  if(aflag&2){
    *d++=addr2;
    *d++=addr2>>8;
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
  val=absoffset(op->offset,sec,pc,&new->relocs,op->reg,0,bitsize);
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
  int sz=2;

  //int c=translate(p,sec,pc),add=0;

  if(p->op[0]&&(p->op[0]->type==OP_ABS||p->op[0]->type==OP_REL))
    sz+=2;
  if(p->op[1]&&(p->op[1]->type==OP_ABS||p->op[1]->type==OP_REL))
    sz+=2;
  return sz;
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
  return 1;
}

/* return true, if the passed argument is understood */
int cpu_args(char *p)
{
  return 0;
}

/* parse cpu-specific directives; return pointer to end of
   cpu-specific text */
char *parse_cpu_special(char *s)
{
  return s;
}
