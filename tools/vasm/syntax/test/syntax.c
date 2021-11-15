/* syntax.c  syntax module for vasm */
/* (c) in 2002 by Volker Barthelmann */

#include "vasm.h"

/* The syntax module parses the input (read_next_line), handles
   assembly-directives (section, data-storage etc.) and parses
   mnemonics. Assembly instructions are split up in mnemonic name,
   qualifiers and operands. new_inst returns a matching instruction,
   if one exists.
   Routines for creating sections and adding atoms to sections will
   be provided by the main module.
*/

char *syntax_copyright="vasm test syntax module (c) 2002 Volker Barthelmann";

hashtable *dirhash;
char commentchar=';';
int dotdirs;
char *defsectname = NULL;
char *defsecttype = NULL;


char *skip(char *s)
{
  while (isspace((unsigned char )*s))
    s++;
  return s;
}

/* check for end of line, issue error, if not */
void eol(char *s)
{
  s = skip(s);
  if (!ISEOL(s))
    syntax_error(6);
}

char *skip_operand(char *s)
{
  int par_cnt=0;
  while(1){
    if(*s=='(') par_cnt++;
    if(*s==')'){
      if(par_cnt>0)
        par_cnt--;
      else
        syntax_error(3);
    }
    if(ISEOL(s)||(*s==','&&par_cnt==0))
      break;
    s++;
  }
  if(par_cnt!=0)
    syntax_error(4);
  return s;
}

static void handle_section(char *s)
{
  char *name,*attr;
  if(*s!='\"'){
    name=s;
    if(!ISIDSTART(*s)){
      syntax_error(10);
      return;
    }
    while(ISIDCHAR(*s))
      s++;
    name=cnvstr(name,s-name);
    s=skip(s);
  }else{
    s++;
    name=s;
    while(*s&&*s!='\"')
      s++;
    name=cnvstr(name,s-name);
    s=skip(s+1);
  }
  if(*s==','){
    s=skip(s+1);
    attr=s;
    if(*s!='\"')
      syntax_error(7);
    else
      s++;
    attr=s;
    while(*s&&*s!='\"')
      s++;    
    attr=cnvstr(attr,s-attr);
    s=skip(s+1);
  }else
    attr="";
  new_section(name,attr,1);
  switch_section(name,attr);
  eol(s);
}

static char *string(char *s,dblock **result)
{
  dblock *db=new_dblock();
  taddr size;
  char *p,esc;
  if(*s!='\"')
    ierror(0);
  else
    s++;
  p=s;
  size=0;
  while(*s&&*s!='\"'){
    if(*s=='\\')
      s=escape(s,&esc);
    else
      s++;
    size++;
  }
  db->size=size;
  db->data=mymalloc(db->size);
  s=p;
  p=(char *)db->data;
  while(*s&&*s!='\"'){
    if(*s=='\\')
      s=escape(s,p++);
    else
      *p++=*s++;
  }
  *result=db;
  if(*s=!'\"')
    syntax_error(7);
  else
    s++;
  return s;
}

static void handle_data(char *s,int size,int noalign,int zeroterm)
{
  dblock *db;
  do{
    char *opstart=s;
    operand *op;
    if(size==8&&*s=='\"'){
      s=string(s,&db);
      add_atom(0,new_data_atom(db,1));
    }else{
      op=new_operand();
      s=skip_operand(s);
      if(!parse_operand(opstart,s-opstart,op,DATA_OPERAND(size))){
        syntax_error(8);
      }else{
        atom *a=new_datadef_atom(size,op);
        if(noalign)
          a->align=1;
        add_atom(0,a);
      }
    }
    s=skip(s);
    if(*s==','){
      s=skip(s+1);
    }else if(*s)
      syntax_error(9);
  }while(*s);
  if(zeroterm){
    if(size!=8)
      ierror(0);
    db=new_dblock();
    db->size=1;
    db->data=mymalloc(1);
    *db->data=0;
    add_atom(0,new_data_atom(db,1));
  }    
  eol(s);
}

static void handle_global(char *s)
{
  symbol *sym;
  char *name;
  name=s;
  if(!(name=parse_identifier(&s))){
    syntax_error(10);
    return;
  }
  sym=new_import(name);
  sym->flags|=EXPORT;
  myfree(name);
  eol(s);
}

static void handle_align(char *s)
{
  taddr align=parse_constexpr(&s);
  atom *a=new_space_atom(number_expr(0),1,0);
  a->align=1<<align;
  add_atom(0,a);
  eol(s);
}

static void handle_space(char *s)
{
  expr *space,*fill=0;
  space=parse_expr_tmplab(&s);
  s=skip(s);
  if(*s==','){
    s=skip(s+1);
    fill=parse_expr_tmplab(&s);
  }
  add_atom(0,new_space_atom(space,1,fill));
  eol(s);  
}

static void handle_8bit(char *s){ handle_data(s,8,0,0); }
static void handle_16bit(char *s){ handle_data(s,16,0,0); }
static void handle_32bit(char *s){ handle_data(s,32,0,0); }
static void handle_16bit_noalign(char *s){ handle_data(s,16,1,0); }
static void handle_32bit_noalign(char *s){ handle_data(s,32,1,0); }
static void handle_string(char *s){ handle_data(s,8,0,1); }
static void handle_texts(char *s){ handle_section(".text,\"acrx4\"");eol(s);}
static void handle_datas(char *s){ handle_section(".data,\"adrw4\"");eol(s);}
static void handle_sdatas(char *s){ handle_section(".sdata,\"adrw4\"");eol(s);}
static void handle_sdata2s(char *s){ handle_section(".sdata2,\"adr4\"");eol(s);}
static void handle_rodatas(char *s){ handle_section(".rodata,\"adr4\"");eol(s);}
static void handle_bsss(char *s){ handle_section(".bss,\"aurw4\"");eol(s);}
static void handle_sbsss(char *s){ handle_section(".bss,\"aurw4\"");eol(s);}

