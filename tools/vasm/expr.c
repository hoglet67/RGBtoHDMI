/* expr.c expression handling for vasm */
/* (c) in 2002-2021 by Volker Barthelmann and Frank Wille */

#include "vasm.h"

char current_pc_char='$';
int unsigned_shift;
static char *s;
static symbol *cpc;
static int make_tmp_lab;
static int exp_type;

static expr *expression();


#ifndef EXPSKIP
#define EXPSKIP() s=expskip(s)
static char *expskip(char *s)
{
  s=skip(s);
  if(*s==commentchar) *s='\0';  /* rest of line is commented out */
  return s;
}
#endif

expr *new_expr(void)
{
  expr *new=mymalloc(sizeof(*new));
  new->left=new->right=0;
  return new;
}

expr *make_expr(int type,expr *left,expr *right)
{
  expr *new=mymalloc(sizeof(*new));
  new->left=left;
  new->right=right;
  new->type=type;
  return new;
}

expr *copy_tree(expr *old)
{
  expr *new=0;

  if(old){
    new=make_expr(old->type,copy_tree(old->left),copy_tree(old->right));
    new->c=old->c;
  }
  return new;
}

expr *new_sym_expr(symbol *sym)
{
  expr *new=new_expr();
  new->type=SYM;
  new->c.sym=sym;
  return new;
}

expr *curpc_expr(void)
{
  if(!cpc){
    cpc=new_import(" *current pc dummy*");
    cpc->type=LABSYM;
    cpc->flags|=VASMINTERN|PROTECTED;
  }
  return new_sym_expr(cpc);
}

static void update_curpc(expr *exp,section *sec,taddr pc)
{
  if(exp->c.sym==cpc&&sec!=NULL){
    cpc->sec=sec;
    cpc->pc=pc;
    if(sec->flags&ABSOLUTE)
      cpc->flags|=ABSLABEL;
    else
      cpc->flags&=~ABSLABEL;
  }
}

static expr *primary_expr(void)
{
  expr *new;
  char *m,*name;
  int base;

  if(*s=='('){
    s++;
    EXPSKIP();
    new=expression();
    if(*s!=')')
      general_error(6,')');
    else
      s++;
    EXPSKIP();
    return new;
  }
  if(name=get_local_label(&s)){
    symbol *sym=find_symbol(name);
    if(!sym)
      sym=new_import(name);
    sym->flags|=USED;
    new=(sym->type!=EXPRESSION)?new_sym_expr(sym):copy_tree(sym->expr);
    myfree(name);
    return new;
  }
  m=const_prefix(s,&base);
  if(base!=0){
    char *start=s;
    utaddr val,nval;
    thuge huge;
#if FLOAT_PARSER
    tfloat flt;
#endif
    switch(exp_type){
    case NUM:
      s=m;
      val=0;
      if(base<=10){
        while(*s>='0'&&*s<base+'0'){
          nval=base*val;
          if (nval/base!=val)
            goto hugeval;  /* taddr overflow, read a thuge value instead */
          val=nval+*s++-'0';
        }
#if FLOAT_PARSER
        if(base==10&&(*s=='e'||*s=='E'||(*s=='.'&&*(s+1)>='0'&&*(s+1)<='9')))
          goto fltval;  /* decimal point or exponent: read floating point */
#endif
      }else if(base==16){
        for(;;){
          nval=val<<4;
          if(*s>='0'&&*s<='9')
            nval+=*s++-'0';
          else if(*s>='a'&&*s<='f')
            nval+=*s++-'a'+10;
          else if(*s>='A'&&*s<='F')
            nval+=*s++-'A'+10;
          else break;
          if (nval>>4!=val)
            goto hugeval;  /* taddr overflow, read a thuge value instead */
          else
            val=nval;
        }
      }else ierror(0);
      break;
    hugeval:
      exp_type=HUG;
    case HUG:
      s=m;
      huge=huge_zero();
      if(base<=10){
        while(*s>='0'&&*s<base+'0')
          huge=haddi(hmuli(huge,base),*s++-'0');
#if FLOAT_PARSER
        if(base==10&&(*s=='e'||*s=='E'||
                      (*s=='.'&&*(s+1)>='0'&&*(s+1)<='9')))
          goto fltval;  /* decimal point or exponent: read floating point */
#endif
      }else if(base==16){
        for(;;){
          if(*s>='0'&&*s<='9')
            huge=haddi(hmuli(huge,16),*s++-'0');
          else if(*s>='a'&&*s<='f')
            huge=haddi(hmuli(huge,16),*s++-'a'+10);
          else if(*s>='A'&&*s<='F')
            huge=haddi(hmuli(huge,16),*s++-'A'+10);
          else break;
        }
      }else ierror(0);
      break;
#if FLOAT_PARSER
    fltval:
      exp_type=FLT;
    case FLT:
      if(base!=10) goto hugeval;
      s=m;
      flt=strtotfloat(s,&s);
      break;
#endif
    default:
      ierror(0);
      break;
    }
    if(s==m){
      general_error(75,base);
      goto dummyexp;
    }
    s=const_suffix(start,s);
    EXPSKIP();
    new=new_expr();
    switch(new->type=exp_type){
      case NUM: new->c.val=val; break;
      case HUG: new->c.huge=huge; break;
#if FLOAT_PARSER
      case FLT: new->c.flt=flt; break;
#endif
    }
    return new;
  }
  if(*s==current_pc_char && !ISIDCHAR(*(s+1))){
    s++;
    EXPSKIP();
    if(make_tmp_lab){
      new=new_sym_expr(new_tmplabel(0));
      add_atom(0,new_label_atom(new->c.sym));
    }else new=curpc_expr();
    return new;
  }
  if(name=parse_identifier(&s)){
    symbol *sym;    
    EXPSKIP();
    sym=find_symbol(name);
    if(!sym){
#ifdef NARGSYM
      if(!strcmp(name,NARGSYM)){
        new=new_expr();
        new->type=NUM;
        new->c.val=cur_src->num_params; /*@@@ check for macro mode? */
        return new;
      }
#endif
      sym=new_import(name);
    }
    sym->flags|=USED;
    new=(sym->type!=EXPRESSION)?new_sym_expr(sym):copy_tree(sym->expr);
    myfree(name);
    return new;
  }
  if(*s=='\''||*s=='\"'){
    taddr val=0;
    int shift=0,cnt=0;
    char quote=*s++;
    while(*s){
      char c;
      if(*s=='\\')
        s=escape(s,&c);
      else{
        c=*s++;
        if(c==quote){
          if(*s==quote)
            s++;  /* allow """" to be recognized as " and '''' as ' */
          else break;
        }
      }
      if(++cnt>bytespertaddr){
        general_error(21,bytespertaddr*8);  /* target data type overflow */
        break;
      }
      if(BIGENDIAN){
        val=(val<<8)+c;
      }else if(LITTLEENDIAN){
        val+=c<<shift;
        shift+=8;
      }else
        ierror(0);
    }
    EXPSKIP();
    new=new_expr();
    new->type=NUM;
    new->c.val=val;
    return new;
  }
  general_error(9);
dummyexp:
  new=new_expr();
  new->type=NUM;
  new->c.val=-1;
  return new;
}
    
