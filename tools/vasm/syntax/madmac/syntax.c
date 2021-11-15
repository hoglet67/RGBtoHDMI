/* syntax.c  syntax module for vasm */
/* (c) in 2015-2021 by Frank Wille */

#include "vasm.h"
#include "error.h"

/* The syntax module parses the input (read_next_line), handles
   assembly-directives (section, data-storage etc.) and parses
   mnemonics. Assembly instructions are split up in mnemonic name,
   qualifiers and operands. new_inst returns a matching instruction,
   if one exists.
   Routines for creating sections and adding atoms to sections will
   be provided by the main module.
*/

char *syntax_copyright="vasm madmac syntax module 0.4h (c) 2015-2021 Frank Wille";
hashtable *dirhash;
char commentchar = ';';
int dotdirs;

static char text_name[] = ".text";
static char data_name[] = ".data";
static char bss_name[] = ".bss";
static char text_type[] = "acrx";
static char data_type[] = "adrw";
static char bss_type[] = "aurw";
char *defsectname = text_name;
char *defsecttype = text_type;

static char macroname[] = ".macro";
static char endmname[] = ".endm";
static char reptname[] = ".rept";
static char endrname[] = ".endr";
static struct namelen macro_dirlist[] = {
  { 6,&macroname[0] }, { 5,&macroname[1] }, { 0,0 }
};
static struct namelen endm_dirlist[] = {
  { 5,&endmname[0] }, { 4,&endmname[1] }, { 0,0 }
};
static struct namelen rept_dirlist[] = {
  { 5,&reptname[0] }, { 4,&reptname[1] }, { 0,0 }
};
static struct namelen endr_dirlist[] = {
  { 5,&endrname[0] }, { 4,&endrname[1] }, { 0,0 }
};

static char *labname;  /* current label field for assignment directives */


char *skip(char *s)
{
  while (isspace((unsigned char )*s))
    s++;
  return s;
}


void eol(char *s)
{
  s = skip(s);
  if (!ISEOL(s))
    syntax_error(6);
}


char *skip_operand(char *s)
{
  int par_cnt = 0;
  char c;

  for (;;) {
    c = *s;

    if (START_PARENTH(c)) {
      par_cnt++;
    }
    else if (END_PARENTH(c)) {
      if (par_cnt > 0)
        par_cnt--;
      else
        syntax_error(3);  /* too many closing parentheses */
    }
    else if (c=='\'' || c=='\"')
      s = skip_string(s,c,NULL) - 1;
    else if (!c || (par_cnt==0 && (c==',' || c==commentchar)))
      break;

    s++;
  }

  if (par_cnt != 0)
    syntax_error(4);  /* missing closing parentheses */
  return s;
}


char *const_prefix(char *s,int *base)
{
  if (isdigit((unsigned char)*s)) {
    *base = 10;
    return s;
  }
  if (*s == '$' && isxdigit((unsigned char)*(s+1))) {
    *base = 16;
    return s+1;
  }
  if (*s=='@' && isdigit((unsigned char)*(s+1))) {
    *base = 8;
    return s+1;
  }
  if (*s == '%') {
    *base = 2;
    return s+1;
  }
  *base = 0;
  return s;
}


char *const_suffix(char *start,char *end)
{
  return end;
}


char *get_local_label(char **start)
/* local labels start with '.' */
{
  char *name = *start;
  char *end;

  if (*name == '.') {
    end = skip_identifier(name+1);
    if (end != NULL) {
      *start = skip(end);
      return make_local_label(NULL,0,name,end-name);
    }
  }
  return NULL;
}


/* Directives */

static void handle_equ(char *s)
{
  int exp;
  symbol *sym;

  if (*s=='=' && *(s-1)=='=') {
    s = skip(s+1);
    exp = 1;  /* == automatically exports a symbol */
  }
  else
    exp = 0;

  sym = new_equate(labname,parse_expr_tmplab(&s));
  if (exp) {
    if (is_local_label(labname))
      syntax_error(1);  /* cannot export local symbol */
    sym->flags |= EXPORT;
  }
  eol(s);
}


