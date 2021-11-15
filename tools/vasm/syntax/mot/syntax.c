/* syntax.c  syntax module for vasm */
/* (c) in 2002-2021 by Frank Wille */

#include "vasm.h"

/* The syntax module parses the input (read_next_line), handles
   assembly-directives (section, data-storage etc.) and parses
   mnemonics. Assembly instructions are split up in mnemonic name,
   qualifiers and operands. new_inst returns a matching instruction,
   if one exists.
   Routines for creating sections and adding atoms to sections will
   be provided by the main module.
*/

char *syntax_copyright="vasm motorola syntax module 3.15b (c) 2002-2021 Frank Wille";
hashtable *dirhash;
char commentchar = ';';
int dotdirs;

static char code_name[] = "CODE";
static char data_name[] = "DATA";
static char bss_name[] = "BSS";
static char code_type[] = "acrx";
static char data_type[] = "adrw";
static char bss_type[] = "aurw";
static char rs_name[] = "__RS";
static char so_name[] = "__SO";
static char fo_name[] = "__FO";
static char line_name[] = "__LINE__";
char *defsectname = code_name;
char *defsecttype = code_type;

static struct namelen macro_dirlist[] = {
  { 5,"macro" }, { 0,0 }
};
static struct namelen endm_dirlist[] = {
  { 4,"endm" }, { 0,0 }
};
static struct namelen rept_dirlist[] = {
  { 4,"rept" }, { 0,0 }
};
static struct namelen endr_dirlist[] = {
  { 4,"endr" }, { 0,0 }
};
static struct namelen erem_dirlist[] = {
  { 4,"erem" }, { 0,0 }
};

/* options */
static int allmp;
static int align_data;
static int phxass_compat;
static int devpac_compat;
static int allow_spaces;
static int check_comm;
static int dot_idchar;
static char local_char = '.';

/* (currenty two-byte only) padding value for CNOPs */
#ifdef VASM_CPU_M68K
static taddr cnop_pad = 0x4e71;
#else
static taddr cnop_pad = 0;
#endif

/* unique macro IDs */
#define IDSTACKSIZE 100
static unsigned long id_stack[IDSTACKSIZE];
static int id_stack_index;

/* isolated local labels block */
#define INLSTACKSIZE 100
#define INLLABFMT "=%06d"
static int inline_stack[INLSTACKSIZE];
static int inline_stack_index;
static char *saved_last_global_label;
static char inl_lab_name[8];

static int parse_end = 0;
static expr *carg1;


char *skip(char *s)
{
  while (isspace((unsigned char )*s))
    s++;
  return s;
}


int iscomment(char *s)
{
  if (phxass_compat) {
    /* PhxAss can also introduce comments with * in operands and expressions,
       provided that is follows a space character. */
    char *start = s;

    s = skip(start);
    if (s>start && *s=='*' && isspace((unsigned char )*(s-1)))
      return 1;
  }
  return *s == commentchar;
}


/* issue a warning for comments introduced by blanks in the operand field */
static void comment_check(char *s)
{
  if (isspace((unsigned char)*s)) {
    s = skip(s + 1);
    if (!ISEOL(s))
      syntax_error(18);  /* check comment warning */
  }
}


/* check for end of line, issue error, if not */
void eol(char *s)
{
  if (allow_spaces) {
    s = skip(s);
    if (!ISEOL(s))
      syntax_error(6);
  }
  else {
    if (!ISEOL(s) && !isspace((unsigned char)*s))
      syntax_error(6);
  }
}


int isidchar(char c)
{
  if (isalnum((unsigned char)c) || c=='_' || c=='$' || c=='%')
    return 1;
  if (dot_idchar && c=='.')
    return 1;
  if (phxass_compat && (unsigned char)c>=0x80)
    return 1;
  if (devpac_compat && (c=='?' || c=='@'))
    return 1;
  return 0;
}


#ifdef VASM_CPU_M68K
char *chkidend(char *start,char *end)
{
  if (dot_idchar && (end-start)>2 && *(end-2)=='.') {
    char c = tolower((unsigned char)*(end-1));

    if (c=='b' || c=='w' || c=='l')
      return end - 2;	/* .b/.w/.l extension is not part of identifier */
  }
  return end;
}
#endif


char *exp_skip(char *s)
{
  if (allow_spaces) {
    char *start = s;

    s = skip(start);
    if (phxass_compat && s>start && *s=='*' && isspace((unsigned char )*(s-1)))
      *s = '\0';  /* rest of operand is ignored */
  }
  else if (isspace((unsigned char)*s)) {
    if (check_comm)
      comment_check(s);
    *s = '\0';  /* rest of operand is ignored */
  }
  return s;
}


char *skip_operand(char *s)
{
  int par_cnt = 0;
  char c;

  for (;;) {
    s = exp_skip(s);
    c = *s;

    if (START_PARENTH(c)) {
      par_cnt++;
    }
    else if (END_PARENTH(c)) {
      if (par_cnt>0)
        par_cnt--;
      else
        syntax_error(3);  /* too many closing parentheses */
    }
    else if (c=='\'' || c=='\"')
      s = skip_string(s,c,NULL) - 1;
    else if (!c || (par_cnt==0 && (c==',' || iscomment(s))))
      break;

    s++;
  }

  if (par_cnt != 0)
    syntax_error(4);  /* missing closing parentheses */
  return s;
}


/* assign value of current struct- or frame-offset symbol to an abs-symbol,
   or just increment/decrement when equname is NULL */
static symbol *new_setoffset_size(char *equname,char *symname,
                                  char **s,int dir,taddr size)
{
  symbol *sym,*equsym;
  expr *new,*old;

  /* get current offset symbol expression, then increment or decrement it */
  sym = internal_abs(symname);

  if (!ISEOL(*s)) {
    /* Make a new expression out of the parsed expression multiplied by size
       and add to or subtract it from the current symbol's expression.
       Perform even alignment when requested. */
    new = make_expr(MUL,parse_expr_tmplab(s),number_expr(size));
    simplify_expr(new);

    if (align_data && size>1) {
      /* align the current offset symbol first */
      utaddr dalign = DATA_ALIGN((int)size*8) - 1;

      old = make_expr(BAND,
                      make_expr(dir>0?ADD:SUB,sym->expr,number_expr(dalign)),
                      number_expr(~dalign));
      simplify_expr(old);
    }
    else
      old = sym->expr;

    new = make_expr(dir>0?ADD:SUB,old,new);
  }
  else {
    new = old = sym->expr;
    if (devpac_compat)
      general_error(9);  /* Devpac requires an expression here */
  }

  /* assign expression to equ-symbol and change exp. of the offset-symbol */
  if (equname)
    equsym = new_equate(equname,dir>0 ? copy_tree(old) : copy_tree(new));
  else
    equsym = NULL;

  simplify_expr(new);
  sym->expr = new;
  return equsym;
}


/* assign value of current struct- or frame-offset symbol to an abs-symbol,
   determine operation size from directive extension first */
static symbol *new_setoffset(char *equname,char **s,char *symname,int dir)
{
  taddr size = 1;
  char *start = *s;
  char ext;

  /* get extension character and proceed to operand */
  if (*(start+2) == '.') {
    ext = tolower((unsigned char)*(start+3));
    *s = skip(start+4);
    switch (ext) {
      case 'b':
        break;
      case 'w':
        size = 2;
        break;
      case 'l':
      case 's':
        size = 4;
        break;
      case 'q':
      case 'd':
        size = 8;
        break;
      case 'x':
        size = 12;
        break;
      default:
        syntax_error(1);  /* invalid extension */
        break;
    }
  }
  else {
    size = 2;  /* defaults to 'w' extension when missing */
    *s = skip(start+2);
  }

  return new_setoffset_size(equname,symname,s,dir,size);
}


static atom *do_space(int size,expr *cnt,expr *fill)
{
  atom *a;

  a = new_space_atom(cnt,size>>3,fill);
  a->align = align_data ? DATA_ALIGN(size) : 1;
  add_atom(0,a);
  return a;
}