static expr *unary_expr(void)
{
  expr *new;
  char *m;
  int len;
  if(*s=='+'||*s=='-'||*s=='!'||*s=='~'){
    m=s++;
    EXPSKIP();
  }
  else if(len=EXT_UNARY_NAME(s)){
    m=s;
    s+=len;
    EXPSKIP();
  }else
    return primary_expr();
  if(*m=='+')
    return primary_expr();
  new=new_expr();
  if(*m=='-')
    new->type=NEG;
  else if(*m=='!')
    new->type=NOT;
  else if(*m=='~')
    new->type=CPL;
  else if(EXT_UNARY_NAME(m))
    new->type=EXT_UNARY_TYPE(m);
  new->left=primary_expr();
  return new;
}  

static expr *shift_expr(void)
{
  expr *left,*new;
  char m;
  left=unary_expr();
  EXPSKIP();
  while((*s=='<'||*s=='>')&&s[1]==*s){
    m=*s;
    s+=2;
    EXPSKIP();
    new=new_expr();
    if(m=='<')
      new->type=LSH;
    else
      new->type=unsigned_shift?RSHU:RSH;
    new->left=left;
    new->right=unary_expr();
    left=new;
    EXPSKIP();
  }
  return left;
}

static expr *and_expr(void)
{
  expr *left,*new;
  left=shift_expr();
  EXPSKIP();
  while(*s=='&'&&s[1]!='&'){
    s++;
    EXPSKIP();
    new=new_expr();
    new->type=BAND;
    EXPSKIP();
    new->left=left;
    new->right=shift_expr();
    left=new;
  }
  return left;
}

static expr *exclusive_or_expr(void)
{
  expr *left,*new;
  left=and_expr();
  EXPSKIP();
  while(*s=='^'||*s=='~'){
    s++;
    EXPSKIP();
    new=new_expr();
    new->type=XOR;
    EXPSKIP();
    new->left=left;
    new->right=and_expr();
    left=new;
  }
  return left;
}

static expr *inclusive_or_expr(void)
{
  expr *left,*new;
  left=exclusive_or_expr();
  EXPSKIP();
  while((*s=='|'&&s[1]!='|')||(*s=='!'&&s[1]!='=')){
    s++;
    EXPSKIP();
    new=new_expr();
    new->type=BOR;
    EXPSKIP();
    new->left=left;
    new->right=exclusive_or_expr();
    left=new;
  }
  return left;
}

static expr *multiplicative_expr(void)
{
  expr *left,*new;
  char m;
  left=inclusive_or_expr();
  EXPSKIP();
  while(*s=='*'||*s=='/'||*s=='%'){
    m=*s++;
    EXPSKIP();
    new=new_expr();
    if(m=='/'){
      if(*s=='/'){
        s++;
        new->type=MOD;
      }
      else
        new->type=DIV;
    }
    else if(m=='*')
      new->type=MUL;
    else
      new->type=MOD;
    new->left=left;
    new->right=inclusive_or_expr();
    left=new;
    EXPSKIP();
  }
  return left;
}

