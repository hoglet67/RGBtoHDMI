/* cpu.c example cpu-description file */
/* (c) in 2013 by Volker Barthelmann */

#include "vasm.h"

char *cpu_copyright="VideoCore IV cpu backend 0.1 (c) in 2013 Volker Barthelmann";
char *cpuname="vidcore";

mnemonic mnemonics[]={
#include "opcodes.h"
};

int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);

int bitsperbyte=8;
int bytespertaddr=4;

static char *ccs[]={"eq","ne","cs","cc","mi","pl","vs","vc",
		    "hi","ls","ge","lt","gt","le","???","f"};

static char *vflags[]={"all","none","ifz","ifnz","ifn","ifnn","ifc","ifnc"};

static char *accs[]={
  "???", "sub", "wba", "???", "clra", "???", "???", "???",
  "sign", "???", "???", "???", "???", "???", "???", "???",
  "high", "???", "???", "???", "???", "???", "???", "???",
  "???", "???", "???", "???", "???", "???", "???", "???",
  "uadd", "usub", "uacc", "udec", "???", "???", "???", "???",
  "sadd", "ssub", "sacc", "sdec", "???", "???", "???", "???",
  "uaddh", "usubh", "uacch", "udech", "???", "???", "???", "???",
  "saddh", "ssubh", "sacch", "sdech", "???", "???", "???", "???"};

static char *sru[]={
  "sumu", "sums", "???", "imin", "???", "imax", "???", "max"};

static size_t oplen(int op)
{
  if(op<=EN_MEMDISP16)
    return 2;
  else if(op<=EN_ARITHI32)
    return 4;
  else if(op<=EN_VARITHI48)
    return 6;
  else if(op<=EN_ADDCMPB64)
    return 8;
  else if(op<=EN_VARITHI80)
    return 10;
  ierror(0);
  return 0;
}

static char *skip_reg(char *s,int *reg)
{
  int r=-1;
  char* orig=s;
  if(s[0]=='p'&&s[1]=='c'){
    *reg=31;
    return s+2;
  }
  if(s[0]=='s'&&s[1]=='r'){
    *reg=30;
    return s+2;
  }
  if(s[0]=='l'&&s[1]=='r'){
    *reg=26;
    return s+2;
  }
  if(s[0]=='s'&&s[1]=='p'){
    *reg=25;
    return s+2;
  }
  if(*s!='r'){
    return s;
  }
  s++;
  if(*s<'0'||*s>'9'){
    return orig;
  }
  r=*s++-'0';
  if(*s>='0'&&*s<='9')
    r=10*r+*s++-'0';
  if(isalnum(*s)||*s=='_'){
    return orig;
  }
  *reg=r;
  return s;
}