static void handle_set(char *s)
{
  new_abs(labname,parse_expr_tmplab(&s));
  eol(s);
}


static int do_cond(char **s)
{
  expr *condexp = parse_expr_tmplab(s);
  taddr val;

  if (!eval_expr(condexp,&val,NULL,0)) {
    general_error(30);  /* expression must be constant */
    val = 0;
  }
  free_expr(condexp);
  return val != 0;
}


static void handle_if(char *s)
{
  cond_if(do_cond(&s));
  eol(s);
}


static void handle_else(char *s)
{
  cond_skipelse();
  eol(s);
}


static void handle_endif(char *s)
{
  cond_endif();
  eol(s);
}


static void do_section(char *s,char *name,char *type)
{
  try_end_rorg();  /* works like ending the last RORG-block */
  set_section(new_section(name,type,1));
  eol(s);
}


static void handle_text(char *s)
{
  do_section(s,text_name,text_type);
}


static void handle_data(char *s)
{
  do_section(s,data_name,data_type);
}


static void handle_bss(char *s)
{
  do_section(s,bss_name,bss_type);
}


static void handle_org(char *s)
{
  if (current_section!=NULL && !(current_section->flags & ABSOLUTE))
    start_rorg(parse_constexpr(&s));
  else
    set_section(new_org(parse_constexpr(&s)));
  eol(s);
}


static void handle_68000(char *s)
{
  try_end_rorg();  /* works like ending the last RORG-block */
  eol(s);
}


static void handle_globl(char *s)
{
  char *name;
  symbol *sym;

  for (;;) {
    if (!(name = parse_identifier(&s))) {
      syntax_error(10);  /* identifier expected */
      return;
    }
    sym = new_import(name);
    myfree(name);

    if (sym->flags & EXPORT)
      general_error(62,sym->name,get_bind_name(sym)); /* binding already set */
    sym->flags |= EXPORT;

    s = skip(s);
    if (*s == ',')
      s = skip(s+1);
    else
      break;
  }

  eol(s);
}


static void handle_assert(char *s)
{
  char *expstr = s;
  expr *aexp = NULL;

  do {
    s = skip(s);
    if (aexp)  /* concat. expressions with log. AND */
      aexp = make_expr(LAND,aexp,parse_expr(&s));
    else
      aexp = parse_expr(&s);
    simplify_expr(aexp);
    s = skip(s);
  }
  while (*s++ == ',');  /* another assertion, separated by comma? */

  s--;
  add_atom(0,new_assert_atom(aexp,cnvstr(expstr,s-expstr),NULL));
  eol(s);
}


static void handle_datadef(char *s,int sz)
{
  for (;;) {
    char *opstart = s;
    operand *op;
    dblock *db = NULL;

    if (OPSZ_BITS(sz)==8 && (*s=='\"' || *s=='\'')) {
      if (db = parse_string(&opstart,*s,8)) {
        add_atom(0,new_data_atom(db,1));
        s = opstart;
      }
    }
    if (!db) {
      op = new_operand();
      s = skip_operand(s);
      if (parse_operand(opstart,s-opstart,op,DATA_OPERAND(sz)))
        add_atom(0,new_datadef_atom(OPSZ_BITS(sz),op));
      else
        syntax_error(8);  /* invalid data operand */
    }

    s = skip(s);
    if (*s == ',')
      s = skip(s+1);
    else {
      eol(s);
      break;
    }
  }
}


static void handle_d8(char *s)
{
  handle_datadef(s,8);
}


static void handle_d16(char *s)
{
  handle_datadef(s,16);
}


static void handle_d32(char *s)
{
  handle_datadef(s,32);
}


static void handle_i32(char *s)
{
  handle_datadef(s,OPSZ_SWAP|32);
}


static void do_space(int size,expr *cnt,expr *fill)
{
  atom *a;

  a = new_space_atom(cnt,size>>3,fill);
  a->align = DATA_ALIGN(size);
  add_atom(0,a);
}


static void handle_space(char *s,int size)
{
  do_space(size,parse_expr_tmplab(&s),0);
  eol(s);
}


static void handle_spc8(char *s)
{
  handle_space(s,8);
}