static expr *additive_expr(void)
{
  expr *left,*new;
  char m;
  left=multiplicative_expr();
  EXPSKIP();
  while((*s=='+'&&s[1]!='+')||(*s=='-'&&s[1]!='-')){
    m=*s++;
    EXPSKIP();
    new=new_expr();
    if(m=='+')
      new->type=ADD;
    else
      new->type=SUB;
    new->left=left;
    new->right=multiplicative_expr();
    left=new;
    EXPSKIP();
  }
  return left;
}

static expr *relational_expr(void)
{
  expr *left,*new;
  char m1,m2=0;
  left=additive_expr();
  EXPSKIP();
  while(((*s=='<'&&s[1]!='>')||*s=='>')&&s[1]!=*s){
    m1=*s++;
    if(*s=='=')
      m2=*s++;
    EXPSKIP();
    new=new_expr();
    if(m1=='<'){
      if(m2)
        new->type=LEQ;
      else
        new->type=LT;
    }else{
      if(m2)
        new->type=GEQ;
      else
        new->type=GT;
    }
    EXPSKIP();
    new->left=left;
    new->right=additive_expr();
    left=new;
  }
  return left;
}

static expr *equality_expr(void)
{
  expr *left,*new;
  char m;
  left=relational_expr();
  EXPSKIP();
  while(*s=='='||(*s=='!'&&s[1]=='=')||(*s=='<'&&s[1]=='>')){
    m=*s++;
    if(m==*s||m!='=')
      s++;
    EXPSKIP();
    new=new_expr();
    if(m=='=')
      new->type=EQ;
    else
      new->type=NEQ;
    EXPSKIP();
    new->left=left;
    new->right=relational_expr();
    left=new;
  }
  return left;
}

static expr *logical_and_expr(void)
{
  expr *left,*new;
  left=equality_expr();
  EXPSKIP();
  while(*s=='&'&&s[1]=='&'){
    s+=2;
    EXPSKIP();
    new=new_expr();
    new->type=LAND;
    EXPSKIP();
    new->left=left;
    new->right=equality_expr();
    left=new;
  }
  return left;
}

static expr *expression(void)
{
  expr *left,*new;
  left=logical_and_expr();
  EXPSKIP();
  while(*s=='|'&&s[1]=='|'){
    s+=2;
    EXPSKIP();
    new=new_expr();
    new->type=LOR;
    EXPSKIP();
    new->left=left;
    new->right=logical_and_expr();
    left=new;
  }
  return left;
}

/* Tries to parse the string as a constant value. Sets pp to the
   end of the parse. Already defined absolute symbols are
   recognized.
   Automatically switches to a HUG expression type, when encountering a
   constant which doesn't fit into taddr.
   Automatically switches to a FLT expression type, when reading a decimal
   point or an exponent in a decimal constant (requires FLOAT_PARSER). */
expr *parse_expr(char **pp)
{
  expr *tree;
  s=*pp;
  make_tmp_lab=0;
  exp_type=NUM;
  tree=expression();
  simplify_expr(tree);
  *pp=s;
  return tree;
}

expr *parse_expr_tmplab(char **pp)
{
  expr *tree;
  s=*pp;
  make_tmp_lab=1;
  exp_type=NUM;
  tree=expression();
  simplify_expr(tree);
  *pp=s;
  return tree;
}

/* Tries to parse the string as a huge-integer (128 bits) constant (thuge).
   No labels allowed in this mode. */
expr *parse_expr_huge(char **pp)
{
  expr *tree;
  s=*pp;
  make_tmp_lab=0;
  exp_type=HUG;
  tree=expression();
  simplify_expr(tree);
  *pp=s;
  return tree;
}

/* Tries to parse the string as a floating point constant (tfloat).
   No labels allowed in this mode.
   When the constant is not decimal switch to reading a HUG expression type. */
expr *parse_expr_float(char **pp)
{
#if FLOAT_PARSER
  expr *tree;
  s=*pp;
  make_tmp_lab=0;
  exp_type=FLT;
  tree=expression();
  simplify_expr(tree);
  *pp=s;
  return tree;
#else
  general_error(79,cpuname);  /* backend does not support floating point */
  return NULL;
#endif
}

void free_expr(expr *tree)
{
  if(!tree)
    return;
  free_expr(tree->left);
  free_expr(tree->right);
  myfree(tree);
}

/* Return type of expression.
   Either NUM, HUG or FLT. Labels or unknown symbols default to NUM.
   Returns 0 in case of an error (e.g. epxression is NULL pointer). */
int type_of_expr(expr *tree)
{
  int ltype,rtype;
  if(tree==NULL)
    return 0;
  ltype=tree->type;
  if(ltype==SYM){
    if(tree->c.sym->flags&INEVAL)
      general_error(18,tree->c.sym->name);
    tree->c.sym->flags|=INEVAL;
    ltype=tree->c.sym->type==EXPRESSION?type_of_expr(tree->c.sym->expr):NUM;
    tree->c.sym->flags&=~INEVAL;
    return ltype;
  }else if(ltype==NUM||ltype==HUG||ltype==FLT)
    return ltype;
  ltype=type_of_expr(tree->left);
  rtype=type_of_expr(tree->right);
  return rtype>ltype?rtype:ltype;
}