int parse_operand(char *p,int len,operand *op,int req)
{
  int r;
  char *s,*start=p;

  op->type=-1;
  p=skip(p);

  if(req>=OP_REG&&req<=OP_LR){
    s=skip_reg(p,&r);
    if(s!=p){
      if((req==OP_REG&&r<=31)||
	 (req==OP_SREG&&r<=15)||
	 (req==OP_PC&&r==31)||
	 (req==OP_LR&&r==26)||
	 (req==OP_MREG&&(r==0||r==6||r==16||r==24))){
	op->type=OP_REG;
	op->reg=r;
      }else
	return 0;
    }else
      return 0;
    p=skip(s);
  }else if(req==OP_MREG){
    int first;
    s=skip_reg(p,&r);
    if(s==p)
      return 0;
    if(r==0)
      op->dreg=0;
    else if(r==6)
      op->dreg=1;
    else if(r==16)
      op->dreg=2;
    else if(r==24)
      op->dreg=3;
    else
      return 0;
    s=skip(s);
    if(*s!='-')
      return 0;
    p=skip(s+1);
    first=r;
    s=skip_reg(p,&r);
    if(s==p) 
      return 0;
    p=skip(s);
    op->reg=(r-first)&31;
  }else if(req==OP_IND){
    p=skip(p);
    if(*p!='(') return 0;
    p=skip(p+1);
    s=skip_reg(p,&r);
    if(s==p) return 0;
    if(r>15) return 0;
    p=skip(s);
    op->reg=r;
    if(*p!=')') return 0;
    p=skip(p+1);
    op->type=OP_IND;
  }else if(req==OP_VIND){
    int nodisp=0;
    p=skip(p);
    if(*p=='('){
      char *m;
      m=s=skip(p+1);
      s=skip_reg(s,&r);
      if((m!=s)||(*s=='0')) nodisp=1;
    }
    if(!nodisp){
      op->offset=parse_expr(&p);
      if(!op->offset) return 0;
    }else
      op->offset=0;
    if(*p!='(') return 0;
    p=skip(p+1);
    s=skip_reg(p,&r);
    if (s==p){
      /* Special case: Allow '0' instead of a register */
      if (*p=='0'){
	p=skip(p+1);
	op->reg=15;
      }
      else return 0;
    }else{
      if(r>14) return 0;
      p=skip(s);
      op->reg=r;
    }
    op->dreg=15;
    if(*p=='+'&&p[1]=='='){
      p=skip(p+2);
      s=skip_reg(p,&r);
      if(s==p) return 0;
      if(r>14) return 0;
      p=skip(s);
      op->dreg=r;
    }
    if(*p!=')') return 0;
    p=skip(p+1);
    op->type=OP_IND;
  }else if(req==OP_PREDEC){
    p=skip(p);
    if(p[0]!='-'||p[1]!='-') return 0;
    p=skip(p+2);
    if(*p!='(') return 0;
    p=skip(p+1);
    s=skip_reg(p,&r);
    if(s==p) return 0;
    p=skip(s);
    if(*p!=')') return 0;
    p=skip(p+1);
    op->reg=r;
    op->type=OP_PREDEC;
  }else if(req==OP_POSTINC){
    p=skip(p);
    if(*p!='(') return 0;
    p=skip(p+1);
    s=skip_reg(p,&r);
    if(s==p) return 0;
    p=skip(s);
    if(*p!=')') return 0;
    p=skip(p+1);
    if(*p++!='+') return 0;
    if(*p!='+') return 0;
    p=skip(p+1);
    op->reg=r;
    op->type=OP_POSTINC;
  }else if(req==OP_REGIND){
    p=skip(p);
    if(*p!='(') return 0;
    p=skip(p+1);
    s=skip_reg(p,&op->reg);
    if(s==p) return 0;  
    p=skip(s);
    if(*p!=',') return 0;
    p=skip(p+1);
    s=skip_reg(p,&op->dreg);
    if(s==p) return 0;
    p=skip(s);
    if(*p!=')') return 0;
    op->type=OP_REGIND;
    p=skip(p+1);
  }else if(req==OP_IMMIND||
	   req==OP_IMMINDSP||
	   req==OP_IMMINDSD||
	   req==OP_IMMINDR0||
	   req==OP_IMMINDS||
	   req==OP_IMMINDPC){
    op->offset=0;
    if(*p=='('){
      char *p2;
      p2=skip(p+1);
      s=skip_reg(p2,&r);
      if(s!=p2){
	p=skip(s);
	if(*p==')'){
	  p=skip(p+1);
	  op->offset=number_expr(0);
	}
      }
    }
    if(!op->offset){
      op->offset=parse_expr(&p);
      if(!op->offset) return 0;
      p=skip(p);
      if(*p!='(') return 0;
      p=skip(p+1);
      s=skip_reg(p,&r);
      if(s==p) return 0;
      p=skip(s);
      if(*p!=')') return 0;
    }
    if(req==OP_IMMINDSP&&r!=25) return 0;
    if(req==OP_IMMINDPC&&r!=31) return 0;
    if(req==OP_IMMINDSD&&r!=24) return 0;
    if(req==OP_IMMINDR0&&r!=0) return 0;
    if(req==OP_IMMINDS&&r>15) return 0;
    op->reg=r;
    op->type=OP_IMMIND;
    p=skip(p+1);
  }else if(req>=OP_IMM4&&req<=OP_IMM32&&skip_reg(p,&r)==p){
    op->type=OP_IMM32;
    op->offset=parse_expr(&p);
    simplify_expr(op->offset);
    p=skip(p);
  }else if((req==OP_ABS||req==OP_REL)&&skip_reg(p,&r)==p){
    op->type=req;
    op->offset=parse_expr(&p);
    simplify_expr(op->offset);
    p=skip(p);
  }else if(req==OP_VREG||req==OP_VREGM||req==OP_VREGMM||req==OP_VREGA80){
    s=skip_reg(p,&r);
    op->offset=0;
    if(s!=p){
      if (*s=='+'||*s=='-'){
        if(req==OP_VREG) return 0;
        op->offset=parse_expr(&s);
      }
      p=skip(s);
      op->type=OP_VREG;
      if(req==OP_VREG){
	op->reg=(0xE<<6)|r;
	op->dreg=0x3c;
      }else{
	op->reg=(0xE<<6);
	op->dreg=r<<2;
      }
    }else if(*p=='-'){
      p=skip(p+1);
      op->type=OP_VREG;
      op->reg=(0xF<<6);
      op->dreg=0x3c;
    }else{
      int size,vert,x,y,inc=15<<2;
      expr *e;
      if(*p=='H'||*p=='h')
	vert=0;
      /* Disable vertical registers in 48-bit instructions for now */
      else if((req!=OP_VREG)&&(*p=='V'||*p=='v'))
	vert=1;
      else
	return 0;
      p=skip(p+1);
      if(*p=='X'||*p=='x'){
	size=2;
	p=skip(p+1);
      }else if(*p=='1'&&p[1]=='6'){
	size=2;
	p=skip(p+2);
      }else if(*p=='Y'||*p=='y'){
	size=4;
	p=skip(p+1);
      }else if(*p=='3'&&p[1]=='2'){
	size=4;
	p=skip(p+2);
      }else if(*p=='8'){
	size=1;
	p=skip(p+1);
      }else{
	size=1;
	p=skip(p);
      }
      if(*p!='(') return 0;
      p=skip(p+1);
      e=parse_expr(&p);
      if(e->type!=NUM) return 0;
      y=e->c.val;
      p=skip(p);
#if 1
      if(req!=OP_VREG&&*p=='+'){
	p++;
	if(*p!='+') return 0;
	if(vert) {
	  cpu_error(4);
	  return 0;
	}
	p=skip(p+1);
	inc|=2;
      }
#endif
      if(*p!=',') return 0;
      p=skip(p+1);
      if((*p=='c')||(*p=='C'))
      {
	p++;
	if (*p!='b'&&*p!='B') return 0;
	p++;
	if (*p!='+') return 0;
	inc|=1;
      }
      e=parse_expr(&p);
      if(e->type!=NUM) return 0;
      x=e->c.val;
      p=skip(p);
#if 1
      if(req!=OP_VREG&&*p=='+'){
	p++;
	if(*p!='+') return 0;
	if(!vert) {
	  cpu_error(4);
	  return 0;
	}
	p=skip(p+1);
	inc|=2;
      }
#endif
      if(*p!=')') return 0;
      p=skip(p+1);
      if(req!=OP_VREG&&*p=='*'){
	p=skip(p+1);
	inc|=1;
      }
      if(req!=OP_VREG&&*p=='+'){
	p=skip(p+1);
	s=skip_reg(p,&r);
	if(s==p) return 0;
	p=skip(s);
	inc&=~0x3c;
	inc|=(r<<2);
      }
      if(req==OP_VREGA80){
	if(size==1){
	  op->reg=(vert<<6)|((x&48)<<3);
	}else if(size==2){
	  if(x&16) return 0;
	  op->reg=(vert<<6)|(8<<6)|((x&32)<<2);
	}else{
	  if(x&48) return 0;
	  op->reg=(vert<<6)|(12<<6);
	}
	if(y<0||y>63) return 0;
	op->reg|=y&63;
	inc|=(x&15)<<6;
      }else{
	if(vert){
	  if(size==1){
	    op->reg=(1<<6)|((x&48)<<3);
	  }else if(size==2){
	    if(x&16) return 0;
	    op->reg=(9<<6)|((x&32)<<2);
	  }else{
	    if(x&48) return 0;
	    op->reg=13<<6;
	  }
	  if(y<0||y>63||(y&15)) return 0;
	  op->reg|=(y&48)|(x&15);
	}
	else{
	  if(size==1){
	    if(x&15) return 0;
	    op->reg=(x&48)<<3;
	  }else if(size==2){
	    if(x&31) return 0;
	    op->reg=(8<<6)|((x&32)<<2);
	  }else{
	    if(x) return 0;
	    op->reg=12<<6;
	  }
	if(y<0||y>63) return 0;
	op->reg|=y&63;
	}
      }
      op->dreg=inc;
      op->type=OP_VREG;
    }
  }else
    return 0;

  if(req==OP_VREGMM||req==OP_IMM32M||req==OP_VIND){
    int vecmod=0,i;
    while(1){
      if(!strncasecmp(p,"rep",3)&&isspace((unsigned char)p[3])){
	expr *e;
	taddr x;
	p=skip(p+3);
	if((*p=='r')&&(p[1]=='0')&&!isdigit(p[2])){
	  p=skip(p+2);
	  vecmod|=7<<16;
	  i=0;
	}else{
	  e=parse_expr(&p);
	  if(e->type!=NUM) return 0;
	  x=e->c.val;
	  for(i=0;i<8;i++){
	    if((1<<i)==x){
	      vecmod|=i<<16;
	      break;
	    }
	  }
	}
	if(i>=8) return 0;
	p=skip(p);
	continue;
      }
      for(i=0;i<sizeof(vflags)/sizeof(vflags[0]);i++){
	if((!strncasecmp(p,vflags[i],strlen(vflags[i])))&&
	   (!isalpha(p[strlen(vflags[i])]))){
	  if(vecmod&0x380) return 0;
	  vecmod|=i<<7;
	  p=skip(p+strlen(vflags[i]));
	  continue;
	}
      }
      for (i=0;i<sizeof(sru)/sizeof(sru[0]);i++){
	if(!strncasecmp(p,sru[i],strlen(sru[i]))){
	  p=skip(p+strlen(sru[i]));
	  p=skip_reg(p,&r);
	  if((r<0)||(r>7)) return 0;
	  vecmod|=(1<<6)|(i<<3)|r;
	  continue;
	}
      }
      for (i=0;i<sizeof(accs)/sizeof(accs[0]);i++){
	if((!strncasecmp(p,accs[i],strlen(accs[i])))&&
	   (!isalpha(p[strlen(accs[i])]))){
	  if(vecmod&0x40) return 0;
	  vecmod|=i;
	  p=skip(p+strlen(accs[i]));
	  continue;
	}
      }
      if(!strncasecmp(p,"ena",3)){
	if(vecmod&0x40) return 0;
	p=skip(p+3);
	vecmod|=32;
      }
      if(!strncasecmp(p,"setf",4)){
	p=skip(p+4);
	vecmod|=1<<20;
	continue;
      }
      break;
    }

    op->vecmod=vecmod;
  }

  if(p>=start+len)
    return 1;
  else
    return 0;
}