struct {
  char *name;
  void (*func)(char *);
} directives[]={
  "section",handle_section,
  "dcb",handle_8bit,
  "dc.b",handle_8bit,
  "dcw",handle_16bit_noalign,
  "dc.w",handle_16bit_noalign,
  "dcl",handle_32bit_noalign,
  "dc.l",handle_32bit_noalign,
  "public",handle_global,
  "xdef",handle_global,
  "xref",handle_global,  
  "text",handle_texts,
  "cseg",handle_texts,
  "data",handle_datas,
  "bss",handle_bsss,  
  "align",handle_align,
  "ds.b",handle_space,
};

int dir_cnt=sizeof(directives)/sizeof(directives[0]);

/* Handles assembly directives; returns non-zero if the line
   was a directive. */
static int handle_directive(char *line)
{
  char *s,*name;
  hashdata data;
  s=skip(line);
  if(!ISIDSTART(*s))
    return 0;
  name=s;
  while(ISIDCHAR(*s))
    s++;
  if(!find_namelen(dirhash,name,s-name,&data))
    return 0;
  directives[data.idx].func(skip(s));
  return 1;
}

/* Very simple, very incomplete. */
void parse(void)
{
  char *s,*line,*inst,*ext[MAX_QUALIFIERS?MAX_QUALIFIERS:1],*op[MAX_OPERANDS];
  int inst_len,ext_len[MAX_QUALIFIERS?MAX_QUALIFIERS:1],op_len[MAX_OPERANDS];
  int i,ext_cnt,op_cnt;
  instruction *ip;
  while(line=read_next_line()){
    s=line;

    if(isalnum((unsigned char)*s)){
      /* Handle labels at beginning of line */
      char *labname;
      symbol *label;
      while(*s&&!isspace((unsigned char)*s)&&*s!=':')
        s++;
      labname=cnvstr(line,s-line);
      s=skip(s+1);
      if(!strncmp(s,"equ",3)&&isspace((unsigned char)s[3])){
        s=skip(s+3);
        label=new_abs(labname,parse_expr(&s));
      }else{
        label=new_labsym(0,labname);
        add_atom(0,new_label_atom(label));
      }
      free(labname);
    }

    s=parse_cpu_special(s);

    if(handle_directive(s))
      continue;

    /* skip spaces */
    s=skip(s);
    if(!*s)
      continue;

    /* read mnemonic name */
    inst=s;
    if(!ISIDSTART(*s)){
      syntax_error(10);
      continue;
    }
#if MAX_QUALIFIERS==0
    while(*s&&!isspace((unsigned char)*s))
      s++;
#else
    while(*s&&*s!='.'&&!isspace((unsigned char)*s))
      s++;
#endif
    inst_len=s-inst;

    /* read qualifiers */
    ext_cnt=0;
    while(*s=='.'&&ext_cnt<MAX_QUALIFIERS){
      s++;
      ext[ext_cnt]=s;
      while(*s&&*s!='.'&&!isspace((unsigned char)*s))
        s++;
      ext_len[ext_cnt]=s-ext[ext_cnt];
      if(ext_len[ext_cnt]<=0)
        syntax_error(1);
      else
        ext_cnt++;
    }

    if(!isspace((unsigned char)*s)) syntax_error(2);
    
    /* read operands, terminated by comma (unless in parentheses)  */
    s=skip(s);
    op_cnt=0;
    while(*s&&op_cnt<MAX_OPERANDS){
      op[op_cnt]=s;
      s=skip_operand(s);
      op_len[op_cnt]=s-op[op_cnt];
      if(op_len[op_cnt]<=0)
        syntax_error(5);
      else
        op_cnt++;
      s=skip(s);
      if(*s!=','){
        break;
      }else{
        s=skip(s+1);
      }
    }      
    s=skip(s);
    if(*s!=0) syntax_error(6);
    ip=new_inst(inst,inst_len,op_cnt,op,op_len);
#if MAX_QUALIFIERS>0
    if(ip){
      for(i=0;i<ext_cnt;i++)
        ip->qualifiers[i]=cnvstr(ext[i],ext_len[i]);
      for(;i<MAX_QUALIFIERS;i++)
        ip->qualifiers[i]=0;
    }
#endif
    if(ip){
      add_atom(0,new_inst_atom(ip));
    }else
      ;
  }
}

char *const_prefix(char *s,int *base)
{
  if(isdigit((unsigned char)*s)){
    *base=10;
    return s;
  }
  if(*s=='$'){
    *base=16;
    return s+1;
  }
  if(*s=='%'){
    *base=2;
    return s+1;
  }
  *base=0;
  return s;
}

char *const_suffix(char *start,char *end)
{
  return end;
}

char *get_local_label(char **start)
{
  return NULL;
}

char *parse_macro_arg(struct macro *m,char *s,
                      struct namelen *param,struct namelen *arg)
{
  /* no macro support */
  return s;
}

int expand_macro(source *src,char **line,char *d,int dlen)
{
  return 0;
}

int init_syntax()
{
  size_t i;
  hashdata data;
  dirhash=new_hashtable(0x200); /*FIXME: */
  for(i=0;i<dir_cnt;i++){
    data.idx=i;
    add_hashentry(dirhash,directives[i].name,data);
  }
  
  return 1;
}

int syntax_args(char *p)
{
  return 0;
}