/* Find pointer to first symbol occurence inside expression tree */
expr **find_sym_expr(expr **ptree,char *name)
{
  expr **psym;

  if(*ptree==NULL)
    return NULL;
  if((*ptree)->left!=NULL &&
     (*ptree)->left->type==SYM&&!strcmp((*ptree)->left->c.sym->name,name))
    return &(*ptree)->left;
  if(psym=find_sym_expr(&(*ptree)->left,name))
    return psym;
  if((*ptree)->right!=NULL &&
     (*ptree)->right->type==SYM&&!strcmp((*ptree)->right->c.sym->name,name))
    return &(*ptree)->right;
  if(psym=find_sym_expr(&(*ptree)->right,name))
    return psym;
  return NULL;
}

/* Try to evaluate expression as far as possible. Subexpressions
   only containing constants or absolute symbols are simplified. */
void simplify_expr(expr *tree)
{
  taddr ival;
  thuge hval;
#if FLOAT_PARSER
  tfloat fval;
#endif
  int type=0;
  if(!tree)
    return;
  simplify_expr(tree->left);
  simplify_expr(tree->right);
  if(tree->type==SUB&&tree->right->type==SYM&&tree->left->type==SUB&&
     tree->left->left->type!=SYM&&tree->left->right->type==SYM){
    /* Rearrange nodes from "const-symbol-symbol", so that "symbol-symbol"
       is evaluated first, as it may yield a constant. */
    expr *x=tree->right;
    tree->right=tree->left;
    tree->left=tree->right->left;
    tree->right->left=tree->right->right;
    tree->right->right=x;
  }
  if(tree->left){
    if(tree->left->type==NUM||tree->left->type==HUG||tree->left->type==FLT)
      type=tree->left->type;
    else return;
  }
  if(tree->right){
    if(tree->right->type==NUM||tree->right->type==HUG||tree->right->type==FLT){
      if(tree->right->type>type)
        type=tree->right->type;
    } else return;
  }
  if(type==NUM){
    switch(tree->type){
    case ADD:
      ival=(tree->left->c.val+tree->right->c.val);
      break;
    case SUB:
      ival=(tree->left->c.val-tree->right->c.val);
      break;
    case MUL:
      ival=(tree->left->c.val*tree->right->c.val);
      break;
    case DIV:
      if(tree->right->c.val==0){
        general_error(41);
        ival=0;
      }else if(tree->left->c.val==taddrmin&&tree->right->c.val==-1){
        general_error(21,sizeof(taddr)*8);  /* target data type overflow */
        ival=taddrmin;
      }else
        ival=(tree->left->c.val/tree->right->c.val);
      break;
    case MOD:
      if(tree->right->c.val==0){
        general_error(41);
        ival=0;
      }else if(tree->left->c.val==taddrmin&&tree->right->c.val==-1){
        general_error(21,sizeof(taddr)*8);  /* target data type overflow */
        ival=taddrmin;
      }else
        ival=(tree->left->c.val%tree->right->c.val);
      break;
    case NEG:
      ival=(-tree->left->c.val);
      break;
    case CPL:
      ival=(~tree->left->c.val);
      break;
    case LAND:
      ival=BOOLEAN(tree->left->c.val&&tree->right->c.val);
      break;
    case LOR:
      ival=BOOLEAN(tree->left->c.val||tree->right->c.val);
      break;
    case BAND:
      ival=(tree->left->c.val&tree->right->c.val);
      break;
    case BOR:
      ival=(tree->left->c.val|tree->right->c.val);
      break;
    case XOR:
      ival=(tree->left->c.val^tree->right->c.val);
      break;
    case NOT:
      ival=(!tree->left->c.val);
      break;
    case LSH:
      ival=(tree->left->c.val<<tree->right->c.val);
      break;
    case RSH:
      ival=(tree->left->c.val>>tree->right->c.val);
      break;
    case RSHU:
      ival=((utaddr)tree->left->c.val>>tree->right->c.val);
      break;
    case LT:
      ival=BOOLEAN(tree->left->c.val<tree->right->c.val);
      break;
    case GT:
      ival=BOOLEAN(tree->left->c.val>tree->right->c.val);
      break;
    case LEQ:
      ival=BOOLEAN(tree->left->c.val<=tree->right->c.val);
      break;
    case GEQ:
      ival=BOOLEAN(tree->left->c.val>=tree->right->c.val);
      break;
    case NEQ:
      ival=BOOLEAN(tree->left->c.val!=tree->right->c.val);
      break;
    case EQ:
      ival=BOOLEAN(tree->left->c.val==tree->right->c.val);
      break;
    default:
#ifdef EXT_UNARY_EVAL
      if (tree->left && EXT_UNARY_EVAL(tree->type,tree->left->c.val,&ival,1))
        break;
#endif
      return;
    }
  }
  else if(type==HUG){
    thuge lval,rval;
    if(tree->left){
      if(tree->left->type==NUM)
        lval=huge_from_int(tree->left->c.val);
      else
        lval=tree->left->c.huge;
    }
    if(tree->right){
      if(tree->right->type==NUM)
        rval=huge_from_int(tree->right->c.val);
      else
        rval=tree->right->c.huge;
    }
    switch(tree->type){
    case ADD:
      hval=hadd(lval,rval);
      break;
    case SUB:
      hval=hsub(lval,rval);
      break;
    case MUL:
      hval=hmul(lval,rval);
      break;
    case DIV:
      if(hcmp(rval,huge_zero())==0){
        general_error(41);
        hval=huge_zero();
      }else
        hval=hdiv(lval,rval);
      break;
    case MOD:
      if(hcmp(rval,huge_zero())==0){
        general_error(41);
        hval=huge_zero();
      }else
        hval=hmod(lval,rval);
      break;
    case NEG:
      hval=hneg(lval);
      break;
    case CPL:
      hval=hcpl(lval);
      break;
    case LAND:
      ival=BOOLEAN(hcmp(lval,huge_zero())!=0&&hcmp(rval,huge_zero())!=0);
      type=NUM;
      break;
    case LOR:
      ival=BOOLEAN(hcmp(lval,huge_zero())!=0||hcmp(rval,huge_zero())!=0);
      type=NUM;
      break;
    case BAND:
      hval=hand(lval,rval);
      break;
    case BOR:
      hval=hor(lval,rval);
      break;
    case XOR:
      hval=hxor(lval,rval);
      break;
    case NOT:
      hval=hnot(lval);
      break;
    case LSH:
      hval=hshl(lval,huge_to_int(rval));
      break;
    case RSH:
      hval=hshra(lval,huge_to_int(rval));
      break;
    case RSHU:
      hval=hshr(lval,huge_to_int(rval));
      break;
    case LT:
      ival=BOOLEAN(hcmp(lval,rval)<0);
      type=NUM;
      break;
    case GT:
      ival=BOOLEAN(hcmp(lval,rval)>0);
      type=NUM;
      break;
    case LEQ:
      ival=BOOLEAN(hcmp(lval,rval)<=0);
      type=NUM;
      break;
    case GEQ:
      ival=BOOLEAN(hcmp(lval,rval)>=0);
      type=NUM;
      break;
    case NEQ:
      ival=BOOLEAN(hcmp(lval,rval)!=0);
      type=NUM;
      break;
    case EQ:
      ival=BOOLEAN(hcmp(lval,rval)==0);
      type=NUM;
      break;
    default:
      return;
    }
  }
#if FLOAT_PARSER
  else if(type==FLT){
    tfloat lval,rval;
    if(tree->left){
      switch(tree->left->type){
        case NUM: lval=(tfloat)tree->left->c.val; break;
        case HUG: lval=huge_to_float(tree->left->c.huge); break;
        case FLT: lval=tree->left->c.flt; break;
      }
    }
    if(tree->right){
      switch(tree->right->type){
        case NUM: rval=(tfloat)tree->right->c.val; break;
        case HUG: rval=huge_to_float(tree->right->c.huge); break;
        case FLT: rval=tree->right->c.flt; break;
      }
    }
    switch(tree->type){
    case ADD:
      fval=(lval+rval);
      break;
    case SUB:
      fval=(lval-rval);
      break;
    case MUL:
      fval=(lval*rval);
      break;
    case DIV:
      if(rval==0.0){
        general_error(41);
        fval=0.0;
      }else
        fval=(lval/rval);
      break;
    case NEG:
      fval=(-lval);
      break;
    case LT:
      ival=BOOLEAN(lval<rval);
      type=NUM;
      break;
    case GT:
      ival=BOOLEAN(lval>rval);
      type=NUM;
      break;
    case LEQ:
      ival=BOOLEAN(lval<=rval);
      type=NUM;
      break;
    case GEQ:
      ival=BOOLEAN(lval>=rval);
      type=NUM;
      break;
    case NEQ:
      ival=BOOLEAN(lval!=rval);
      type=NUM;
      break;
    case EQ:
      ival=BOOLEAN(lval==rval);
      type=NUM;
      break;
    default:
      return;
    }
  }
#endif  /* FLOAT_PARSER */
  else{
    if(tree->type==SYM&&tree->c.sym->type==EXPRESSION){
      switch(tree->c.sym->expr->type){
      case NUM:
        ival=tree->c.sym->expr->c.val;
        type=NUM;
        break;
      case HUG:
        hval=tree->c.sym->expr->c.huge;
        type=HUG;
        break;
#if FLOAT_PARSER
      case FLT:
        fval=tree->c.sym->expr->c.flt;
        type=FLT;
        break;
#endif
      default:
        return;
      }
    }else return;
  }
  free_expr(tree->left);
  free_expr(tree->right);
  tree->left=tree->right=NULL;
  switch(tree->type=type){
    case NUM: tree->c.val=ival; break;
    case HUG: tree->c.huge=hval; break;
#if FLOAT_PARSER
    case FLT: tree->c.flt=fval; break;
#endif
    default: ierror(0); break;
  }
}