static void handle_spc16(char *s)
{
  handle_space(s,16);
}


static void handle_spc32(char *s)
{
  handle_space(s,32);
}


static void handle_block(char *s,int size)
{
  expr *cnt,*fill=0;

  cnt = parse_expr_tmplab(&s);
  s = skip(s);
  if (*s == ',') {
    s = skip(s+1);
    fill = parse_expr_tmplab(&s);
  }
  do_space(size,cnt,fill);
  eol(s);
}


static void handle_blk8(char *s)
{
  handle_block(s,8);
}


static void handle_blk16(char *s)
{
  handle_block(s,16);
}


static void handle_blk32(char *s)
{
  handle_block(s,32);
}


static void handle_end(char *s)
{
  end_source(cur_src);
  eol(s);
}


static void do_alignment(taddr align,expr *offset,size_t pad,expr *fill)
{
  atom *a = new_space_atom(offset,pad,fill);

  a->align = align;
  add_atom(0,a);
}


static void handle_even(char *s)
{
  do_alignment(2,number_expr(0),1,NULL);
  eol(s);
}


static void handle_long(char *s)
{
  do_alignment(4,number_expr(0),1,NULL);
  eol(s);
}


static void handle_phrase(char *s)
{
  do_alignment(8,number_expr(0),1,NULL);
  eol(s);
}


static void handle_dphrase(char *s)
{
  do_alignment(16,number_expr(0),1,NULL);
  eol(s);
}


static void handle_qphrase(char *s)
{
  do_alignment(32,number_expr(0),1,NULL);
  eol(s);
}


static void handle_include(char *s)
{
  char *name;

  if (name = parse_name(&s)) {
    eol(s);
    include_source(name);
  }
}


static void handle_incbin(char *s)
{
  char *name;

  if (name = parse_name(&s))
    include_binary_file(name,0,0);
  eol(s);
}


static void handle_rept(char *s)
{
  new_repeat((utaddr)parse_constexpr(&s),NULL,NULL,rept_dirlist,endr_dirlist);
  eol(s);
}


static void handle_endr(char *s)
{
  syntax_error(12,endrname,reptname);  /* unexpected endr without rept */
}


static void handle_list(char *s)
{
  set_listing(1);
  eol(s);
}


static void handle_nlist(char *s)
{
  set_listing(0);
  eol(s);
}


static void handle_macro(char *s)
{
  char *name;

  if (name = parse_identifier(&s)) {
    s = skip(s);
    if (ISEOL(s))
      s = NULL;  /* no named arguments */
    new_macro(name,macro_dirlist,endm_dirlist,s);
    myfree(name);
  }
  else
    syntax_error(10);  /* identifier expected */
}


static void handle_macundef(char *s)
{
  char *name;

  while (name = parse_identifier(&s)) {
    undef_macro(name);
    myfree(name);
    s = skip(s);
    if (*s != ',')
      break;
    s = skip(s+1);
  }
  eol(s);
}


static void handle_endm(char *s)
{
  syntax_error(12,endmname,macroname);  /* unexpected endm without macro */
}


static void handle_exitm(char *s)
{
  leave_macro();
  eol(s);
}


static void handle_print(char *s)
{
  while (!ISEOL(s)) {
    if (*s == '\"') {
      size_t len;
      char *txt;

      skip_string(s,'\"',&len);
      if (len > 0) {
        txt = mymalloc(len+1);
        s = read_string(txt,s,'\"',8);
        txt[len] = '\0';
        add_atom(0,new_text_atom(txt));
      }
    }
    else {
      int type = PEXP_HEX;
      int size = 16;

      while (*s == '/') {
        /* format character */
        char f;

        s = skip(s+1);
        f = tolower((unsigned char)*s);
        if (s = skip_identifier(s)) {
          switch (f) {
            case 'x':
              type = PEXP_HEX;
              break;
            case 'd':
              type = PEXP_SDEC;
              break;
            case 'u':
              type = PEXP_UDEC;
              break;
            case 'w':
              size = 16;
              break;
            case 'l':
              size = 32;
              break;
            default:
              syntax_error(7,f);  /* unknown print format flag */
              break;
          }
        }
        else {
          syntax_error(9);  /* print format corrupted */
          break;
        }
        s = skip(s);
      }
      add_atom(0,new_expr_atom(parse_expr(&s),type,size));
    }
    s = skip(s);
    if (*s != ',')
      break;
    s = skip(s+1);
  }
  add_atom(0,new_text_atom(NULL));  /* new line */
  eol(s);
}


