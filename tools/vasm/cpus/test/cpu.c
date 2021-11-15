/* cpu.c example cpu-description file */
/* (c) in 2002 by Volker Barthelmann */

#include "vasm.h"

char *cpu_copyright="vasm test cpu backend (c) in 2002 Volker Barthelmann";

/* example machine.
   valid Registers: R0-R3
   valid extensions: .b/.w (default .w)

   Instruction format:
   XCCCCCCC 11112222 [32bit op1] [32bit op2]

   X:   set if byte-extension
   C:   instruction code
   1/2: operand 1/2 type
        0-3  register
        4-7  register indirect (32bit offset follows)
        8    absolute (32bit value follows)
        9    immediate (32bit value follows)
        Special case for addq: 1111: immediate 0-15
        Special case for bra: 11112222: 0-255 relative offset
*/

char *cpuname="test";
int bitsperbyte=8;
int bytespertaddr=4;

mnemonic mnemonics[]={
  "move",{OP_ALL,OP_REG},{CPU_ALL,0x0},
  "move",{OP_REG,OP_ALL_DEST},{CPU_ALL,0x1},
  "add",{OP_ALL,OP_REG},{CPU_ALL,0x2},
  "add",{OP_REG,OP_ALL_DEST},{CPU_ALL,0x3},
  "add",{OP_IMM32,OP_MEM},{CPU_ALL,0x4},
  "addq",{OP_IMM32,OP_ALL_DEST},{CPU_ALL,0x5},
  "jmp",{OP_ABS,0},{CPU_ALL,0x6},
  "bra",{OP_ABS,0},{CPU_ALL,0x7},
};

int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);


char *parse_instruction(char *s,int *inst_len,char **ext,int *ext_len,
                        int *ext_cnt)
/* parse instruction and save extension locations */
{
  char *inst = s;

  while (*s && *s!='.' && !isspace((unsigned char)*s))
    s++;
  *inst_len = s - inst;
  if (*s =='.') {
    /* extension present */
    ext[*ext_cnt] = ++s;
    while (*s && *s!='.' && !isspace((unsigned char)*s))
      s++;
    ext_len[*ext_cnt] = s - ext[*ext_cnt];
    *ext_cnt += 1;
  }
  return s;
}

int set_default_qualifiers(char **q,int *q_len)
/* fill in pointers to default qualifiers, return number of qualifiers */
{
  q[0] = "w";
  q_len[0] = 1;
  return 1;
}

/* Does not do much useful parsing yet. */
int parse_operand(char *p,int len,operand *op,int requires)
{
  p=skip(p);
  if(len==2&&(p[0]=='r'||p[0]=='R')&&p[1]>='0'&&p[1]<='3'){
    op->type=OP_REG;
    op->basereg=p[1]-'0';
  }else if(p[0]=='#'){
    op->type=OP_IMM32;
    p=skip(p+1);
    op->offset=parse_expr(&p);
  }else{
    int parent=0;
    expr *tree;
    op->type=-1;
    if(*p=='('){
      parent=1;
      p=skip(p+1);
    }
    tree=parse_expr(&p);
    if(!tree)
      return 0;
    p=skip(p);
    if(parent){
      if(*p==','){
	p=skip(p+1);
	if((*p!='r'&&*p!='R')||p[1]<'0'||p[1]>'3'){
	  cpu_error(0);
	  return 0;
	}
	op->type=OP_REGIND;
	op->basereg=p[1]-'0';
	p=skip(p+2);
      }
      if(*p!=')'){
	cpu_error(0);
	return 0;
      }else
	p=skip(p+1);
    }
    if(op->type!=OP_REGIND)
      op->type=OP_ABS;
    op->offset=tree;
  }
  if(requires==op->type)
    return 1;
  if(requires==OP_ALL_DEST&&op->type!=OP_IMM32)
    return 1;
  if(requires==OP_MEM&&OP_ISMEM(op->type))
    return 1;
  if(requires==OP_ALL)
    return 1;
  return 0;
}

/* Return new instruction code, if instruction can be optimized
   to another one. */