static void add_dep(section *src, section *dest)
{
  if(num_secs&&src!=NULL&&src!=dest){
    if(debug&&(!dest->deps||!BTST(dest->deps,src->idx)))
      printf("sec %s might depend on %s\n",src->name,dest->name);
    if(!dest->deps){
      dest->deps=mymalloc(BVSIZE(num_secs));
      memset(dest->deps,0,BVSIZE(num_secs));
    }
    BSET(dest->deps, src->idx);
  }
}

/* Evaluate an expression using current values of all symbols.
   Result is written to *result. The return value specifies
   whether the result is constant (i.e. only depending on
   constants or absolute symbols). */
int eval_expr(expr *tree,taddr *result,section *sec,taddr pc)
{
  taddr val,lval,rval;
  symbol *lsym,*rsym;
  int cnst=1;

  if(!tree)
    ierror(0);
  if(tree->left&&!eval_expr(tree->left,&lval,sec,pc))
    cnst=0;
  if(tree->right&&!eval_expr(tree->right,&rval,sec,pc))
    cnst=0;

  switch(tree->type){
  case ADD:
    val=(lval+rval);
    break;
  case SUB:
    find_base(tree->left,&lsym,sec,pc);
    find_base(tree->right,&rsym,sec,pc);
    if(cnst==0&&rsym!=NULL&&LOCREF(rsym)){
      if(lsym!=NULL&&LOCREF(lsym)&&lsym->sec==rsym->sec){
        /* l2-l1 is constant when both have a valid symbol-base, and both
           symbols are LABSYMs from the same section, e.g. (sym1+x)-(sym2-y) */
        cnst=1;
	add_dep(sec,lsym->sec);
      }else if(lsym!=NULL&&(rsym->sec==sec&&(EXTREF(lsym)||LOCREF(lsym)))){
        /* Difference between symbols from different section or between an
           external symbol and a symbol from the current section can be
           represented by a REL_PC, so we calculate the addend. */
        if((rsym->flags&ABSLABEL)&&(lsym->flags&ABSLABEL)){
	  add_dep(sec, lsym->sec);
	  add_dep(sec, rsym->sec);
          cnst=1;  /* constant, when labels are from two ORG sections */
	}else{
          /* prepare a value which works with REL_PC */
          val=(pc-rval+lval-(lsym->sec?lsym->sec->org:0));
          break;
        }
      }else if(lsym==NULL&&(rsym->flags&ABSLABEL))
        /* const-label is valid and yields a const in absolute ORG sections */
	  add_dep(sec, rsym->sec);
	  cnst=1;
    }
    val=(lval-rval);
    break;
  case MUL:
    val=(lval*rval);
    break;
  case DIV:
    if(rval==0){
      if (final_pass)
        general_error(41);
      val=0;
    }else if(lval==taddrmin&&rval==-1){
      if (final_pass)
        general_error(21,sizeof(taddr)*8);  /* target data type overflow */
      val=taddrmin;
    }else
      val=(lval/rval);
    break;
  case MOD:
    if(rval==0){
      if (final_pass)
        general_error(41);
      val=0;
    }else if(lval==taddrmin&&rval==-1){
      if (final_pass)
        general_error(21,sizeof(taddr)*8);  /* target data type overflow */
      val=taddrmin;
    }else
      val=(lval%rval);
    break;
  case NEG:
    val=(-lval);
    break;
  case CPL:
    val=(~lval);
    break;
  case LAND:
    val=BOOLEAN(lval&&rval);
    break;
  case LOR:
    val=BOOLEAN(lval||rval);
    break;
  case BAND:
    val=(lval&rval);
    break;
  case BOR:
    val=(lval|rval);
    break;
  case XOR:
    val=(lval^rval);
    break;
  case NOT:
    val=(!lval);
    break;
  case LSH:
    val=(lval<<rval);
    break;
  case RSH:
    val=(lval>>rval);
    break;
  case RSHU:
    val=((utaddr)lval>>rval);
    break;
  case LT:
    val=BOOLEAN(lval<rval);
    break;
  case GT:
    val=BOOLEAN(lval>rval);
    break;
  case LEQ:
    val=BOOLEAN(lval<=rval);
    break;
  case GEQ:
    val=BOOLEAN(lval>=rval);
    break;
  case NEQ:
    val=BOOLEAN(lval!=rval);
    break;
  case EQ:
    val=BOOLEAN(lval==rval);
    break;
  case SYM:
    lsym=tree->c.sym;
    if(lsym->type==EXPRESSION){
      if(lsym->flags&INEVAL)
        general_error(18,lsym->name);
      lsym->flags|=INEVAL;
      cnst=eval_expr(lsym->expr,&val,sec,pc);
      lsym->flags&=~INEVAL;
    }else if(LOCREF(lsym)){
      update_curpc(tree,sec,pc);
      val=lsym->pc;
      cnst=lsym->sec==NULL?0:(lsym->sec->flags&UNALLOCATED)!=0;
      if(lsym->flags&ABSLABEL) cnst=1;
      if(cnst) add_dep(sec,lsym->sec);
    }else{
      /* IMPORT */
      cnst=0;
      val=0;
    }
    break;
  case NUM:
    val=tree->c.val;
    break;
  case HUG:
    if (!huge_chkrange(tree->c.huge,bytespertaddr*8) && final_pass)
      general_error(21,bytespertaddr*8);  /* target data type overflow */
    val=huge_to_int(tree->c.huge);
    break;
#if FLOAT_PARSER
  case FLT:
    if (!flt_chkrange(tree->c.flt,bytespertaddr*8) && final_pass)
      general_error(21,bytespertaddr*8);  /* target data type overflow */
    val=(taddr)tree->c.flt;
    break;
#endif
  default:
#ifdef EXT_UNARY_EVAL
    if (EXT_UNARY_EVAL(tree->type,lval,&val,cnst))
      break;
#endif
    ierror(0);
  }
  *result=val;
  return cnst;
}