static void handle_offset(char *s)
{
  taddr offs;

  if (!ISEOL(s))
    offs = parse_constexpr(&s);
  else
    offs = 0;

  try_end_rorg();
  switch_offset_section(NULL,offs);
}



struct {
  char *name;
  void (*func)(char *);
} directives[] = {
  "equ",handle_equ,
  "=",handle_equ,
  "set",handle_set,
  "if",handle_if,
  "else",handle_else,
  "endif",handle_endif,
  "rept",handle_rept,
  "endr",handle_endr,
  "macro",handle_macro,
  "endm",handle_endm,
  "exitm",handle_exitm,
  "macundef",handle_macundef,
  "text",handle_text,
  "data",handle_data,
  "bss",handle_bss,
  "org",handle_org,
  "68000",handle_68000,
  "globl",handle_globl,
  "extern",handle_globl,
  "assert",handle_assert,
  "dc",handle_d16,
  "dc.b",handle_d8,
  "dc.w",handle_d16,
  "dc.l",handle_d32,
  "dc.i",handle_i32,
  "dcb",handle_blk16,
  "dcb.b",handle_blk8,
  "dcb.w",handle_blk16,
  "dcb.l",handle_blk32,
  "ds",handle_spc16,
  "ds.b",handle_spc8,
  "ds.w",handle_spc16,
  "ds.l",handle_spc32,
  "end",handle_end,
  "even",handle_even,
  "long",handle_long,
  "phrase",handle_phrase,
  "dphrase",handle_dphrase,
  "qphrase",handle_qphrase,
  "include",handle_include,
  "incbin",handle_incbin,
  "list",handle_list,
  "nlist",handle_nlist,
  "nolist",handle_nlist,
  "print",handle_print,
#ifndef VASM_CPU_JAGRISC
  "abs",handle_offset,
#endif
  "offset",handle_offset
};

int dir_cnt = sizeof(directives) / sizeof(directives[0]);


/* checks for a valid directive, and return index when found, -1 otherwise */
static int check_directive(char **line)
{
  char *s,*name;
  hashdata data;

  s = skip(*line);
  if (!ISIDSTART(*s) && *s!='=')
    return -1;
  name = s++;
  while (ISIDCHAR(*s) || *s=='.')
    s++;
  if (*name=='.') {  /* leading dot is optional for all directives */
    name++;
    dotdirs = 1;
  }
  else
    dotdirs = 0;
  if (!find_namelen_nc(dirhash,name,s-name,&data))
    return -1;
  *line = s;
  return data.idx;
}


/* Handles assembly directives; returns non-zero if the line
   was a directive. */
static int handle_directive(char *line)
{
  int idx = check_directive(&line);

  if (idx >= 0) {
    directives[idx].func(skip(line));
    return 1;
  }
  return 0;
}


static int oplen(char *e,char *s)
{
  while (s!=e && isspace((unsigned char)e[-1]))
    e--;
  return e-s;
}