static taddr dooffset(int rel,expr *tree,section *sec,taddr pc,rlist **relocs,int roffset,int size,taddr mask)
{
  taddr val;
  if(!eval_expr(tree,&val,sec,pc)){
    taddr addend=val;
    symbol *base;
    if(find_base(tree,&base,sec,pc)!=BASE_OK){
      general_error(38);
      return val;
    }
    if(rel==REL_PC){
      val-=pc;
      addend+=roffset/8;
    }
    if(rel!=REL_PC||!LOCREF(base)||base->sec!=sec){
      add_nreloc_masked(relocs,base,addend,rel,size,roffset,mask);
      return 0;
    }
  }
#if 0
  if(val<-(1<<(size-1))||val>((1<<(size-1))-1))
    cpu_error(1,size);
#endif
  return val&mask;
}

static taddr reloffset(expr *tree,section *sec,taddr pc,rlist **relocs,int roffset,int size,taddr mask)
{
  /* taddr mask=size==32?0xffffffff:((((taddr)1)<<size)-1);*/
  return dooffset(REL_PC,tree,sec,pc,relocs,roffset,size,mask);
}

static taddr absoffset(expr *tree,section *sec,taddr pc,rlist **relocs,int roffset,int size)
{
  /* taddr mask=size==32?0xffffffff:((((taddr)1)<<size)-1);*/
  return dooffset(REL_ABS,tree,sec,pc,relocs,roffset,size,0xffffffff);
}