/* Evaluate a huge integer expression using current values of all symbols.
   Result is written to *result. The return value specifies whether all
   operations were valid. */
int eval_expr_huge(expr *tree,thuge *result)
{
  thuge val,lval,rval;

  if(!tree)
    ierror(0);
  if(tree->left&&!eval_expr_huge(tree->left,&lval))
    return 0;
  if(tree->right&&!eval_expr_huge(tree->right,&rval))
    return 0;

  switch (tree->type){
  case ADD:
    val=hadd(lval,rval);
    break;
  case SUB:
    val=hsub(lval,rval);
    break;
  case MUL:
    val=hmul(lval,rval);
    break;
  case DIV:
    if(hcmp(rval,huge_zero())==0){
      if (final_pass)
        general_error(41);
      val=huge_zero();
    }else
      val=hdiv(lval,rval);
    break;
  case MOD:
    if(hcmp(rval,huge_zero())==0){
      if (final_pass)
        general_error(41);
      val=huge_zero();
    }else
      val=hmod(lval,rval);
    break;
  case NEG:
    val=hneg(lval);
    break;
  case CPL:
    val=hcpl(lval);
    break;
  case LAND:
    val=huge_from_int(BOOLEAN(hcmp(lval,huge_zero())!=0
                      &&hcmp(rval,huge_zero())!=0));
    break;
  case LOR:
    val=huge_from_int(BOOLEAN(hcmp(lval,huge_zero())!=0
                      ||hcmp(rval,huge_zero())!=0));
    break;
  case BAND:
    val=hand(lval,rval);
    break;
  case BOR:
    val=hor(lval,rval);
    break;
  case XOR:
    val=hxor(lval,rval);
    break;
  case NOT:
    val=hnot(lval);
    break;
  case LSH:
    val=hshl(lval,huge_to_int(rval));
    break;
  case RSH:
    val=hshra(lval,huge_to_int(rval));
    break;
  case RSHU:
    val=hshr(lval,huge_to_int(rval));
    break;
  case LT:
    val=huge_from_int(BOOLEAN(hcmp(lval,rval)<0));
    break;
  case GT:
    val=huge_from_int(BOOLEAN(hcmp(lval,rval)>0));
    break;
  case LEQ:
    val=huge_from_int(BOOLEAN(hcmp(lval,rval)<=0));
    break;
  case GEQ:
    val=huge_from_int(BOOLEAN(hcmp(lval,rval)>=0));
    break;
  case NEQ:
    val=huge_from_int(BOOLEAN(hcmp(lval,rval)!=0));
    break;
  case EQ:
    val=huge_from_int(BOOLEAN(hcmp(lval,rval)==0));
    break;
  case SYM:
    if(tree->c.sym->type==EXPRESSION){
      int ok;
      if(tree->c.sym->flags&INEVAL)
        general_error(18,tree->c.sym->name);
      tree->c.sym->flags|=INEVAL;
      ok=eval_expr_huge(tree->c.sym->expr,&val);
      tree->c.sym->flags&=~INEVAL;
      if(!ok) return 0;
    }
#if 0 /* all relocations should be representable by taddr */
    else if(EXTREF(tree->c.sym))
      val=huge_zero();
#endif
    else
      return 0;
    break;
  case NUM:
    val=huge_from_int(tree->c.val);
    break;
  case HUG:
    val=tree->c.huge;
    break;
#if FLOAT_PARSER
  case FLT:
    val=huge_from_float(tree->c.flt);
    break;
#endif
  default:
    return 0;
  }
  *result=val;
  return 1;
}