static void handle_space(char *s,int size)
{
  do_space(size,parse_expr_tmplab(&s),0);
}


static void handle_uspace(char *s,int size)
{
  atom *a = do_space(size,parse_expr_tmplab(&s),0);
  a->content.sb->flags |= SPC_UNINITIALIZED;
}


static void handle_dbss(char *s,int size)
{
  atom *a = do_space(size,parse_expr_tmplab(&s),0);
  a->content.sb->flags |= SPC_DATABSS;
}


static char *read_sec_attr(char *attr,char *s,uint32_t *mem)
{
  char *type = s;

  if (!(s = skip_identifier(s))) {
    syntax_error(10);  /* identifier expected */
    return NULL;
  }

  if ((s-type==3 || s-type==5) && !strnicmp(type,"bss",3))
    strcpy(attr,bss_type);
  else if ((s-type==4 || s-type==6) && !strnicmp(type,"data",4))
    strcpy(attr,data_type);
  else if ((s-type==4 || s-type==6) &&
           (!strnicmp(type,"code",4) || !strnicmp(type,"text",4)))
    strcpy(attr,code_type);
  else {
    syntax_error(13);  /* illegal section type */
    return NULL;
  }

  if (s-type==5 || s-type==6) {
    if (*(s-2) == '_') {
      switch (tolower((unsigned char)*(s-1))) {
        case 'c':
          *mem = 2;  /* AmigaDOS MEMF_CHIP */
          break;
        case 'f':
          *mem = 4;  /* AmigaDOS MEMF_FAST */
          break;
        case 'p':
          break;
        default:
          syntax_error(13);
          return NULL;
      }
    }
    else {
      syntax_error(13);  /* illegal section type */
      return NULL;
    }
  }

  s = skip(s);
  if (*s == ',') {
    /* read optional memory type */
    taddr mc;

    s = skip(s+1);
    type = s;

    /* check for "chip" or "fast" memory type (AmigaDOS) */
    if (s = skip_identifier(s)) {
      if (s-type==4 && !strnicmp(type,"chip",4)) {
        *mem = 2;  /* AmigaDOS MEMF_CHIP */
        return skip(s);
      }
      else if (s-type==4 && !strnicmp(type,"fast",4)) {
        *mem = 4;  /* AmigaDOS MEMF_FAST */
        return skip(s);
      }
    }

    /* try to read a numerical memory type constant */
    s = type;
    mc = parse_constexpr(&type);
    if (type>s && mc!=0)
      *mem = (uint32_t)mc;
    else
      syntax_error(15);  /* illegal memory type */
    s = skip(type);
  }

  return s;
}


static void motsection(section *sec,uint32_t mem)
/* mot-syntax specific section initializations on a new section */
{
  if (!devpac_compat)
    try_end_rorg();  /* end a potential ORG block */

#if NOT_NEEDED
  if (phxass_compat!=0 && strchr(sec->attr,'c')!=NULL) {
    /* CNOP alignments pad with NOP instruction in code sections */
    sec->padbytes = 2;
    sec->pad[0] = 0x4e;
    sec->pad[1] = 0x71;
  }
#endif

  /* set optional memory attributes (e.g. AmigaOS hunk format Chip/Fast) */
  sec->memattr = mem;

  /* a section named __MERGED allows base-relative addressing optimizations */
  if (!strcmp(sec->name,"__MERGED"))
    sec->flags |= NEAR_ADDRESSING;
}


static void handle_section(char *s)
{
  char attr[32];
  char *name;
  uint32_t mem = 0;

  /* read section name */
  if (!(name = parse_name(&s)))
    return;

  if (*s == ',') {
    /* read section type and memory attributes */
    s = read_sec_attr(attr,skip(s+1),&mem);
  }
  else if (unnamed_sections) {
    /* only name is given - guess type from the name (i.e. name is type) */
    if (!stricmp(name,"data")) {
      strcpy(attr,data_type);
      name = data_name;
    }
    else if (!stricmp(name,"bss")) {
      strcpy(attr,bss_type);
      name = bss_name;
    }
    else if (!stricmp(name,"code") || !stricmp(name,"text")) {
      strcpy(attr,code_type);
      name = code_name;
    }
    else {
      syntax_error(13);  /* illegal section type */
      s = NULL;
    }
  }
  else {
    /* otherwise a missing section type defaults to CODE */
    strcpy(attr,code_type);
  }

  if (s) {
    motsection(new_section(name,attr,1),mem);
    switch_section(name,attr);
  }
}


static void handle_offset(char *s)
{
  taddr offs;

  if (!ISEOL(s))
    offs = parse_constexpr(&s);
  else
    offs = -1;  /* use last offset */

  if (!devpac_compat)
    try_end_rorg();
  switch_offset_section(NULL,offs);
}


static void nameattrsection(char *secname,char *sectype,uint32_t mem)
/* switch to a section called secname, with attributes sectype+addattr */
{
  motsection(new_section(secname,sectype,1),mem);
  switch_section(secname,sectype);
}

static void handle_csec(char *s)
{
  nameattrsection(code_name,code_type,0);
}

static void handle_dsec(char *s)
{
  nameattrsection(data_name,data_type,0);
}

static void handle_bss(char *s)
{
  nameattrsection(bss_name,bss_type,0);
}

static void handle_codec(char *s)
{
  nameattrsection("CODE_C",code_type,2);  /* AmigaDOS MEMF_CHIP */
}

static void handle_codef(char *s)
{
  nameattrsection("CODE_F",code_type,4);  /* AmigaDOS MEMF_FAST */
}

static void handle_datac(char *s)
{
  nameattrsection("DATA_C",data_type,2);  /* AmigaDOS MEMF_CHIP */
}

static void handle_dataf(char *s)
{
  nameattrsection("DATA_F",data_type,4);  /* AmigaDOS MEMF_FAST */
}

static void handle_bssc(char *s)
{
  nameattrsection("BSS_C",bss_type,2);  /* AmigaDOS MEMF_CHIP */
}

static void handle_bssf(char *s)
{
  nameattrsection("BSS_F",bss_type,4);  /* AmigaDOS MEMF_FAST */
}


static void handle_org(char *s)
{
  if (*s == '*') {    /*  "* = * + <expr>" reserves bytes */
    s = skip(s+1);
    if (*s == '+')
      handle_uspace(skip(s+1),8);
    else
      syntax_error(7);  /* syntax error */
  }
  else {
    if (current_section!=NULL && !(current_section->flags & ABSOLUTE))
      start_rorg(parse_constexpr(&s));
    else
      set_section(new_org(parse_constexpr(&s)));
  }
}


static void handle_rorg(char *s)
{
  expr *offs = parse_expr_tmplab(&s);
  expr *fill;

  s = skip(s);
  if (*s == ',') {
    /* may be followed by an optional fill-value */
    s = skip(s+1);
    fill = parse_expr_tmplab(&s);
  }
  else
    fill = NULL;
  add_atom(0,new_roffs_atom(offs,fill));
}


static void do_bind(char *s,int bind)
{
  symbol *sym;
  char *name;

  do {
    s = skip(s);
    if (!(name = parse_identifier(&s))) {
      syntax_error(10);  /* identifier expected */
      return;
    }
    sym = new_import(name);
    myfree(name);
    if ((sym->flags & (EXPORT|WEAK|NEAR)) != 0 &&
        (sym->flags & (EXPORT|WEAK|NEAR)) != bind)
      general_error(62,sym->name,get_bind_name(sym)); /* binding already set */
    else
      sym->flags |= bind;
    s = skip(s);
  }
  while (*s++ == ',');
}


static void handle_global(char *s)
{
  do_bind(s,EXPORT);
}


static void handle_weak(char *s)
{
  do_bind(s,WEAK);
}


static void handle_nref(char *s)
{
  do_bind(s,EXPORT|NEAR);
}