static taddr absval(expr *tree,section *sec,taddr pc,int bits,int sgn)
{
  taddr val;
  if(!eval_expr(tree,&val,sec,pc))
    cpu_error(0);
  if(!sgn){
    if(val<0||val>=(1<<bits))
      cpu_error(1,bits);
  }else{
    if(val<-(1<<(bits-1))||val>=(1<<(bits-1)))
      cpu_error(1,bits);
  }
  return val&((1<<bits)-1);
}

static int chkval(expr *tree,section *sec,taddr pc,int bits,int sgn)
{
  taddr val;
  if(!eval_expr(tree,&val,sec,pc))
    cpu_error(0);
  if(!sgn){
    if(val<0||val>=(1<<bits))
      return 0;
  }else{
    if(val<-(1<<(bits-1))||val>=(1<<(bits-1)))
      return 0;
  }
  return 1;
}

/* replace instruction by one with encoding enc */
static int replace(int c,int enc)
{
  int i=c+1;
  while(!strcmp(mnemonics[c].name,mnemonics[i].name)){
    if(mnemonics[i].ext.encoding==enc){
      c=i;
      break;
    }
    i++;
  }
  if(c!=i) ierror(0);  
  return c;
}
static int translate(instruction *p,section *sec,taddr pc)
{
  int c=p->code,e=mnemonics[c].ext.encoding;
  taddr val;

  if(p->qualifiers[0]){
    /* extend to larger variants if ccs are required */
    if(e==EN_MEMDISP16)
      c=replace(c,EN_MEMDISP32);
    if(e==EN_ARITHR16)
      c=replace(c,EN_ARITHR32);
    if(e==EN_ARITHI16)
      c=replace(c,EN_ARITHI32);
  }
  if(e==EN_ARITHI32){
    if(p->op[2]){
      if(!chkval(p->op[2]->offset,sec,pc,6,1)&&(!strcmp("add",mnemonics[c].name)||!strcmp("sub",mnemonics[c].name)))
	c=replace(c,EN_ADD48);
    }else{
      if((mnemonics[c].ext.code<32)&&(!chkval(p->op[1]->offset,sec,pc,6,1)))
	c=replace(c,EN_ARITHI48);
    }
  }
  if(e==EN_ARITHI16){
    if(!chkval(p->op[1]->offset,sec,pc,5,0))
      c=replace(c,EN_ARITHI48);
  }
  if(e==EN_RBRANCH16){
    symbol *base;
    if(find_base(p->op[0]->offset,&base,sec,pc)!=BASE_OK||!LOCREF(base)||base->sec!=sec)
      c=replace(c,EN_RBRANCH32);
    else{
      eval_expr(p->op[0]->offset,&val,sec,pc);
      val-=pc;
      if(val>126||val<-128)
	c=replace(c,EN_RBRANCH32);
    }
  }
  if(e==EN_ADDCMPB32){
    eval_expr(p->op[3]->offset,&val,sec,pc);
    val-=pc;
    if(val>1022||val<-1024)
      c+=4;
    else if(p->op[2]->type!=OP_REG&&(val>254||val<-256))
      c+=4;
  }
  if((e==EN_MEMDISP16||e==EN_MEMDISP32||e==EN_MEM12DISP32||e==EN_MEM16DISP32)){
    if(!eval_expr(p->op[1]->offset,&val,sec,pc)){
      c=replace(c,EN_MEM48);
    }else{
      if(e==EN_MEMDISP16&&(val<0||val>54||(val&3))){
	if(val<=4095&&val>=-4096)
	  c=replace(c,EN_MEM12DISP32);
	else
	  c=replace(c,EN_MEM48);
      }
    }
  }
  /* todo */
  return c;
}

/* Convert an instruction into a DATA atom including relocations,
   if necessary. */