#if FLOAT_PARSER
/* Evaluate a floating point expression using current values of all symbols.
   Result is written to *result. The return value specifies whether all
   operations were valid. */
int eval_expr_float(expr *tree,tfloat *result)
{
  tfloat val,lval,rval;

  if(!tree)
    ierror(0);
  if(tree->left&&!eval_expr_float(tree->left,&lval))
    return 0;
  if(tree->right&&!eval_expr_float(tree->right,&rval))
    return 0;

  switch(tree->type){
  case ADD:
    val=(lval+rval);
    break;
  case SUB:
    val=(lval-rval);
    break;
  case MUL:
    val=(lval*rval);
    break;
  case DIV:
    if(rval==0.0){
      if (final_pass)
        general_error(41);
      val=0.0;
    }else
      val=(lval/rval);
    break;
  case NEG:
    val=(-lval);
    break;
  case LT:
    val=BOOLEAN(lval<rval);
    break;
  case GT:
    val=BOOLEAN(lval>rval);
    break;
  case LEQ:
    val=BOOLEAN(lval<=rval);
    break;
  case GEQ:
    val=BOOLEAN(lval>=rval);
    break;
  case NEQ:
    val=BOOLEAN(lval!=rval);
    break;
  case EQ:
    val=BOOLEAN(lval==rval);
    break;
  case SYM:
    if(tree->c.sym->type==EXPRESSION){
      int ok;
      if(tree->c.sym->flags&INEVAL)
        general_error(18,tree->c.sym->name);
      tree->c.sym->flags|=INEVAL;
      ok=eval_expr_float(tree->c.sym->expr,&val);
      tree->c.sym->flags&=~INEVAL;
      if(!ok) return 0;
    }else
      return 0;
    break;
  case NUM:
    val=(tfloat)tree->c.val;
    break;
  case HUG:
    val=huge_to_float(tree->c.huge);
    break;
  case FLT:
    val=tree->c.flt;
    break;
  default:
    return 0;
  }
  *result=val;
  return 1;
}
#endif  /* FLOAT_PARSER */