void parse(void)
{
  char *s,*line,*inst;
  char *ext[MAX_QUALIFIERS?MAX_QUALIFIERS:1];
  char *op[MAX_OPERANDS];
  int ext_len[MAX_QUALIFIERS?MAX_QUALIFIERS:1];
  int op_len[MAX_OPERANDS];
  int ext_cnt,op_cnt,inst_len;
  instruction *ip;

  while (line = read_next_line()) {

    if (!cond_state()) {
      /* skip source until ELSE or ENDIF */
      int idx = -1;

      s = skip(line);
      if (labname = parse_labeldef(&s,1)) {
        if (*s == ':')
          s++;  /* skip double-colon */
        myfree(labname);
        s = skip(s);
      }
      else {
        if (inst = skip_identifier(s)) {
          inst = skip(inst);
          idx = check_directive(&inst);
        }
      }
      if (idx < 0)
        idx = check_directive(&s);
      if (idx >= 0) {
        if (directives[idx].func == handle_if)
          cond_skipif();
        else if (directives[idx].func == handle_else)
          cond_else();
        else if (directives[idx].func == handle_endif)
          cond_endif();
      }
      continue;
    }

    s = skip(line);
again:
    if (*s=='\0' || *line=='*' || *s==commentchar)
      continue;

    if (labname = parse_labeldef(&s,1)) {
      /* we have found a valid global or local label */
      symbol *sym = new_labsym(0,labname);

      if (*s == ':') {
        /* double colon automatically declares label as exported */
        sym->flags |= EXPORT;
        s++;
      }
      add_atom(0,new_label_atom(sym));
      myfree(labname);
      s = skip(s);
    }
    else {
      /* there can still be a sym. in the 1st fld and an assignm. directive */
      inst = s;
      labname = parse_symbol(&s);
      if (labname == NULL) {
        syntax_error(10);  /* identifier expected */
        continue;
      }
      s = skip(s);

      /* Now we have labname pointing to the first field in the line
         and s pointing to the second. Find out where the directive is. */
      if (!ISEOL(s)) {
#ifdef PARSE_CPU_LABEL
        if (PARSE_CPU_LABEL(labname,&s)) {
          myfree(labname);
          continue;
        }
#endif
        if (handle_directive(s)) {
          myfree(labname);
          continue;
        }
      }

      /* directive or mnemonic must be in the first field */
      myfree(labname);
      s = inst;
    }

    if (!strnicmp(s,".iif",4) || !(strnicmp(s,"iif",3))) {
      /* immediate conditional assembly: parse line after ',' when true */
      s = skip(*s=='.'?s+4:s+3);
      if (do_cond(&s)) {
        s = skip(s);
        if (*s == ',') {
          s = skip(s+1);
          goto again;
        }
        else
          syntax_error(0);  /* malformed immediate-if */
      }
      continue;
    }

    /* check for directives */
    s = parse_cpu_special(s);
    if (ISEOL(s))
      continue;

    if (handle_directive(s))
      continue;

    /* read mnemonic name */
    inst = s;
    ext_cnt = 0;
    if (!ISIDSTART(*s)) {
      syntax_error(10);  /* identifier expected */
      continue;
    }
#if MAX_QUALIFIERS==0
    while (*s && !isspace((unsigned char)*s))
      s++;
    inst_len = s - inst;
#else
    s = parse_instruction(s,&inst_len,ext,ext_len,&ext_cnt);
#endif
    if (!isspace((unsigned char)*s) && *s!='\0')
      syntax_error(2);  /* no space before operands */
    s = skip(s);

    if (execute_macro(inst,inst_len,ext,ext_len,ext_cnt,s))
      continue;

    /* read operands, terminated by comma or blank (unless in parentheses) */
    op_cnt = 0;
    while (!ISEOL(s) && op_cnt<MAX_OPERANDS) {
      op[op_cnt] = s;
      s = skip_operand(s);
      op_len[op_cnt] = oplen(s,op[op_cnt]);
#if !ALLOW_EMPTY_OPS
      if (op_len[op_cnt] <= 0)
        syntax_error(5);  /* missing operand */
      else
#endif
        op_cnt++;
      s = skip(s);
      if (*s != ',')
        break;
      else
        s = skip(s+1);
    }
    eol(s);

    ip = new_inst(inst,inst_len,op_cnt,op,op_len);

#if MAX_QUALIFIERS>0
    if (ip) {
      int i;

      for (i=0; i<ext_cnt; i++)
        ip->qualifiers[i] = cnvstr(ext[i],ext_len[i]);
      for(; i<MAX_QUALIFIERS; i++)
        ip->qualifiers[i] = NULL;
    }
#endif

    if (ip)
      add_atom(0,new_inst_atom(ip));
  }

  cond_check();  /* check for open conditional blocks */
}