dblock *eval_instruction(instruction *p,section *sec,taddr pc)
{
  dblock *db=new_dblock();
  int c,sz,enc,cc,ww,m=0;
  unsigned long code,code2,code3;
  rlist *relocs=0;

  if(p->qualifiers[0]){
    int i;
    for(i=0;i<15;i++){
      if(!strcmp(p->qualifiers[0],ccs[i])){
	cc=i;
	break;
      }
    }
    if(i>15) ierror(0);
  }else
    cc=14;

  c=translate(p,sec,pc);
  enc=mnemonics[c].ext.encoding;

  db->size=sz=oplen(enc);
  db->data=mymalloc(db->size);

  code=mnemonics[c].ext.code;

  switch(enc){
  case EN_ARITHR16:
    if(cc!=14) cpu_error(3);
    code=0x40000000|(code<<24);
    code|=(p->op[0]->reg<<16)|(p->op[1]->reg<<20);
    break;
  case EN_ARITHI16:
    if(cc!=14) cpu_error(3);
    code=0x60000000|((code>>1)<<25);
    code|=(p->op[0]->reg<<16)|(absval(p->op[1]->offset,sec,pc,5,0)<<20);
    break;
  case EN_FIX16:
    if(cc!=14) cpu_error(3);
    break;
  case EN_IBRANCH16:
    if(cc!=14) cpu_error(3);
    code|=(p->op[0]->reg<<16);
    break;
  case EN_RBRANCH16:
    code=0x18000000;
    code|=cc<<23;
    code|=((reloffset(p->op[0]->offset,sec,pc,&relocs,0,7,0xfe)>>1)&127)<<16;
    break;
  case EN_MREG16:
    if(cc!=14) cpu_error(3);
    code|=(p->op[0]->dreg<<21)|(p->op[0]->reg<<16);
    break;
  case EN_RBRANCH32:
    if(code==0x90000000)
      code|=cc<<24;
    else
      code|=((reloffset(p->op[0]->offset,sec,pc,&relocs,8,4,0xF000000)>>24)&0xf)<<24;
    code|=((reloffset(p->op[0]->offset,sec,pc,&relocs,16,16,0x1fffe)>>1)&0x7fffff);
    code|=((reloffset(p->op[0]->offset,sec,pc,&relocs,0,7,0xfe0000)>>17)&0x7f)<<16;
    break;
  case EN_FUNARY32:
    code=0xC0000040|(code<<21);
    code|=p->op[0]->reg<<16;
    code|=p->op[1]->reg<<11;
    code|=cc<<7;
    break;
  case EN_ARITHR32:
    code=0xC0000000|(code<<21);
    code|=p->op[0]->reg<<16;
    if(p->op[2]){
      code|=p->op[1]->reg<<11;
      code|=p->op[2]->reg;
    }else{
      code|=p->op[0]->reg<<11;
      code|=p->op[1]->reg;
    }      
    code|=cc<<7;
    break;
  case EN_ARITHI32:
    code=0xC0000040|(code<<21);
    code|=p->op[0]->reg<<16;
    if(p->op[2]){
      code|=p->op[1]->reg<<11;
      code|=absval(p->op[2]->offset,sec,pc,6,1);
    }else{
      code|=p->op[0]->reg<<11;
      code|=absval(p->op[1]->offset,sec,pc,6,1);
    }
    code|=cc<<7;
    break;
  case EN_ADDCMPB64:
    /* Large offset: invert CC, place a long branch after
       so we really generate the following:
         addcmpb<~cc> d, a, b, skip
         b <destination>
       skip:
         <rest of code> */
    m=1;
    if(cc&1) cc--; else cc++;
  case EN_ADDCMPB32:
    code=0x80000000|(code<<14);
    code|=cc<<24;
    code|=p->op[0]->reg<<16;
    if(code&0x4000){
      code|=absval(p->op[1]->offset,sec,pc,4,1)<<20;
    }else{
      code|=p->op[1]->reg<<20;
    }
    if(code&0x8000){
      code|=absval(p->op[2]->offset,sec,pc,6,0)<<8;
      if(m)
	code|=4;
      else
	code|=((reloffset(p->op[3]->offset,sec,pc,&relocs,16,9,0x1ff)>>1)&0xff);
    }else{
      code|=p->op[2]->reg<<10;
      if(m)
	code|=4;
      else
	code|=((reloffset(p->op[3]->offset,sec,pc,&relocs,16,11,0x7ff)>>1)&0x3ff);
    }
    if(m){
      code2=0x90000000|(14<<24);
      code2|=((reloffset(p->op[3]->offset,sec,pc+4,&relocs,48,16,0x1fffe)>>1)&0x7fffff);
      code2|=((reloffset(p->op[3]->offset,sec,pc+4,&relocs,32,7,0xfe0000)>>17)&0x7f)<<16;
    }
    break;
  case EN_ARITHI48:
    if(cc!=14) cpu_error(3);
    code=0xE8000000|(code<<21);
    code|=p->op[0]->reg<<16;
    code2=absoffset(p->op[1]->offset,sec,pc,&relocs,16,32);
    break;
  case EN_ADD48:
    if(cc!=14) cpu_error(3);
    if(p->op[2])
      code2=absoffset(p->op[2]->offset,sec,pc,&relocs,16,32);
    else
      code2=absoffset(p->op[1]->offset,sec,pc,&relocs,16,32);
    /* pseudo sub */
    if(code==6)
      code2=-code2;
    code=0xEC000000;
    code|=p->op[0]->reg<<16;
    if(p->op[2])
      code|=p->op[1]->reg<<21;
    else
      code|=p->op[0]->reg<<21;
    break;
  case EN_MEMREG16:
    if(cc!=14) cpu_error(3);
    code=0x08000000|(code<<24);
    code|=p->op[0]->reg<<16;
    code|=p->op[1]->reg<<20;
    ww=0; /*FIXME*/
    code|=ww<<25;
    break;
  case EN_MEMSTACK16:
    if(cc!=14) cpu_error(3);
    code=0x04000000|(code<<25);
    code|=p->op[0]->reg<<16;
    code|=(absval(p->op[1]->offset,sec,pc,7,1)>>2)<<20;
    break;
  case EN_MEMDISP16:
    if(cc!=14) cpu_error(3);
    code=0x20000000|(code<<28);
    code|=p->op[0]->reg<<16;
    code|=(absval(p->op[1]->offset,sec,pc,6,0)>>2)<<24;
    code|=p->op[1]->reg<<20;
    break;
  case EN_LEA16:
    if(cc!=14) cpu_error(3);
    code=0x10000000;
    code|=p->op[0]->reg<<16;
    code|=(absval(p->op[1]->offset,sec,pc,8,1)>>2)<<21;
    break;
  case EN_LEA48:
    if(cc!=14) cpu_error(3);
    code=0xE5000000|(p->op[0]->reg<<16);
    code2=reloffset(p->op[1]->offset,sec,pc,&relocs,16,32,0xffffffff);
    break;
  case EN_MEM48:
    if(cc!=14) cpu_error(3);
    code=0xE6000000|(code<<21);
    code|=p->op[0]->reg<<16;
    if(p->op[1]->reg==31)
      code2=reloffset(p->op[1]->offset,sec,pc,&relocs,16,27,0xffffffff)&0x7ffffff;
    else
      /* TODO: other relocs */
      code2=absoffset(p->op[1]->offset,sec,pc,&relocs,16,27)&0x7ffffff;
    code2|=p->op[1]->reg<<27;
    break;
  case EN_MEMREG32:
    code=0xA0000000|(code<<21);
    code|=p->op[0]->reg<<16;
    code|=p->op[1]->reg<<11;
    code|=p->op[1]->dreg;
    code|=cc<<7;
    break;
  case EN_MEMDISP32:
    code=0xA0000040|(code<<21);
    code|=p->op[0]->reg<<16;
    code|=p->op[1]->reg<<11;
    code|=absval(p->op[1]->offset,sec,pc,5,1);
    code|=cc<<7;
    break;
  case EN_MEMPREDEC:
  case EN_MEMPOSTINC:
    code<<=21;
    if(p->op[1]->type==OP_POSTINC)
      code|=0xA5000000;
    else
      code|=0xA4000000;
    code|=p->op[0]->reg<<16;
    code|=p->op[1]->reg<<11;
    code|=cc<<7;
    break;
  case EN_MEM12DISP32:
    if(cc!=14) cpu_error(3);
    code=0xA2000000|(code<<21);
    code|=p->op[0]->reg<<16;
    code|=p->op[1]->reg<<11;
    code|=absval(p->op[1]->offset,sec,pc,12,1)&0x7ff;
    code|=(absval(p->op[1]->offset,sec,pc,12,1)&0x800)<<13;
    break;
  case EN_MEM16DISP32:
    if(cc!=14) cpu_error(3);
    code|=p->op[0]->reg<<16;
    if((code&0xff000000)==0xaa000000)
      code|=reloffset(p->op[1]->offset,sec,pc,&relocs,16,16,0xffff)&0xffff;
    else
      code|=absval(p->op[1]->offset,sec,pc,16,1);
    break;
  case EN_VLOAD48:
    code=0xF0000000|(code<<19);
    code2=(p->op[0]->reg<<22)|(7<<7)|p->op[1]->reg;
    break;
  case EN_VSTORE48:
    code=0xF0000000|(code<<19);
    code2=(p->op[0]->reg<<12)|(7<<7)|p->op[1]->reg;
    break;
  case EN_VREAD48:
    code=0xF0000000|(code<<19);
    code2=(p->op[0]->reg<<22)|p->op[1]->reg;
    break;
  case EN_VWRITE48:
    code=0xF0000000|(code<<19);
    code2=(p->op[0]->reg<<12)|p->op[1]->reg;
    break;
  case EN_VREADI48:
    code=0xF0000000|(code<<19);
    code2=(p->op[0]->reg<<22)|absval(p->op[1]->offset,sec,pc,6,0)|(1<<10);
    break;
  case EN_VWRITEI48:
    code=0xF0000000|(code<<19);
    code2=(p->op[0]->reg<<12)|absval(p->op[1]->offset,sec,pc,6,0)|(1<<10);
    break;
  case EN_VARITHR48:
    if(code<48) code|=64;
    code=0xF4000000|(code<<19);
    if(p->op[2]){
      code2=(p->op[0]->reg<<22)|(p->op[1]->reg<<12)|p->op[2]->reg;
    }else{
      code2=(p->op[0]->reg<<22)|p->op[1]->reg;
    }
    break;
  case EN_VARITHI48:
    if(code<48) code|=64;
    code=0xF4000000|(code<<19);
    if(p->op[2]){
      code2=(p->op[0]->reg<<22)|(p->op[1]->reg<<12)|absval(p->op[2]->offset,sec,pc,6,0)|(1<<10);
    }else{
      code2=(p->op[0]->reg<<22)|absval(p->op[1]->offset,sec,pc,6,0)|(1<<10);
    }
    break;
  /* TODO: Check these! */
  case EN_VLOAD80:
    code=0xF8000000|(code<<19)|(p->op[1]->vecmod&0x70000);
    code2=(p->op[0]->reg<<22);
    code3=(p->op[1]->reg<<2)|(p->op[0]->dreg<<26)|((p->op[1]->dreg)<<22);
    if(p->op[1]->offset){
      code2|=(absval(p->op[1]->offset,sec,pc,16,1)&0x7f);
      code3|=(absval(p->op[1]->offset,sec,pc,16,1)>>7)&3;
      code3|=((absval(p->op[1]->offset,sec,pc,16,1)>>9)&0x7f)<<6;
    }
    break;
  case EN_VSTORE80:
    code=0xF8000000|(code<<19)|(p->op[1]->vecmod&0x70000);
    code2=(p->op[0]->reg<<12);
    code3=(p->op[1]->reg<<2)|(p->op[0]->dreg<<20)|((p->op[1]->dreg)<<28);
    if(p->op[1]->offset){
      code2|=(absval(p->op[1]->offset,sec,pc,16,1)&0x7f);
      code3|=(absval(p->op[1]->offset,sec,pc,16,1)>>7)&3;
      code3|=((absval(p->op[1]->offset,sec,pc,16,1)>>9)&0x7f)<<6;
    }
    break;
  case EN_VREAD80:
    if(p->op[1]->offset){
      p->op[1]->reg|=(absval(p->op[1]->offset,sec,pc,9,1)&0x7f);
      p->op[1]->dreg|=(absval(p->op[1]->offset,sec,pc,9,1)>>7)&3;
    }
    code=0xF8000000|(code<<19)|(p->op[1]->vecmod&0x70000);
    code2=(p->op[0]->reg<<22)|(p->op[1]->reg);
    if(p->op[1]->vecmod&(1<<20)) code2|=(1<<11);
    code3=(p->op[0]->dreg<<26)|(p->op[1]->dreg);
    code3|=(p->op[1]->vecmod&0x3fff)<<6;
    break;
  case EN_VWRITE80:
    if(p->op[1]->offset){
      p->op[1]->reg|=(absval(p->op[1]->offset,sec,pc,9,1)&0x7f);
      p->op[1]->dreg|=(absval(p->op[1]->offset,sec,pc,9,1)>>7)&3;
    }
    code=0xF8000000|(code<<19)|(p->op[1]->vecmod&0x70000);
    code2=(p->op[0]->reg<<12)|(p->op[1]->reg);
    if(p->op[1]->vecmod&(1<<20)) code2|=(1<<11);
    code3=((p->op[0]->dreg&0x3f)<<20)|((p->op[0]->dreg&0x3c0)<<10)|(p->op[1]->dreg);
    code3|=(p->op[1]->vecmod&0x3fff)<<6;
    break;
  case EN_VREADI80:
    if(p->op[1]->offset){
      p->op[1]->reg|=(absval(p->op[1]->offset,sec,pc,9,1)&0x7f);
      p->op[1]->dreg|=(absval(p->op[1]->offset,sec,pc,9,1)>>7)&3;
    }
    code=0xF8000000|(code<<19)|(p->op[1]->vecmod&0x70000);
    if(p->op[1]->vecmod&(1<<20)) code2|=(1<<11);
    code2=(p->op[0]->reg<<22)|(1<<10)|(absval(p->op[1]->offset,sec,pc,16,0)&0x3ff);
    code3=(p->op[0]->dreg<<26)|((absval(p->op[1]->offset,sec,pc,16,0)>>10)&0x3f);
    code3|=(p->op[1]->vecmod&0x3fff)<<6;
    break;
  case EN_VWRITEI80:
    if(p->op[1]->offset){
      p->op[1]->reg|=(absval(p->op[1]->offset,sec,pc,9,1)&0x7f);
      p->op[1]->dreg|=(absval(p->op[1]->offset,sec,pc,9,1)>>7)&3;
    }
    code=0xF8000000|(code<<19)|(p->op[1]->vecmod&0x70000);
    if(p->op[1]->vecmod&(1<<20)) code2|=(1<<11);
    code2=(p->op[0]->reg<<12)|(1<<10)|(absval(p->op[1]->offset,sec,pc,16,0)&0x3ff);
    code3=((p->op[0]->dreg&0x3f)<<20)|((p->op[0]->dreg&0x3c0)<<10)|((absval(p->op[1]->offset,sec,pc,16,0)>>10)&0x3f);
    code3|=(p->op[1]->vecmod&0x3fff)<<6;
    break;
  case EN_VARITHR80:
    if(code<48) code|=64;
    if(p->op[2]){
      if(p->op[2]->offset){
	p->op[2]->reg|=(absval(p->op[2]->offset,sec,pc,9,1)&0x7f);
	p->op[2]->dreg|=(absval(p->op[2]->offset,sec,pc,9,1)>>7)&3;
      }
      code=0xFC000000|(code<<19)|(p->op[2]->vecmod&0x70000);
      code2=(p->op[0]->reg<<22)|(p->op[1]->reg<<12)|(p->op[2]->reg);
      if(p->op[2]->vecmod&(1<<20)) code2|=(1<<11);
      code3=(p->op[0]->dreg<<26)|((p->op[1]->dreg&0x3f)<<20)|((p->op[1]->dreg&0x3c0)<<10)|(p->op[2]->dreg);
      code3|=(p->op[2]->vecmod&0x3fff)<<6;
    }else{
      if(p->op[1]->offset){
	p->op[1]->reg|=(absval(p->op[1]->offset,sec,pc,9,1)&0x7f);
	p->op[1]->dreg|=(absval(p->op[1]->offset,sec,pc,9,1)>>7)&3;
      }
      code=0xFC000000|(code<<19)|(p->op[1]->vecmod&0x70000);
      code2=(p->op[0]->reg<<22)|(p->op[1]->reg);
      if(p->op[1]->vecmod&(1<<20)) code2|=(1<<11);
      code3=(p->op[0]->dreg<<26)|(p->op[1]->dreg);
      code3|=(p->op[1]->vecmod&0x3fff)<<6;
    }
    break;
  case EN_VARITHI80:
    if(code<48) code|=64;
    if(p->op[2]){
      if(p->op[2]->offset){
	p->op[2]->reg|=(absval(p->op[2]->offset,sec,pc,9,1)&0x7f);
	p->op[2]->dreg|=(absval(p->op[2]->offset,sec,pc,9,1)>>7);
      }
      code=0xFC000000|(code<<19)|(p->op[2]->vecmod&0x70000);
      if(p->op[2]->vecmod&(1<<20)) code2|=(1<<11);
      code2=(p->op[0]->reg<<22)|(p->op[1]->reg<<12)|(1<<10)|(absval(p->op[2]->offset,sec,pc,16,0)&0x3ff);
      code3=(p->op[0]->dreg<<26)|((p->op[1]->dreg&0x3f)<<20)|((p->op[1]->dreg&0x3c0)<<10)|((absval(p->op[2]->offset,sec,pc,16,0)>>10)&0x3f);
      code3|=(p->op[2]->vecmod&0x3fff)<<6;
    }else{
      if(p->op[1]->offset){
	p->op[1]->reg|=(absval(p->op[1]->offset,sec,pc,9,1)&0x7f);
	p->op[1]->dreg|=(absval(p->op[1]->offset,sec,pc,9,1)>>7);
      }
      code=0xFC000000|(code<<19)|(p->op[1]->vecmod&0x70000);
      if(p->op[1]->vecmod&(1<<20)) code2|=(1<<11);
      code2=(p->op[0]->reg<<22)|(1<<10)|(absval(p->op[1]->offset,sec,pc,16,0)&0x3ff);
      code3=(p->op[0]->dreg<<26)|((absval(p->op[1]->offset,sec,pc,16,0)>>10)&0x3f);
      code3|=(p->op[1]->vecmod&0x3fff)<<6;
    }
    break;
  default:
    ierror(0);
  }

  if(enc>=EN_VLOAD48&&enc<=EN_VARITHI48){
    db->data[0]=(code>>16)&0xff;
    db->data[1]=(code>>24)&0xff;
    db->data[3]=(code2>>24)&0xff;
    db->data[2]=(code2>>16)&0xff;
    db->data[5]=(code2>>8)&0xff;
    db->data[4]=(code2>>0)&0xff;
  }else if(sz==2){
    db->data[0]=(code>>16)&0xff;
    db->data[1]=(code>>24)&0xff;
  }else if(sz==4){
    db->data[1]=(code>>24)&0xff;
    db->data[0]=(code>>16)&0xff;
    db->data[3]=(code>>8)&0xff;
    db->data[2]=(code)&0xff;
  }else if(sz==6){
    db->data[0]=(code>>16)&0xff;
    db->data[1]=(code>>24)&0xff;
    db->data[5]=(code2>>24)&0xff;
    db->data[4]=(code2>>16)&0xff;
    db->data[3]=(code2>>8)&0xff;
    db->data[2]=(code2>>0)&0xff;
  }
  else if(sz==8){
    db->data[1]=(code>>24)&0xff;
    db->data[0]=(code>>16)&0xff;
    db->data[3]=(code>>8)&0xff;
    db->data[2]=(code)&0xff;
    db->data[5]=(code2>>24)&0xff;
    db->data[4]=(code2>>16)&0xff;
    db->data[7]=(code2>>8)&0xff;
    db->data[6]=(code2)&0xff;
  }else if(sz==10){
    db->data[0]=(code>>16)&0xff;
    db->data[1]=(code>>24)&0xff;
    db->data[3]=(code2>>24)&0xff;
    db->data[2]=(code2>>16)&0xff;
    db->data[5]=(code2>>8)&0xff;
    db->data[4]=(code2>>0)&0xff;
    db->data[7]=(code3>>24)&0xff;
    db->data[6]=(code3>>16)&0xff;
    db->data[9]=(code3>>8)&0xff;
    db->data[8]=(code3>>0)&0xff;
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
    cpu_error(2);
  val=absoffset(op->offset,sec,pc,&new->relocs,0,bitsize);
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
  int c;
  c=translate(p,sec,pc);
  return oplen(mnemonics[c].ext.encoding);
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
  char *merk=s;

  return merk;
}

int set_default_qualifiers(char **q,int *q_len)
/* fill in pointers to default qualifiers, return number of qualifiers */
{
  q[0]="u32";
  q_len[0]=3;
  q[1]="";
  q_len[1]=0;
  return 2;
}

char *parse_instruction(char *s,int *inst_len,char **ext,int *ext_len,
                        int *ext_cnt)
/* parse instruction and save extension locations */
{
  char *inst = s;
  int cnt = 0, l, i;

  while (*s && *s!='.' && !isspace((unsigned char)*s))
    s++;
  l = s - inst;

  if(strncasecmp("divs",inst,l)&&strncmp("vreadacc",inst,l)&&
     strncasecmp("vcmpge",inst,l)){
    for(i=0;i<16;i++){
      int cl=strlen(ccs[i]);
      if(l>cl&&!strncasecmp(s-cl,ccs[i],cl)){
	ext[0]=s-cl;
	ext_len[0]=cl;
	cnt=1;
	l-=cl;
	break;
      }
    }
  }

  *ext_cnt = cnt;

  *inst_len=l;

  return s;
}