static void handle_comm(char *s)
{
  char *name = parse_identifier(&s);
  symbol *sym;
  taddr sz = 4;

  if (name == NULL) {
    syntax_error(10);  /* identifier expected */
    return;
  }
  sym = new_import(name);
  sym->flags |= COMMON;
  myfree(name);

  s = skip(s);
  if (*s == ',') {
    s = skip(s+1);
    sz = parse_constexpr(&s);
  }
  else
    syntax_error(9);  /* , expected */

  sym->size = number_expr(sz);
  sym->align = 4;
}


static void handle_data(char *s,int size)
{
  /* size is negative for floating point data! */
  for (;;) {
    char *opstart = s;
    operand *op;
    dblock *db = NULL;

    if (OPSZ_BITS(size)==8 && (*s=='\"' || *s=='\'')) {
      if (db = parse_string(&opstart,*s,8)) {
        add_atom(0,new_data_atom(db,1));
        s = opstart;
      }
    }
    if (!db) {
      op = new_operand();
      s = skip_operand(s);
      if (parse_operand(opstart,s-opstart,op,DATA_OPERAND(size))) {
        atom *a;

        a = new_datadef_atom(OPSZ_BITS(size),op);
        if (!align_data)
          a->align = 1;
        add_atom(0,a);
      }
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
  handle_data(s,8);
}


static void handle_d16(char *s)
{
  handle_data(s,16);
}


static void handle_d32(char *s)
{
  handle_data(s,32);
}


static void handle_d64(char *s)
{
  handle_data(s,64);
}


#if FLOAT_PARSER
static void handle_f32(char *s)
{
  handle_data(s,OPSZ_FLOAT|32);
}


static void handle_f64(char *s)
{
  handle_data(s,OPSZ_FLOAT|64);
}


static void handle_f96(char *s)
{
  handle_data(s,OPSZ_FLOAT|96);
}
#endif


static void do_alignment(taddr align,expr *offset,size_t pad,expr *fill)
{
  atom *a = new_space_atom(offset,pad,fill);

  a->align = align;
  add_atom(0,a);
}


static void handle_cnop(char *s)
{
  expr *offset;
  taddr align=1;

  offset = parse_expr_tmplab(&s);
  s = skip(s);
  if (*s == ',') {
    s = skip(s+1);
    align = parse_constexpr(&s);
  }
  else
    syntax_error(9);  /* , expected */

  /* align with cnop_pad in a code section, otherwise with zero */
  /* @@@ number of padding bytes should be variable for different archs. ? */
  if (!devpac_compat && align>3 &&
      (current_section==NULL || strchr(current_section->attr,'c')!=NULL))
    do_alignment(align,offset,2,number_expr(cnop_pad));
  else
    do_alignment(align,offset,1,NULL);
}


static void handle_align(char *s)
{
  do_alignment(1<<parse_constexpr(&s),number_expr(0),1,NULL);
}


static void handle_even(char *s)
{
  do_alignment(2,number_expr(0),1,NULL);
}


static void handle_odd(char *s)
{
  do_alignment(2,number_expr(1),1,NULL);
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
}


static void handle_xspc8(char *s)
{
  handle_dbss(s,8);
}


static void handle_xspc16(char *s)
{
  handle_dbss(s,16);
}


static void handle_xspc32(char *s)
{
  handle_dbss(s,32);
}


static void handle_xspc64(char *s)
{
  handle_dbss(s,64);
}


static void handle_xspc96(char *s)
{
  handle_dbss(s,96);
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


static void handle_spc64(char *s)
{
  handle_space(s,64);
}


static void handle_spc96(char *s)
{
  handle_space(s,96);
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


static void handle_blk64(char *s)
{
  handle_block(s,64);
}


static void handle_blk96(char *s)
{
  handle_block(s,96);
}


#ifdef VASM_CPU_M68K
static void handle_reldata(char *s,int size)
{
  for (;;) {
    char *opstart = s;
    operand *op;

    op = new_operand();
    s = skip_operand(s);
    if (parse_operand(opstart,s-opstart,op,DATA_OPERAND(size))) {
      if (op->value[0]) {
        expr *tmplab,*new;
        atom *a;

        tmplab = new_expr();
        tmplab->type = SYM;
        tmplab->c.sym = new_tmplabel(0);
        add_atom(0,new_label_atom(tmplab->c.sym));
        /* subtract the current pc value from all data expressions */
        new = make_expr(SUB,op->value[0],tmplab);
        simplify_expr(new);
        op->value[0] = new;
        a = new_datadef_atom(OPSZ_BITS(size),op);
        if (!align_data)
          a->align = 1;
        add_atom(0,a);
      }
      else
        ierror(0);
    }
    else
      syntax_error(8);  /* invalid data operand */
    s = skip(s);
    if (*s == ',')
      s = skip(s+1);
    else
      break;
  }
}


static void handle_reldata8(char *s)
{
  handle_reldata(s,8);
}


static void handle_reldata16(char *s)
{
  handle_reldata(s,16);
}


static void handle_reldata32(char *s)
{
  handle_reldata(s,32);
}
#endif


static void handle_end(char *s)
{
  parse_end = 1;
}


static void handle_fail(char *s)   
{ 
  add_atom(0,new_assert_atom(NULL,NULL,mystrdup(s)));
}


static void handle_idnt(char *s)
{
  char *name;

  if (name = parse_name(&s))
    setfilename(name);
}


static void handle_list(char *s)
{
  set_listing(1);
}


static void handle_nolist(char *s)
{
  set_listing(0);
}


static void handle_plen(char *s)
{
  int plen = (int)parse_constexpr(&s);

  listlinesperpage = plen > 12 ? plen : 12;
}


static void handle_page(char *s)
{
  /* @@@ we should also start a new page here! */
  listformfeed = 1;
}


static void handle_nopage(char *s)
{
  listformfeed = 0;
}


static void handle_output(char *s)
{
  char *name;

  if (name = parse_name(&s)) {
    if (*name=='.') {
      char *p;
      int outlen;

      if (!outname)
        outname = inname;
      if (p = strrchr(outname,'.'))
        outlen = p - outname;
      else
        outlen = strlen(outname);
      p = mymalloc(outlen+strlen(name)+1);
      memcpy(p,outname,outlen);
      strcpy(p+outlen,name);
      myfree(name);
      outname = p;
    }
    else if (!outname)
      outname = name;
  }
}


static void handle_dsource(char *s)
{
  char *name;

  if (name = parse_name(&s))
    setdebugname(name);
}


static void handle_debug(char *s)
{
  atom *a = new_srcline_atom((int)parse_constexpr(&s));

  add_atom(0,a);
}


static void handle_msource(char *s)
{
  if (!strnicmp(s,"on",2))
    msource_disable = 0;
  else if (!strnicmp(s,"off",3))
    msource_disable = 1;
  else
    msource_disable = atoi(s) == 0;
}


static void handle_vdebug(char *s)
{
  atom *a = new_atom(VASMDEBUG,0);

  add_atom(0,a);
}


static void handle_incdir(char *s)
{
  char *name;

  while (name = parse_name(&s)) {
    new_include_path(name);
    if (*s != ',') {
      return;
    }
    s = skip(s+1);
  }
  syntax_error(5);
}


static void handle_include(char *s)
{
  char *name;

  if (name = parse_name(&s)) {
    include_source(name);
  }
}


static void handle_incbin(char *s)
{
  char *name;
  taddr offs = 0;
  taddr length = 0;

  if (name = parse_name(&s)) {
    s = skip(s);
    if (*s == ',') {
      if (!devpac_compat && !phxass_compat) {
        /* We have an offset */
        s = skip(s + 1);
        offs = parse_constexpr(&s);
        s = skip(s);
        if (*s == ',') {
          /* We have a length */
          s = skip(s + 1);
          length = parse_constexpr(&s);
        }
      }
      else
        syntax_error(7);
    }
    include_binary_file(name,offs,length);
  }
}


static void handle_rept(char *s)
{
  new_repeat((utaddr)parse_constexpr(&s),NULL,NULL,rept_dirlist,endr_dirlist);
}


static void handle_endr(char *s)
{
  syntax_error(12,"endr","rept");  /* unexpected endr without rept */
}


static void handle_macro(char *s)
{
  char *name;

  if (name = parse_identifier(&s))
    new_macro(name,macro_dirlist,endm_dirlist,NULL);
  else
    syntax_error(10);  /* identifier expected */
}


static void handle_endm(char *s)
{
  syntax_error(12,"endm","macro");  /* unexpected endm without macro */
}


static void handle_mexit(char *s)
{
  leave_macro();
}


#if STRUCT
static void handle_struct(char *s)
{
  char *name;

  if (name = parse_identifier(&s)) {
    s = skip(s);
    if (new_structure(name))
      current_section->flags |= LABELS_ARE_LOCAL;
    myfree(name);
  }
  else
    syntax_error(10);  /* identifier expected */
}


static void handle_endstruct(char *s)
{
  section *prevsec;
  symbol *szlabel;

  if (end_structure(&prevsec)) {
    /* create the structure name as label defining the structure size */
    current_section->flags &= ~LABELS_ARE_LOCAL;
    szlabel = new_labsym(0,current_section->name);
    add_atom(0,new_label_atom(szlabel));
    /* end structure declaration by switching to previous section */
    set_section(prevsec);
  }
}
#endif


static void handle_rem(char *s)
{
  new_repeat(0,NULL,NULL,NULL,erem_dirlist);
}


static void handle_erem(char *s)
{
  syntax_error(12,"erem","rem");  /* unexpected erem without rem */
}


static void handle_ifb(char *s)
{
  cond_if(ISEOL(s));
}

static void handle_ifnb(char *s)
{
  cond_if(!ISEOL(s));
}

static void ifc(char *s,int b)
{
  char *str1,*str2;
  int result;

  str1 = parse_name(&s);
  if (str1!=NULL && *s==',') {
    s = skip(s+1);
    if (str2 = parse_name(&s)) {
      result = strcmp(str1,str2) == 0;
      cond_if(result == b);
      return;
    }
  }
  syntax_error(5);  /* missing operand */
}

static void handle_ifc(char *s)
{
  ifc(s,1);
}

static void handle_ifnc(char *s)
{
  ifc(s,0);
}

static void ifdef(char *s,int b)
{
  char *name;
  symbol *sym;
  int result;

  if (!(name = parse_symbol(&s))) {
    syntax_error(10);  /* identifier expected */
    return;
  }
  if (sym = find_symbol(name))
    result = sym->type != IMPORT;
  else
    result = 0;
  myfree(name);
  cond_if(result == b);
}

static void handle_ifd(char *s)
{
  ifdef(s,1);
}

static void handle_ifnd(char *s)
{
  ifdef(s,0);
}

static void ifmacro(char *s,int b)
{
  char *name = s;
  int result;

  if (s = skip_identifier(s)) {
    result = find_macro(name,s-name) != NULL;
    cond_if(result == b);
  }
  else
    syntax_error(10);  /*identifier expected */
}

static void handle_ifmacrod(char *s)
{
  ifmacro(s,1);
}

static void handle_ifmacrond(char *s)
{
  ifmacro(s,0);
}

static int eval_ifexp_advance(char **s,int c)
{
  expr *condexp = parse_expr_tmplab(s);
  taddr val;
  int b;

  if (eval_expr(condexp,&val,NULL,0)) {
    switch (c) {
      case 0: b = val == 0; break;
      case 1: b = val != 0; break;
      case 2: b = val > 0; break;
      case 3: b = val >= 0; break;
      case 4: b = val < 0; break;
      case 5: b = val <= 0; break;
      default: ierror(0); break;
    }
  }
  else {
    general_error(30);  /* expression must be constant */
    b = 0;
  }
  free_expr(condexp);
  return b;
}

static int eval_ifexp(char *s,int c)
{
  return eval_ifexp_advance(&s,c);
}

static void ifexp(char *s,int c)
{
  cond_if(eval_ifexp(s,c));
}

/* Move line_ptr to the end of the string if the parsing should stop,
   otherwise move line_ptr after the iif directive and the expression
   so the parsing can continue and return the new line_ptr.
   The string is never modified. */
static char *handle_iif(char *line_ptr)
{
  if (strnicmp(line_ptr,"iif",3) == 0 &&
      isspace((unsigned char)line_ptr[3])) {
    char *expr_copy,*expr_end;
    int condition;
    size_t expr_len;

    line_ptr += 3;

    /* Move the line ptr to the beginning of the iif expression. */
    line_ptr = skip(line_ptr);

    /* As eval_ifexp_advance() may modify the input string, duplicate
       it for the case when the parsing should continue. */
    expr_copy = mystrdup(line_ptr);
    expr_end = expr_copy;
    condition = eval_ifexp_advance(&expr_end,1);
    expr_len = expr_end - expr_copy;
    myfree(expr_copy);

    if (condition) {
      /* Parsing should continue after the expression, from the next field. */
      line_ptr += expr_len;
      line_ptr = skip(line_ptr);
    } else {
      /* Parsing should stop, move ptr to the end of the line. */
      line_ptr += strlen(line_ptr);
    }
  }
  return line_ptr;
}

static void handle_ifeq(char *s)
{
  ifexp(s,0);
}

static void handle_ifne(char *s)
{
  ifexp(s,1);
}

static void handle_ifgt(char *s)
{
  ifexp(s,2);
}

static void handle_ifge(char *s)
{
  ifexp(s,3);
}

static void handle_iflt(char *s)
{
  ifexp(s,4);
}

static void handle_ifle(char *s)
{
  ifexp(s,5);
}

static void handle_ifp1(char *s)
{
  cond_if(1);        /* vasm parses only once, so we assume true */
  syntax_error(25);  /* and warn about it */
}

static void handle_ifp2(char *s)
{
  cond_if(0);        /* vasm parses only once, so we assume false */
  syntax_error(26);  /* and warn about it */
}

static void handle_else(char *s)
{
  cond_skipelse();
}

static void handle_endif(char *s)
{
  cond_endif();
}


static void handle_rsreset(char *s)
{
  new_abs(rs_name,number_expr(0));
}

static void handle_rsset(char *s)
{
  new_abs(rs_name,number_expr(parse_constexpr(&s)));
}

static void handle_clrfo(char *s)
{
  new_abs(fo_name,number_expr(0));
}

static void handle_setfo(char *s)
{
  new_abs(fo_name,number_expr(parse_constexpr(&s)));
}

static void handle_rs8(char *s)
{
  new_setoffset_size(NULL,rs_name,&s,1,1);
}

static void handle_rs16(char *s)
{
  new_setoffset_size(NULL,rs_name,&s,1,2);
}

static void handle_rs32(char *s)
{
  new_setoffset_size(NULL,rs_name,&s,1,4);
}

static void handle_rs64(char *s)
{
  new_setoffset_size(NULL,rs_name,&s,1,8);
}

static void handle_rs96(char *s)
{
  new_setoffset_size(NULL,rs_name,&s,1,12);
}

static void handle_fo8(char *s)
{
  new_setoffset_size(NULL,fo_name,&s,-1,1);
}

static void handle_fo16(char *s)
{
  new_setoffset_size(NULL,fo_name,&s,-1,2);
}

static void handle_fo32(char *s)
{
  new_setoffset_size(NULL,fo_name,&s,-1,4);
}

static void handle_fo64(char *s)
{
  new_setoffset_size(NULL,fo_name,&s,-1,8);
}

static void handle_fo96(char *s)
{
  new_setoffset_size(NULL,fo_name,&s,-1,12);
}

static void handle_cargs(char *s)
{
  char *name;
  expr *offs;
  taddr size;

  if (*s == '#') {
    /* offset given */
    ++s;
    offs = parse_expr_tmplab(&s);
    s = skip(s);
    if (*s != ',')
      syntax_error(9);  /* , expected */
    else
      s = skip(s+1);
  }
  else
    offs = number_expr(bytespertaddr);  /* default offset */

  for (;;) {

    if (!(name = parse_symbol(&s))) {
      syntax_error(10);  /* identifier expected */
      break;
    }

    if (!check_symbol(name)) {
      /* define new stack offset symbol */
      new_abs(name,copy_tree(offs));
    }
    myfree(name);

    /* increment offset by given size */
    if (*s == '.') {
      ++s;
      switch (tolower((unsigned char)*s)) {
        case 'b':
        case 'w':
          size = 2;
          ++s;
          break;
        case 'l':
          size = 4;
          ++s;
          break;
        default:
          size = 2;
          syntax_error(1);  /* invalid extension */
          break;
      }
    }
    else
      size = 2;

    s = skip(s);
    if (*s != ',')  /* define another offset symbol? */
      break;

    offs = make_expr(ADD,offs,number_expr(size));
    simplify_expr(offs);
    s = skip(s+1);
  }

  /* offset expression was copied, so we can free it now */
  if (offs)
    free_expr(offs);
}

static void handle_printt(char *s)
{
  char *txt;

  while (txt = parse_name(&s)) {
    add_atom(0,new_text_atom(txt));
    s = skip(s);
    if (*s != ',')
      break;
    add_atom(0,new_text_atom(NULL));  /* new line */
    s = skip(s+1);
  }
  add_atom(0,new_text_atom(NULL));  /* new line */
}

static void handle_printv(char *s)
{
  expr *x;

  for (;;) {
    x = parse_expr(&s);
    add_atom(0,new_text_atom("$"));
    add_atom(0,new_expr_atom(x,PEXP_HEX,32));
    add_atom(0,new_text_atom(" "));
    add_atom(0,new_expr_atom(x,PEXP_SDEC,32));
    add_atom(0,new_text_atom(" \""));
    add_atom(0,new_expr_atom(x,PEXP_ASC,32));
    add_atom(0,new_text_atom("\" %"));
    add_atom(0,new_expr_atom(x,PEXP_BIN,32));
    add_atom(0,new_text_atom(NULL));  /* new line */
    s = skip(s);
    if (*s != ',')
      break;
    s = skip(s+1);
  }    
}

static void handle_showoffset(char *s)
{
  char *txt;

  if (txt = parse_name(&s))
    add_atom(0,new_text_atom(txt));
  add_atom(0,new_text_atom(" "));
  add_atom(0,new_expr_atom(curpc_expr(),PEXP_HEX,32));
  add_atom(0,new_text_atom(NULL));  /* new line */
}

static void handle_dummy_expr(char *s)
{
  parse_expr(&s);
  syntax_error(11);  /* directive has no effect */
}

static void handle_dummy_cexpr(char *s)
{
  parse_constexpr(&s);
  syntax_error(11);  /* directive has no effect */
}

static void handle_noop(char *s)
{
  syntax_error(11);  /* directive has no effect */
}

static void handle_comment(char *s)
{
  /* handle Atari-specific "COMMENT HEAD=<expr>" to define the tos-flags */
  if (!strnicmp(s,"HEAD=",5)) {
    s += 5;
    new_abs(" TOSFLAGS",parse_expr_tmplab(&s));
  }
  /* otherwise it's just a comment to be ignored */
}

static void handle_inline(char *s)
{
  static int id;
  char *last;

  if (inline_stack_index < INLSTACKSIZE) {
    sprintf(inl_lab_name,INLLABFMT,id);
    last = set_last_global_label(inl_lab_name);
    if (inline_stack_index == 0)
      saved_last_global_label = last;
    inline_stack[inline_stack_index++] = id++;
  }
  else
    syntax_error(22,INLSTACKSIZE);  /* maximum inline nesting depth exceeded */
}

static void handle_einline(char *s)
{
  if (inline_stack_index > 0 ) {
    if (--inline_stack_index == 0) {
      set_last_global_label(saved_last_global_label);
      saved_last_global_label = NULL;
    }
    else {
      sprintf(inl_lab_name,INLLABFMT,inline_stack[inline_stack_index-1]);
      set_last_global_label(inl_lab_name);
    }
  }
  else
    syntax_error(20);  /* einline without inline */
}

static void handle_pushsect(char *s)
{
  push_section();
  eol(s);
}

static void handle_popsect(char *s)
{
  pop_section();
  eol(s);
}


#define D 1 /* available for DevPac */
#define P 2 /* available for PhxAss */
struct {
  char *name;
  int avail;
  void (*func)(char *);
} directives[] = {
  "org",P|D,handle_org,
  "rorg",P|D,handle_rorg,
  "section",P|D,handle_section,
  "offset",P|D,handle_offset,
  "code",P|D,handle_csec,
  "cseg",P,handle_csec,
  "text",P|D,handle_csec,
  "data",P|D,handle_dsec,
  "dseg",P,handle_dsec,
  "bss",P|D,handle_bss,
  "code_c",P|D,handle_codec,
  "code_f",P|D,handle_codef,
  "data_c",P|D,handle_datac,
  "data_f",P|D,handle_dataf,
  "bss_c",P|D,handle_bssc,
  "bss_f",P|D,handle_bssf,
  "public",P,handle_global,
  "xdef",P|D,handle_global,
  "xref",P|D,handle_global,
  "xref.l",P|D,handle_global,
  "nref",P,handle_nref,
  "entry",0,handle_global,
  "extrn",0,handle_global,
  "global",0,handle_global,
  "import",0,handle_global, /* modifictation: pink-rg: handle purec syntax */
  "export",0,handle_global, /* modifictation: pink-rg: handle purec syntax */
  "weak",0,handle_weak,
  "comm",0,handle_comm,
#ifndef VASM_CPU_JAGRISC    /* conflicts with Jaguar load instruction */
  "load",P,handle_dummy_expr,
#endif
  "jumperr",0,handle_dummy_expr,
  "jumpptr",0,handle_dummy_expr,
  "mask2",0,eol,
  "cnop",P|D,handle_cnop,
  "align",0,handle_align,
  "even",P|D,handle_even,
  "odd",0,handle_odd,
  "dc",P|D,handle_d16,
  "db",0,handle_d8,
  "dw",0,handle_d16,
  "dl",0,handle_d32,
  "dc.b",P|D,handle_d8,
  "dc.w",P|D,handle_d16,
  "dc.l",P|D,handle_d32,
  "dc.q",P,handle_d64,
#if FLOAT_PARSER
  "dc.s",P|D,handle_f32,
  "dc.d",P|D,handle_f64,
  "dc.x",P|D,handle_f96,
#endif
  "ds",P|D,handle_spc16,
  "ds.b",P|D,handle_spc8,
  "ds.w",P|D,handle_spc16,
  "ds.l",P|D,handle_spc32,
  "ds.q",P,handle_spc64,
  "ds.s",P|D,handle_spc32,
  "ds.d",P|D,handle_spc64,
  "ds.x",P|D,handle_spc96,
  "dx",P,handle_xspc16,
  "dx.b",P,handle_xspc8,
  "dx.w",P,handle_xspc16,
  "dx.l",P,handle_xspc32,
  "dx.q",P,handle_xspc64,
  "dx.s",P,handle_xspc32,
  "dx.d",P,handle_xspc64,
  "dx.x",P,handle_xspc96,
  "dcb",P|D,handle_blk16,
  "dcb.b",P|D,handle_blk8,
  "dcb.w",P|D,handle_blk16,
  "dcb.l",P|D,handle_blk32,
  "dcb.q",P,handle_blk64,
  "dcb.s",P|D,handle_blk32,
  "dcb.d",P|D,handle_blk64,
  "dcb.x",P|D,handle_blk96,
  "blk",P,handle_blk16,
  "blk.b",P,handle_blk8,
  "blk.w",P,handle_blk16,
  "blk.l",P,handle_blk32,
  "blk.q",P,handle_blk64,
  "blk.s",P,handle_blk32,
  "blk.d",P,handle_blk64,
  "blk.x",P,handle_blk96,
#ifdef VASM_CPU_M68K
  "dr",0,handle_reldata16,
  "dr.b",0,handle_reldata8,
  "dr.w",0,handle_reldata16,
  "dr.l",0,handle_reldata32,
#endif
  "end",P|D,handle_end,
  "fail",P|D,handle_fail,
  "idnt",P|D,handle_idnt,
  "ttl",P|D,handle_idnt,
  "list",P|D,handle_list,
  "module",P|D,handle_idnt,
  "nolist",P|D,handle_nolist,
  "plen",P|D,handle_plen,
  "llen",P|D,handle_dummy_cexpr,
  "page",P|D,handle_page,
  "nopage",P|D,handle_nopage,
  "spc",P|D,handle_dummy_cexpr,
  "output",P|D,handle_output,
  "symdebug",P,eol,
  "dsource",P,handle_dsource,
  "debug",P,handle_debug,
  "msource",0,handle_msource,
  "vdebug",0,handle_vdebug,
  "comment",P|D,handle_comment,
  "incdir",P|D,handle_incdir,
  "include",P|D,handle_include,
  "incbin",P|D,handle_incbin,
  "image",0,handle_incbin,
  "rept",P|D,handle_rept,
  "endr",P|D,handle_endr,
  "macro",P|D,handle_macro,
  "endm",P|D,handle_endm,
  "mexit",P|D,handle_mexit,
  "rem",P,handle_rem,
  "erem",P,handle_erem,
  "ifb",0,handle_ifb,
  "ifnb",0,handle_ifnb,
  "ifc",P|D,handle_ifc,
  "ifnc",P|D,handle_ifnc,
  "ifd",P|D,handle_ifd,
  "ifnd",P|D,handle_ifnd,
  "ifmacrod",0,handle_ifmacrod,
  "ifmacrond",0,handle_ifmacrond,
  "ifeq",P|D,handle_ifeq,
  "ifne",P|D,handle_ifne,
  "ifgt",P|D,handle_ifgt,
  "ifge",P|D,handle_ifge,
  "iflt",P|D,handle_iflt,
  "ifle",P|D,handle_ifle,
  "ifmi",0,handle_iflt,
  "ifpl",0,handle_ifge,
  "if",P,handle_ifne,
  "ifp1",0,handle_ifp1,
  "if1",0,handle_ifp1,
  "if2",0,handle_ifp2,
  "else",P|D,handle_else,
  "elseif",P|D,handle_else,
  "endif",P|D,handle_endif,
  "endc",P|D,handle_endif,
  "rsreset",P|D,handle_rsreset,
  "rsset",P|D,handle_rsset,
  "clrso",P,handle_rsreset,
  "setso",P,handle_rsset,
  "clrfo",P,handle_clrfo,
  "setfo",P,handle_setfo,
  "rs",P|D,handle_rs16,
  "rs.b",P|D,handle_rs8,
  "rs.w",P|D,handle_rs16,
  "rs.l",P|D,handle_rs32,
  "rs.q",P,handle_rs64,
  "rs.s",P|D,handle_rs32,
  "rs.d",P|D,handle_rs64,
  "rs.x",P|D,handle_rs96,
  "so",P,handle_rs16,
  "so.b",P,handle_rs8,
  "so.w",P,handle_rs16,
  "so.l",P,handle_rs32,
  "so.q",P,handle_rs64,
  "so.s",P,handle_rs32,
  "so.d",P,handle_rs64,
  "so.x",P,handle_rs96,
  "fo",P,handle_fo16,
  "fo.b",P,handle_fo8,
  "fo.w",P,handle_fo16,
  "fo.l",P,handle_fo32,
  "fo.q",P,handle_fo64,
  "fo.s",P,handle_fo32,
  "fo.d",P,handle_fo64,
  "fo.x",P,handle_fo96,
  "cargs",P|D,handle_cargs,
  "echo",P,handle_printt,
  "printt",0,handle_printt,
  "printv",0,handle_printv,
  "showoffset",P,handle_showoffset,
  "auto",0,handle_noop,
  "inline",P,handle_inline,
  "einline",P,handle_einline,
#if STRUCT
  "struct",0,handle_struct,
  "estruct",0,handle_endstruct,
#endif
  "pushsection",0,handle_pushsect,
  "popsection",0,handle_popsect,
};
#undef P
#undef D

size_t dir_cnt = sizeof(directives) / sizeof(directives[0]);


/* checks for a valid directive, and return index when found, -1 otherwise */
static int check_directive(char **line)
{
  char *s,*name;
  hashdata data;

  s = skip(*line);
  if (!ISIDSTART(*s))
    return -1;
  name = s++;
  while (ISIDCHAR(*s) || *s=='.')
    s++;
  if (!find_namelen_nc(dirhash,name,s-name,&data))
    return -1;
  *line = s;
  return data.idx;
}

/* Handles assembly directives;
   returns non-zero if the parsing of the line should stop. */
static int handle_directive(char *line)
{
  int idx = check_directive(&line);

  if (idx >= 0) {
    directives[idx].func(skip(line));
    return 1;
  }
  return 0;
}


static int offs_directive(char *s,char *name)
{
  int len = strlen(name);
  char *d = s + len;

  return !strnicmp(s,name,len) &&
         ((isspace((unsigned char)*d) || ISEOL(d)) ||
          (*d=='.' && (isspace((unsigned char)*(d+2))||ISEOL(d+2))));
}


static symbol *fequate(char *labname,char **s)
{
  char x = tolower((unsigned char)**s);

  if (x=='s' || x=='d' || x=='x' || x=='p') {
    *s = skip(*s + 1);
    return new_equate(labname,parse_expr_float(s));
  }
  syntax_error(1);  /* illegal extension */
  return NULL;
}


static char *skip_local(char *p)
{
  char *s;

  if (ISIDSTART(*p) || isdigit((unsigned char)*p)) {  /* may start with digit */
    s = p++;
    while (ISIDCHAR(*p))
      p++;
    p = CHKIDEND(s,p);
  }
  else
    p = NULL;

  return p;
}


#if STRUCT
/* When a structure with this name exists, insert its atoms and either
   initialize with new values or accept its default values. */
static int execute_struct(char *name,int name_len,char *s)
{
  section *str;
  atom *p;

  str = find_structure(name,name_len);
  if (str == NULL)
    return 0;

  for (p=str->first; p; p=p->next) {
    atom *new;
    char *opp;
    int opl;

    if (p->type==DATA || p->type==SPACE || p->type==DATADEF) {
      opp = s = skip(s);
      s = skip_operand(s);
      opl = s - opp;

      if (opl > 0) {
        /* initialize this atom with a new expression */

        if (p->type == DATADEF) {
          /* parse a new data operand of the declared bitsize */
          operand *op;

          op = new_operand();
          if (parse_operand(opp,opl,op,
                            DATA_OPERAND(p->content.defb->bitsize))) {
            new = new_datadef_atom(p->content.defb->bitsize,op);
            new->align = p->align;
            add_atom(0,new);
          }
          else
            syntax_error(8);  /* invalid data operand */
        }
        else if (p->type == SPACE) {
          /* parse the fill expression for this space */
          new = clone_atom(p);
          new->content.sb = new_sblock(p->content.sb->space_exp,
                                       p->content.sb->size,
                                       parse_expr_tmplab(&opp));
          new->content.sb->space = p->content.sb->space;
          add_atom(0,new);
        }
        else {
          /* parse constant data - probably a string, or a single constant */
          dblock *db;

          db = new_dblock();
          db->size = p->content.db->size;
          db->data = db->size ? mycalloc(db->size) : NULL;
          if (db->data) {
            if (*opp=='\"' || *opp=='\'') {
              dblock *strdb;

              strdb = parse_string(&opp,*opp,8);
              if (strdb->size) {
                if (strdb->size > db->size)
                  syntax_error(24,strdb->size-db->size);  /* cut last chars */
                memcpy(db->data,strdb->data,
                       strdb->size > db->size ? db->size : strdb->size);
                myfree(strdb->data);
              }
              myfree(strdb);
            }
            else {
              taddr val = parse_constexpr(&opp);
              void *p;

              if (db->size > sizeof(taddr) && BIGENDIAN)
                p = db->data + db->size - sizeof(taddr);
              else
                p = db->data;
              setval(BIGENDIAN,p,sizeof(taddr),val);
            }
          }
          add_atom(0,new_data_atom(db,p->align));
        }
      }
      else {
        /* empty: use default values from original atom */
        add_atom(0,clone_atom(p));
      }

      s = skip(s);
      if (*s == ',')
        s++;
    }
    else if (p->type == INSTRUCTION)
      syntax_error(23);  /* skipping instruction in struct init */

    /* other atoms are silently ignored */
  }

  return 1;
}
#endif


void parse(void)
{
  char *s,*line,*inst,*labname;
  char *ext[MAX_QUALIFIERS?MAX_QUALIFIERS:1];
  char *op[MAX_OPERANDS];
  int ext_len[MAX_QUALIFIERS?MAX_QUALIFIERS:1];
  int op_len[MAX_OPERANDS];
  int ext_cnt,op_cnt,inst_len;
  instruction *ip;

  while (line = read_next_line()) {
    if (parse_end)
      continue;
    s = line;
    if (!phxass_compat && !devpac_compat)
      set_internal_abs(line_name,real_line());

    if (!cond_state()) {
      /* skip source until ELSE or ENDIF */
      int idx;

      /* skip label, when present */
      if (labname = parse_labeldef(&s,0)) {
        if (*s == ':')
          s++;  /* skip double-colon */
        myfree(labname);
      }
      /* advance to directive */
      s = skip(s);
      idx = check_directive(&s);
      if (idx >= 0) {
        if (!strncmp(directives[idx].name,"if",2))
          cond_skipif();
        else if (directives[idx].func == handle_else)
          cond_else();
        else if (directives[idx].func == handle_endif)
          cond_endif();
      }
      continue;
    }

    if (labname = parse_labeldef(&s,0)) {
      /* we have found a global or local label */
      uint32_t symflags = 0;
      symbol *label;

      if (*s == ':') {
        /* double colon automatically declares label as exported */
        symflags |= EXPORT;
        s++;
      }

      s = skip(s);

      s = handle_iif(s);

      if (!strnicmp(s,"equ",3) && isspace((unsigned char)*(s+3))) {
        s = skip(s+3);
        label = new_equate(labname,parse_expr_tmplab(&s));
      }
#if FLOAT_PARSER
      else if (!strnicmp(s,"fequ.",5) && isspace((unsigned char)*(s+6))) {
        s += 5;
        label = fequate(labname,&s);
      }
      else if (phxass_compat &&
               !strnicmp(s,"equ.",4) && isspace((unsigned char)*(s+5))) {
        s += 4;
        label = fequate(labname,&s);
      }
#endif
      else if (*s=='=') {
        ++s;
        if (phxass_compat && *s=='.' && isspace((unsigned char)*(s+2))) {
#if FLOAT_PARSER
          ++s;
          label = fequate(labname,&s);
#else
          syntax_error(7);  /* syntax error */
#endif
        }
        else {
          s = skip(s);
          label = new_equate(labname,parse_expr_tmplab(&s));
        }
      }
      else if (!strnicmp(s,"set",3) && isspace((unsigned char)*(s+3))) {
        /* SET allows redefinitions */
        s = skip(s+3);
        label = new_abs(labname,parse_expr_tmplab(&s));
      }
      else if (offs_directive(s,"rs") || offs_directive(s,"so")) {
        label = new_setoffset(labname,&s,rs_name,1);
      }
      else if (offs_directive(s,"fo")) {
        label = new_setoffset(labname,&s,fo_name,-1);
      }
      else if (!strnicmp(s,"ttl",3) && isspace((unsigned char)*(s+3))) {
        s = skip(s+3);
        setfilename(labname);
      }
      else if (!strnicmp(s,"macro",5) &&
               (isspace((unsigned char)*(s+5)) || *(s+5)=='\0'
                || *(s+5)==commentchar)) {
        /* reread original label field as macro name, no local macros */
        s = line;
        myfree(labname);
        if (!(labname = parse_identifier(&s)))
          ierror(0);
        new_macro(labname,macro_dirlist,endm_dirlist,NULL);
        myfree(labname);
        continue;
      }
#ifdef PARSE_CPU_LABEL
      else if (!PARSE_CPU_LABEL(labname,&s)) {
#else
      else {
#endif
        label = new_labsym(0,labname);
        label->flags |= symflags;
        add_atom(0,new_label_atom(label));
      }
      myfree(labname);
    }

    /* check for directives first */
    s = skip(s);
    if (*s=='\0' || *s=='*' || *s==commentchar)
      continue;

    s = handle_iif(s);

    s = parse_cpu_special(s);
    if (ISEOL(s))
      continue;

    if (handle_directive(s))
      continue;

    s = skip(s);
    if (ISEOL(s))
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
#if STRUCT
    if (execute_struct(inst,inst_len,s))
      continue;
#endif

    /* read operands, terminated by comma (unless in parentheses)  */
    op_cnt = 0;
    while (!ISEOL(s) && op_cnt<MAX_OPERANDS) {
      op[op_cnt] = s;
      s = skip_operand(s);
      op_len[op_cnt] = s - op[op_cnt];
      op_cnt++;

      if (allow_spaces) {
        s = skip(s);
        if (*s != ',')
          break;
        else
          s = skip(s+1);
      }
      else {
        if (*s != ',') {
          if (check_comm)
            comment_check(s);
          break;
        }
        s++;
      }
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
  arg->len = 0;  /* no argument reference by keyword */
  param->name = s;

  if (*s == '<') {
    /* macro argument enclosed in < ... > */
    param->name++;
    while (*++s != '\0') {
      if (*s == '>') {
        if (*(s+1) == '>') {
          /* convert ">>" into a single ">" by shifting the whole line buffer */
          char *p;

          for (p=s+1; *p!='\0'; p++)
            *(p-1) = *p;
          *(p-1) = '\0';
        }
        else {
          param->len = s - param->name;
          return s + 1;
        }
      }
    }
    param->len = s - param->name;
  }
  else {
    s = skip_operand(s);
    param->len = trim(s) - param->name;
  }

  return s;
}


/* src is the new macro source, cur_src is still the parent source */
void my_exec_macro(source *src)
{
  symbol *carg;

  /* reset the CARG symbol to 1, selecting the first macro parameter */
  carg = internal_abs(CARGSYM);
  cur_src->cargexp = carg->expr;  /* remember last CARG expression */
  carg->expr = carg1;
}


static int copy_macro_carg(source *src,int inc,char *d,int len)
/* copy macro parameter #CARG to line buffer, increment or decrement CARG */
{
  symbol *carg = internal_abs(CARGSYM);
  int nc;

  if (carg->type != EXPRESSION)
    return 0;
  simplify_expr(carg->expr);
  if (carg->expr->type != NUM) {
    general_error(30);  /* expression must be a constant */
    return 0;
  }
  nc = copy_macro_param(src,carg->expr->c.val-1,d,len);

  if (inc) {
    expr *new = make_expr(inc>0?ADD:SUB,copy_tree(carg->expr),number_expr(1));

    simplify_expr(new);
    carg->expr = new;
  }
  return nc;
}


/* expands arguments and special escape codes into macro context */
int expand_macro(source *src,char **line,char *d,int dlen)
{
  int nc = 0;
  char *s = *line;

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
          else
            nc = -1;
        }
        else
          nc = 1;
      }
      else
        nc = -1;
    }


    else if (*s == '@') {
      /* \@ : insert a unique id "_nnnnnn" */
      unsigned long unique_id;

      s++;
      if (*s == '@') {
        /* pull id from stack */
        if (id_stack_index <= 0) {
          syntax_error(17);  /* id pull without matching push */
          return 0;
        }
        else
          unique_id = id_stack[id_stack_index-1];
      }
      else
        unique_id = src->id;

      if (dlen > 7) {
        nc = sprintf(d,"_%06lu",unique_id);
        switch (*s) {
          case '!':
            /* push id onto stack */
            if (id_stack_index >= IDSTACKSIZE) {
              syntax_error(16);  /* id stack overflow */
              return 0;
            }
            else
              id_stack[id_stack_index++] = unique_id;
            s++;
            break;
          case '?':
            /* push id below the top item on the stack */
            if (id_stack_index >= IDSTACKSIZE) {
              syntax_error(16);  /* id stack overflow */
              return 0;
            }
            else if (id_stack_index <= 0) {
              syntax_error(14);  /* insert on empty id stack */
              return 0;
            }
            else {
              id_stack[id_stack_index] = id_stack[id_stack_index-1];
              id_stack[id_stack_index-1] = unique_id;
              ++id_stack_index;
            }
            s++;
            break;
          case '@':
            --id_stack_index;
            s++;
            break;
        }
      }
      else
        nc = -1;
    }

    else if (*s == '<') {
      /* \<symbol> : insert absolute unsigned symbol value */
      const char *fmt;
      char *name;
      symbol *sym;
      taddr val;

      if (*(++s) == '$') {
        fmt = "%lX";
        s++;
      }
      else
        fmt = "%lu";
      if (name = parse_symbol(&s)) {
        if ((sym = find_symbol(name)) && sym->type==EXPRESSION) {
          if (eval_expr(sym->expr,&val,NULL,0)) {
            if (dlen > 9)
              nc = sprintf(d,fmt,(unsigned long)(uint32_t)val);
            else
              nc = -1;
          }
        }
        myfree(name);
        if (*s++ != '>') {
          syntax_error(19);  /* invalid numeric expansion */
          return 0;
        }
      }
      else {
        syntax_error(10);  /* identifier expected */
        return 0;
      }
    }

    else if (*s == '#') {
      /* \# : insert number of parameters */
      if (dlen > 3) {
        nc = sprintf(d,"%d",src->num_params);
        s++;
      }
      else
        nc = -1;
    }

    else if (*s=='?' && isdigit((unsigned char)*(s+1))) {
      /* \?n : insert parameter n length */
      if (dlen > 3) {
        nc = sprintf(d,"%d",*(s+1)=='0'?
#if MAX_QUALIFIERS > 0
                            src->qual_len[0]:
#else
                            0:
#endif
                            src->param_len[*(s+1)-'1']);
        s += 2;
      }
      else
        nc = -1;
    }

    else if (*s == '.') {
      /* \. : insert parameter #CARG */
      nc = copy_macro_carg(src,0,d,dlen);
      s++;
    }
    else if (*s == '+') {
      /* \+ : insert parameter #CARG and increment CARG */
      nc = copy_macro_carg(src,1,d,dlen);
      s++;
    }
    else if (*s == '-') {
      /* \- : insert parameter #CARG and decrement CARG */
      nc = copy_macro_carg(src,-1,d,dlen);
      s++;
    }

    else if (isdigit((unsigned char)*s)) {
      /* \0..\9 : insert macro parameter 0..9 */
      if (*s == '0')
        nc = copy_macro_qual(src,0,d,dlen);
      else
        nc = copy_macro_param(src,*s-'1',d,dlen);
      s++;
    }

    else if (maxmacparams>9 &&
             tolower((unsigned char)*s)>='a' &&
             tolower((unsigned char)*s)<('a'+maxmacparams-9)) {
        /* \a..\z : insert macro parameter 10..35 */
        nc = copy_macro_param(src,tolower((unsigned char)*s)-'a'+9,d,dlen);
        s++;
    }

    if (nc >= dlen)
      nc = -1;
    else if (nc >= 0)
      *line = s;  /* update line pointer when expansion took place */
  }

  return nc;  /* number of chars written to line buffer, -1: out of space */
}


char *const_prefix(char *s,int *base)
{
  if (isdigit((unsigned char)*s)) {
    *base = 10;
    return s;
  }
  if (*s == '$') {
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
/* Motorola local labels start with a '.' or end with '$': "1234$", ".1" */
{
  char *s,*p,*name;

  name = NULL;
  s = *start;
  p = skip_local(s);

  if (p!=NULL && *p=='\\' && ISIDSTART(*s) && *s!=local_char && *(p-1)!='$') {
    /* skip local part of global\local label */
    s = p + 1;
    if (p = skip_local(s)) {
      name = make_local_label(*start,(s-1)-*start,s,*(p-1)=='$'?(p-1)-s:p-s);
      *start = skip(p);
    }
    else
      return NULL;
  }
  else if (p!=NULL && p>(s+1)) {  /* identifier with at least 2 characters */
    if (*s == local_char) {
      /* .label */
      name = make_local_label(NULL,0,s,p-s);
      *start = skip(p);
    }
    else if (*(p-1) == '$') {
      /* label$ */
      name = make_local_label(NULL,0,s,(p-1)-s);
      *start = skip(p);
    }
  }

  return name;
}


int init_syntax(void)
{
  size_t i;
  symbol *sym;
  hashdata data;
  int avail;

  if (devpac_compat) avail = 1;
  else if (phxass_compat) avail = 2;
  else avail = 0;

  dirhash = new_hashtable(0x200); /* @@@ */
  for (i=0; i<dir_cnt; i++) {
    if ((directives[i].avail & avail) == avail) {
      data.idx = i;
      add_hashentry(dirhash,directives[i].name,data);
    }
  }
  
  cond_init();
  current_pc_char = '*';
  carg1 = number_expr(1);        /* CARG start value for macro invocations */
  set_internal_abs(REPTNSYM,-1); /* reserve the REPTN symbol */
  sym = internal_abs(rs_name);
  refer_symbol(sym,so_name);     /* SO is only an additional reference to RS */
  internal_abs(fo_name);
  maxmacparams = allmp ? 35 : 9; /* 35: allow \a..\z macro parameters */

  if (!phxass_compat && !devpac_compat)
    set_internal_abs(line_name,0);

  if (phxass_compat) {
    if (!outname) {
      /* set a default output name in PhxAss mode */
      char *p;
      int len;

      if (p = strrchr(inname,'.')) {
        if (exec_out || tolower((unsigned char)*(p+1)) != 'o') {
          len = p - inname;
          outname = mymalloc(len+(exec_out?1:3));
          memcpy(outname,inname,len);
          if (exec_out)
            outname[len] = '\0';
          else
            strcpy(outname+len,".o");
        }
      }
    }
  }

  return 1;
}


int syntax_args(char *p)
{
  if (!strcmp(p,"-align")) {
    align_data = 1;
    return 1;
  }
  else if (!strcmp(p,"-allmp")) {
    allmp = 1;
    return 1;
  }
  else if (!strcmp(p,"-devpac")) {
    devpac_compat = 1;
    align_data = 1;
    esc_sequences = 0;
    dot_idchar = 1;
    allmp = 1;
    warn_unalloc_ini_dat = 1;
    return 1;
  }
  else if (!strcmp(p,"-phxass")) {
    set_internal_abs("_PHXASS_",2);
    phxass_compat = 1;
    esc_sequences = 1;
    nocase_macros = 1;
    allow_spaces = 1;
    dot_idchar = 1;
    allmp = 1;
    return 1;
  }
  else if (!strcmp(p,"-spaces")) {
    allow_spaces = 1;
    return 1;
  }
  else if (!strcmp(p,"-ldots")) {
    dot_idchar = 1;
    return 1;
  }
  else if (!strcmp(p,"-localu")) {
    local_char = '_';
    return 1;
  }
  else if (!strcmp(p,"-warncomm")) {
    check_comm = 1;
    return 1;
  }
  else if (!strncmp(p,"-cnop=",6)) {
    int pad_code;

    sscanf(p+6,"%i",&pad_code);
    cnop_pad = (uint16_t)pad_code;
    return 1;
  }
  return 0;
}