/* parse next macro argument */
char *parse_macro_arg(struct macro *m,char *s,
                      struct namelen *param,struct namelen *arg)
{
  arg->len = 0;  /* cannot select specific named arguments */
  param->name = s;
  s = skip_operand(s);
  param->len = s - param->name;
  return s;
}


/* write 0 to buffer when macro argument is missing or empty, 1 otherwise */
static int macro_arg_defined(source *src,char *argstart,char *argend,char *d)
{
  int n = find_macarg_name(src,argstart,argend-argstart);

  if (n >= 0) {
    /* valid argument name */
    *d++ = (n<src->num_params && n<maxmacparams && src->param_len[n]>0) ?
           '1' : '0';
    return 1;
  }
  return 0;
}


/* expands arguments and special escape codes into macro context */
int expand_macro(source *src,char **line,char *d,int dlen)
{
  int nc = 0;
  int n;
  char *s = *line;
  char *end;

  if (*s++ == '\\') {
    /* possible macro expansion detected */

    if (*s == '\\') {
      if (dlen >= 1) {
        *d++ = *s++;
        if (esc_sequences) {
          if (dlen >= 2) {
            *d++ = '\\';  /* make it a double \ again */
            nc = 2;
          }
          else nc = -1;
        }
        else nc = 1;
      }
      else nc = -1;
    }

    else if (*s == '~') {
      /* \~: insert a unique id */
      if (dlen > 10) {
        nc = sprintf(d,"M%lu",src->id);
        s++;
      }
      else nc = -1;
    }
    else if (*s == '#') {
      /* \# : insert number of parameters */
      if (dlen > 3) {
        nc = sprintf(d,"%d",src->num_params);
        s++;
      }
      else nc = -1;
    }
    else if (*s == '!') {
      /* \!: copy qualifier (dot-size) */
      if (dlen > 1) {
        *d++ = '.';
        if ((nc = copy_macro_qual(src,0,d,dlen)) >= 0)
          nc++;
        s++;
      }
      else
        nc = -1;
    }
    else if (isdigit((unsigned char)*s)) {
      /* \1..\9,\0 : insert macro parameter 1..9,10 */
      nc = copy_macro_param(src,*s=='0'?0:*s-'1',d,dlen);
      s++;
    }
    else if (*s=='?' && dlen>=1) {
      if ((end = skip_identifier(s+1)) != NULL) {
        /* \?argname : 1 when argument defined, 0 when missing or empty */
        if ((nc = macro_arg_defined(src,s+1,end,d)) >= 0)
          s = end;
      }
      else if (*(s+1)=='{' && (end = skip_identifier(s+2))!=NULL) {
        if (*end == '}') {
          /* \?{argname} : 1 when argument defined, 0 when missing or empty */
          if ((nc = macro_arg_defined(src,s+2,end,d)) >= 0)
            s = end + 1;
        }
      }
    }
    else if (*s=='{' && (end = skip_identifier(s+1))!=NULL) {
      if (*end == '}') {
        if ((n = find_macarg_name(src,s+1,end-(s+1))) >= 0) {
          /* \{argname}: insert macro parameter n */
          nc = copy_macro_param(src,n,d,dlen);
          s = end + 1;
        }
      }
    }
    else if ((end = skip_identifier(s)) != NULL) {
      if ((n = find_macarg_name(src,s,end-s)) >= 0) {
        /* \argname: insert named macro parameter n */
        nc = copy_macro_param(src,n,d,dlen);
        s = end;
      }
    }

    if (nc >= dlen)
      nc = -1;
    else if (nc >= 0)
      *line = s;  /* update line pointer when expansion took place */
  }

  return nc;  /* number of chars written to line buffer, -1: out of space */
}


int init_syntax()
{
  size_t i;
  hashdata data;

  dirhash = new_hashtable(0x200);
  for (i=0; i<dir_cnt; i++) {
    data.idx = i;
    add_hashentry(dirhash,directives[i].name,data);
  }
  
  cond_init();
  current_pc_char = '*';
  esc_sequences = 1;

  /* assertion errors are only a warning */
  modify_gen_err(WARNING,47,0);

  return 1;
}


int syntax_args(char *p)
{
  return 0;
}