static int opt_inst(instruction *p,section *sec,taddr pc)
{
  /* Ganz simples Beispiel. */

  /* add->addq */
  if((p->code==2||p->code==4)&&p->op[0]->type==OP_IMM32){
    taddr val;
    if(eval_expr(p->op[0]->offset,&val,sec,pc)&&val<16)
      return 5;
  }
  /* jmp->bra */
  if(p->code==6){
    expr *tree=p->op[0]->offset;
    if(tree->type==SYM&&tree->c.sym->sec==sec&&LOCREF(tree->c.sym)&&
       tree->c.sym->pc-pc>=-128&&tree->c.sym->pc-pc<=127)
      return 7;
  }
  return p->code;
}

static int operand_code(operand *p)
{
  if(!p)
    return 0;
  if(p->type==OP_REG)
    return p->basereg;
  if(p->type==OP_REGIND)
    return 4+p->basereg;
  if(p->type==OP_ABS)
    return 8;
  if(p->type==OP_IMM32)
    return 9;
  ierror(0);
  return 0;
}
static unsigned char *fill_operand(operand *p,section *sec,taddr pc,unsigned char *d,rlist **relocs,int roffset)
{
  taddr val;
  if(!p||p->type==OP_REG)
    return d;
  /* FIXME: Test for valid operand, create reloc */
  if(!eval_expr(p->offset,&val,sec,pc)){
    symbol *base;
    if (find_base(p->offset,&base,sec,pc)!=BASE_OK)
      general_error(38);
    else
      add_nreloc(relocs,base,0,REL_ABS,16,roffset*8);
  }
  *d++=val>>24;
  *d++=val>>16;
  *d++=val>>8;
  *d++=val;
  return d;
}

/* Convert an instruction into a DATA atom including relocations,
   if necessary. */
dblock *eval_instruction(instruction *p,section *sec,taddr pc)
{
  /* Auch simpel. Fehlerchecks fehlen. */
  size_t size=instruction_size(p,sec,pc);
  dblock *db=new_dblock();
  int c=opt_inst(p,sec,pc);
  unsigned char *d;
  db->size=size;
  d=db->data=mymalloc(size);
  *d=c;
  if(p->qualifiers[0]){
    if(c>5)
      cpu_error(1,p->qualifiers[0]);
    else if(!strcmp(p->qualifiers[0],"b"))
      *d|=128;
    else if(strcmp(p->qualifiers[0],"w"))
      cpu_error(1,p->qualifiers[0]);
  }
  d++;
  if(c==5){
    /* addq */
    taddr val;
    if(!eval_expr(p->op[0]->offset,&val,sec,pc)||val>15)
      cpu_error(0);
    *d=((val<<4)|operand_code(p->op[1]));
    return db;
  }
  if(c==7){
    expr *tree=p->op[0]->offset;
    if(!(tree->type==SYM&&tree->c.sym->sec==sec&&LOCREF(tree->c.sym)&&
	 tree->c.sym->pc-pc>=-128&&tree->c.sym->pc-pc<=127))
      cpu_error(0);
    else
      *d=tree->c.sym->pc-pc;
    return db;
  }

  *d=((operand_code(p->op[0])<<4)|operand_code(p->op[1]));
  d++;
  d=fill_operand(p->op[0],sec,pc,d,&db->relocs,d-db->data);
  d=fill_operand(p->op[1],sec,pc,d,&db->relocs,d-db->data);
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
    cpu_error(2);
  if(!eval_expr(op->offset,&val,sec,pc)&&bitsize!=32)
    general_error(38);
  if(bitsize==8){
    new->data[0]=val;
  }else if(bitsize==16){
    new->data[0]=val>>8;
    new->data[1]=val;
  }else if(bitsize==32){
    fill_operand(op,sec,pc,new->data,&new->relocs,0);
  }
  return new;
}

static taddr opsize(operand *p)
{
  if(!p)
    return 0;
  if(p->type!=OP_REG)
    return 4;
  else
    return 0;
}

/* Calculate the size of the current instruction; must be identical
   to the data created by eval_instruction. */
size_t instruction_size(instruction *p,section *sec,taddr pc)
{
  int c;
  size_t size=2;
  size+=opsize(p->op[0]);
  size+=opsize(p->op[1]);
  c=opt_inst(p,sec,pc);
  if(c==5||c==7){
    /* addq/bra */
    size-=4;
  }
  return size;    
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