void print_expr(FILE *f,expr *p)
{
  if(p==NULL)
    ierror(0);
  simplify_expr(p);
  if(p->type==NUM)
    fprintf(f,"%lld=0x%llx",(long long)p->c.val,ULLTADDR(p->c.val));
  else if(p->type==HUG)
    fprintf(f,"0x%016llx%016llx",(long long)p->c.huge.hi,(long long)p->c.huge.lo);
#if FLOAT_PARSER
  else if(p->type==FLT)
    fprintf(f,"%.8g",(double)p->c.flt);
#endif
  else
    fprintf(f,"complex expression");
}

static int _find_base(expr *p,symbol **base,section *sec,taddr pc)
{
#ifdef EXT_FIND_BASE
  int ret;
  if(ret=EXT_FIND_BASE(base,p,sec,pc))
    return ret;
#endif
  if(p->type==SYM){
    update_curpc(p,sec,pc);
    if(p->c.sym->type==EXPRESSION)
      return _find_base(p->c.sym->expr,base,sec,pc);
    else{
      if(base)
        *base=p->c.sym;
      return BASE_OK;
    }
  }
  if(p->type==ADD){
    taddr val;
    if(eval_expr(p->left,&val,sec,pc)&&
       _find_base(p->right,base,sec,pc)==BASE_OK)
      return BASE_OK;
    if(eval_expr(p->right,&val,sec,pc)&&
       _find_base(p->left,base,sec,pc)==BASE_OK)
      return BASE_OK;
  }
  if(p->type==SUB){
    taddr val;
    symbol *pcsym;
    if(eval_expr(p->right,&val,sec,pc)&&
       _find_base(p->left,base,sec,pc)==BASE_OK)
      return BASE_OK;
    if(_find_base(p->left,base,sec,pc)==BASE_OK&&
       _find_base(p->right,&pcsym,sec,pc)==BASE_OK) {
      if(LOCREF(pcsym)&&pcsym->sec==sec&&(LOCREF(*base)||EXTREF(*base)))
        return BASE_PCREL;
    }
  }
  return BASE_ILLEGAL;
}

/* Tests, if an expression is based only on one non-absolute
   symbol plus constants. Returns that symbol or zero.
   Note: Does not find all possible solutions. */
int find_base(expr *p,symbol **base,section *sec,taddr pc)
{
  if(base)
    *base=NULL;
  return _find_base(p,base,sec,pc);
}

expr *number_expr(taddr val)
{
  expr *new=new_expr();
  new->type=NUM;
  new->c.val=val;
  return new;
}

expr *huge_expr(thuge val)
{
  expr *new=new_expr();
  new->type=HUG;
  new->c.huge=val;
  return new;
}

#if FLOAT_PARSER
expr *float_expr(tfloat val)
{
  expr *new=new_expr();
  new->type=FLT;
  new->c.flt=val;
  return new;
}
#endif

taddr parse_constexpr(char **s)
{
  expr *tree;
  taddr val = 0;

  if (tree = parse_expr(s)) {
    simplify_expr(tree);
    switch(tree->type){
      case NUM:
        val = tree->c.val;
        break;
      case HUG:
        general_error(59);  /* cannot evaluate huge integer */
        break;
      case FLT:
        general_error(60);  /* cannot evaluate floating point */
        break;
      default:
        general_error(30);  /* expression must be a constant */
        break;
    }
    free_expr(tree);
  }
  return val;
}
