/* parse.c - global parser support functions */
/* (c) in 2009-2021 by Volker Barthelmann and Frank Wille */

#include "vasm.h"

int esc_sequences = 0;  /* do not handle escape sequences by default */
int nocase_macros = 0;  /* macro names are case-insensitive */
int maxmacparams = MAXMACPARAMS;
int maxmacrecurs = MAXMACRECURS;
int msource_disable;    /* true: disable source level debugging within macro */

#ifndef MACROHTABSIZE
#define MACROHTABSIZE 0x800
#endif
static hashtable *macrohash;

#ifndef STRUCTHTABSIZE
#define STRUCTHTABSIZE 0x800
#endif
static hashtable *structhash;

static macro *first_macro;
static macro *cur_macro;
static struct namelen *enddir_list;
static size_t enddir_minlen;
static struct namelen *macrdir_list;
static struct namelen *reptdir_list;

static int rept_cnt = -1;
static source *rept_defsrc;
static int rept_defline;
static char *rept_start,*rept_name,*rept_vals;

static section *cur_struct;
static section *struct_prevsect;


char *escape(char *s,char *code)
{
  char dummy;
  int cnt;

  if (*s++ != '\\')
    ierror(0);
  if (code == NULL)
    code = &dummy;

  if (!esc_sequences) {
    *code='\\';
    return s;
  }

  switch (*s) {
    case 'b':
      *code='\b';
      return s+1;
    case 'f':
      *code='\f';
      return s+1;
    case 'n':
      *code='\n';
      return s+1;
    case 'r':
      *code='\r';
      return s+1;
    case 't':
      *code='\t';
      return s+1;
    case '\\':
      *code='\\';
      return s+1;
    case '\"':
      *code='\"';
      return s+1;
    case '\'':
      *code='\'';
      return s+1;
    case 'e':
      *code=27;
      return s+1;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      *code=0;
      cnt=0;
      while (*s>='0' && *s<='9' && ++cnt<=3) {
        *code = *code*8 + *s-'0';
        s++;
      }
      return s;
    case 'x': case 'X':
      *code=0;
      s++;
      while ((*s>='0' && *s<='9') ||
             (*s>='a' && *s<='f') || (*s>='A' && *s<='F')) {
        if (*s>='0' && *s<='9')
          *code = *code*16 + *s-'0';
        else if (*s>='a' && *s<='f')
          *code = *code*16 + *s-'a' + 10;
        else
          *code = *code*16 + *s -'A' + 10;
        s++;
      }    
      return s;
    default:
      general_error(35,*s);
      return s;
  }
}


char *cut_trail_blanks(char *s)
{
  while (isspace((unsigned char)*(s-1)))  /* cut trailing blanks */
    s--;
  return s;
}


char *parse_name(char **start)
/* parses a quoted or unquoted name-string and returns a pointer to it */
{
  char *s = *start;
  char c,*name;

  if (*s=='\"' || *s=='\'') {
    c = *s++;
    name = s;
    while (*s && *s!=c)
      s++;
    name = cnvstr(name,s-name);
    if (*s)
      s = skip(s+1);
  }
#ifdef VASM_CPU_M68K
  else if (*s=='<') {
    s++;
    name = s;
    while (*s && *s!='>')
      s++;
    name = cnvstr(name,s-name);
    if (*s)
      s = skip(s+1);
  }
#endif
  else {
    name = s;
    while (!ISEOL(s) && !isspace((unsigned char)*s) && *s!=',')
      s++;
    if (s != name) {
      name = cnvstr(name,s-name);
      s = skip(s);
    }
    else
      name = NULL;  /* nothing read */
  }
  *start = s;
  return name;
}


static char *skip_eol(char *s,char *e)
{
  while (s<e && *s!='\0' && *s!='\n' && *s!='\r')
    s++;
  return s;
}


char *skip_line(char *s)
{
  while (*s!='\0')
    s++;
  return s;
}


char *skip_identifier(char *s)
{
  char *name = s;

  if (ISIDSTART(*s)) {
    s++;
    while (ISIDCHAR(*s))
      s++;
    if (s = CHKIDEND(name,s))
      if (!ISBADID(name,s-name))
        return s;
  }
  return NULL;
}


char *parse_identifier(char **s)
{
  char *name = *s;
  char *endname;

  if (endname = skip_identifier(name)) {
    *s = endname;
    return cnvstr(name,endname-name);
  }
  return NULL;
}


char *skip_string(char *s,char delim,size_t *size)
/* skip a string, optionally store the size in bytes in size, when not NULL */
{
  size_t n = 0;

  if (*s != delim)
    general_error(6,delim);  /* " expected */
  else
    s++;

  while (*s) {
    if (*s == '\\') {
      s = escape(s,NULL);
    }
    else {
      if (*s++ == delim) {
        if (*s == delim)
          s++;  /* allow """" to be recognized as " */
        else
          break;
      }
    }
    n++;
  }

  if (*(s-1) != delim)
    general_error(6,delim);  /* " expected */
  if (size)
    *size = n;
  return s;
}


char *read_string(char *p,char *s,char delim,int width)
/* read string contents with width bits for each character into a buffer p,
   optionally starting with a delim-character, excluding the terminating
   character */
{
  char c;

  if (width & 7)
    ierror(0);
  width >>= 3;

  if (*s == delim)
    s++;

  while (*s) {
    if (*s == '\\') {
      s = escape(s,&c);
    }
    else {
      c = *s++;
      if (c == delim) {
        if (*s == delim)
          s++;  /* allow """" to be recognized as " */
        else
          break;
      }
    }
    if (p) {
      setval(BIGENDIAN,p,width,(unsigned char)c);
      p += width;
    }
  }
  return s;
}


dblock *parse_string(char **str,char delim,int width)
{
  size_t size;
  dblock *db;
  char *s = *str;

  if (width & 7)
    ierror(0);

  /* how many bytes do we need for the string? */
  skip_string(s,delim,&size);
  if (size == 1)
    return NULL; /* it's just one char, so use eval_expr() on it */

  db = new_dblock();
  db->size = size * (size_t)(width>>3);
  db->data = db->size ? mymalloc(db->size) : NULL;

  /* now copy the string for real into the dblock */
  s = read_string((char *)db->data,s,delim,width);
  *str = s;
  return db;
}


char *parse_symbol(char **s)
/* return pointer to an allocated local/global symbol string, or NULL */
{
  char *name;

  name = get_local_label(s);
  if (name == NULL)
    name = parse_identifier(s);
  return name;
}


char *parse_labeldef(char **line,int needcolon)
/* Parse a global or local label definition at the beginning of a line.
   Return pointer to its allocate buffer, when valid.
   A trailing colon may be required or optional, but becomes mandatory
   when label is not starting at first column. */
{
  char *s = *line;
  char *labname;

  if (isspace((unsigned char )*s)) {
    s = skip(s);
    needcolon = 1;  /* colon required, when label doesn't start at 1st column */
  }

  if (labname = parse_symbol(&s)) {
    s = skip(s);
    if (*s == ':') {
      s++;
      needcolon = 0;
    }
    if (needcolon) {
      myfree(labname);
      labname = NULL;
    }
    else
      *line = s;
  }
  return labname;
}



int check_indir(char *p,char *q)
/* returns true when the whole sequence between p and q starts and ends with */
/* parentheses and there are no unbalanced parentheses within */
{
  char c;
  int n;

  p = skip(p);
  if (*p++ != '(')
    return 0;

  n = 1;
  while (n>0 && p<q) {
    c = *p++;
    if (c == '(')
      n++;
    else if (c == ')')
      n--;
  }
  if (p < q)
    p = skip(p);

  return n==0 && p>=q;
}


static struct namelen *dirlist_match(char *s,char *e,struct namelen *list)
/* check if a directive from the list matches the current source location */
{
  char *name;
  size_t len;

  if (!ISIDSTART(*s) || (!isspace((unsigned char )*(s-1)) && *(s-1)!='\0'))
    return NULL;  /* cannot be start of directive */

  name = s++;
  while (s<e && ISIDCHAR(*s))
    s++;

  if (!isspace((unsigned char )*s) && *s!='\0')
    return NULL;  /* cannot be end of directive */

  while (len = list->len) {
    if (s-name==len && !strnicmp(name,list->name,len))
        return list;
    list++;
  }
  return NULL;
}


static size_t dirlist_minlen(struct namelen *list)
{
  size_t minlen;

  if (list == NULL)
    ierror(0);
  for (minlen=list->len; list->len; list++) {
    if (list->len < minlen)
      minlen = list->len;
  }
  return minlen;
}


/* add a skipped macro/repeat line to the listing */
static void list_skipped_line(char *p)
{
  listing *new = new_listing(cur_src,cur_src->line);
  size_t len = p - cur_src->srcptr;

  if (len>0 && (*(p-1)=='\n' || *(p-1)=='\r'))
    len--;
  if (len >= MAXLISTSRC)
    len = MAXLISTSRC - 1;
  memcpy(new->txt,cur_src->srcptr,len);
  new->txt[len] = '\0';
}


/* return the line number of the last real source text instance, i.e.
   not the line within a macro or repetition */
int real_line(void)
{
  source *src = cur_src;
  int line = src->line;

  while (src->num_params >= 0) {
    line = src->parent_line;
    src = src->parent;
    if (src == NULL)
      ierror(0);  /* macro must have a parent */
  }
  return line;
}


void new_repeat(int rcnt,char *name,char *vals,
                struct namelen *reptlist,struct namelen *endrlist)
{
  if (cur_macro==NULL && cur_src!=NULL && enddir_list==NULL) {
    enddir_list = endrlist;
    enddir_minlen = dirlist_minlen(endrlist);
    reptdir_list = reptlist;
    rept_start = cur_src->srcptr;
    rept_name = name;
    rept_vals = vals;
    rept_cnt = rcnt;  /* also REPT_IRP or REPT_IRPC */

    /* get start-line of repetition in the last real source text */
    if (cur_src->defsrc) {
      rept_defsrc = cur_src->defsrc;
      rept_defline = cur_src->defline + cur_src->line;
    }
    else {
      rept_defsrc = cur_src;
      rept_defline = cur_src->line;
    }
  }
  else
    ierror(0);
}


int find_macarg_name(source *src,char *name,size_t len)
{
  struct macarg *ma;
  int idx;

  /* named macro arguments */
  if (src->macro != NULL) {
    for (idx=0,ma=src->argnames; ma!=NULL && idx<maxmacparams;
         idx++,ma=ma->argnext) {
      /* @@@ case-sensitive comparison? */
      if (ma->arglen==len && strncmp(ma->argname,name,len)==0)
        return idx;
    }
  }

  /* repeat-loop iterator name */
  if (src->irpname != NULL) {
    if (strlen(src->irpname)==len && strncmp(src->irpname,name,len)==0) {
      /* copy current iterator value to param[MAXMACPARAMS] */
      src->param[MAXMACPARAMS] = src->irpvals->argname;
      src->param_len[MAXMACPARAMS] = src->irpvals->arglen;
      return IRPVAL;
    }
  }

  return -1;
}


/* append a macarg object at the end of the given list */
struct macarg *addmacarg(struct macarg **list,char *start,char *end)
{
  struct macarg *lastarg,*newarg;
  int len = end-start;

  /* count arguments */
  if (lastarg = *list) {
    while (lastarg->argnext)
      lastarg = lastarg->argnext;
  }

  newarg = mymalloc(sizeof(struct macarg) + len);
  newarg->argnext = NULL;
  if (start != NULL) {
    memcpy(newarg->argname,start,len);
    newarg->argname[len] = '\0';
    newarg->arglen = len;
  }
  else
    newarg->arglen = MACARG_REQUIRED;
  if (lastarg)
    lastarg->argnext = newarg;
  else
    *list = newarg;

  return newarg;
}


macro *new_macro(char *name,struct namelen *maclist,struct namelen *endmlist,
                 char *args)
{
  hashdata data;
  macro *m = NULL;

  if (cur_macro==NULL && cur_src!=NULL && enddir_list==NULL) {
    m = mymalloc(sizeof(macro));
    m->name = mystrdup(name);
    if (nocase_macros)
      strtolower(m->name);
    m->num_argnames = -1;
    m->argnames = m->defaults = NULL;
    m->recursions = 0;
    m->vararg = -1;
    m->srcdebug = !msource_disable;

    /* remember the start-line of this macro definition in the real source */
    if (cur_src->defsrc)
      general_error(26,cur_src->name);  /* macro definition inside macro */
    m->defsrc = cur_src;
    m->defline = cur_src->line;

    /* looking for name conflicts */
    if (find_name_nc(mnemohash,name,&data)) {
      int idx;

      m->text = cur_src->srcptr;
      for (idx=data.idx;
           idx<mnemonic_cnt && !stricmp(mnemonics[idx].name,name); idx++) {
        if (MNEMONIC_VALID(idx)) {
          m->text = NULL;
          general_error(51);  /* name conflicts with mnemonic */
          break;
        }
      }
    }
    else if (find_name_nc(dirhash,name,&data)) {
      m->text = NULL;
      general_error(52);  /* name conflicts with directive */
    }
    else
      m->text = cur_src->srcptr;

    cur_macro = m;
    enddir_list = endmlist;
    enddir_minlen = dirlist_minlen(endmlist);
    macrdir_list = maclist;
    rept_cnt = -1;
    rept_start = NULL;

    if (args) {
      /* named arguments have been given */
      struct macarg *ma;
      char *end;
      int n = 0;

      m->num_argnames = 0;
      args = skip(args);
      while (args!=NULL && !ISEOL(args)) {
        end = SKIP_MACRO_ARGNAME(args);

        if (end!=NULL && end-args!=0 && m->vararg<0) {
          /* add another argument name */
          ma = addmacarg(&m->argnames,args,end);
          m->num_argnames++;
          args = skip(end);
          if (end = MACRO_ARG_OPTS(m,n,ma->argname,args))
            args = skip(end);  /* got defaults and qualifiers */
          else
            addmacarg(&m->defaults,args,args);  /* default is empty */
          n++;
        }
        else {
          general_error(42);  /* illegal macro argument */
          break;
        }
        args = MACRO_ARG_SEP(args);  /* check for separator between names */
      }
    }
  }
  else
    ierror(0);

  return m;
}


macro *find_macro(char *name,int name_len)
{
  hashdata data;

  if (nocase_macros) {
    if (!find_namelen_nc(macrohash,name,name_len,&data))
      return NULL;
  }
  else {
    if (!find_namelen(macrohash,name,name_len,&data))
      return NULL;
  }
  return data.ptr;
}


/* check if 'name' is a known macro, then execute macro context */
int execute_macro(char *name,int name_len,char **q,int *q_len,int nq,
                  char *s)
{
  macro *m;
  source *src;
  struct macarg *ma;
  int n;
  struct namelen param,arg;
#if MAX_QUALIFIERS>0
  char *defq[MAX_QUALIFIERS];
  int defq_len[MAX_QUALIFIERS];
#endif
#ifdef NO_MACRO_QUALIFIERS
  char *nptr = name;

  /* Instruction qualifiers are ignored for macros on this architecture.
     So we have to determine the length of the mnemonic again. */
  while (*nptr && !isspace((unsigned char)*nptr))
    nptr++;
  name_len = nptr - name;
  nq = 0;
#endif

  if ((m = find_macro(name,name_len)) == NULL)
    return 0;

  /* it's a macro: read arguments and execute it */
  if (m->recursions >= maxmacrecurs) {
    general_error(56,maxmacrecurs);  /* maximum macro recursions reached */
    return 0;
  }
  m->recursions++;

  src = new_source(m->name,NULL,m->text,m->size);
  src->defsrc = m->defsrc;
  src->defline = m->defline;
  src->argnames = m->argnames;
  src->srcdebug = m->srcdebug;

#if MAX_QUALIFIERS>0
  /* remember given qualifiers, or use the cpu's default qualifiers */
  for (n=0; n<nq; n++) {
    src->qual[n] = q[n];
    src->qual_len[n] = q_len[n];
  }
  nq = set_default_qualifiers(defq,defq_len);
  for (; n<nq; n++) {
    src->qual[n] = defq[n];
    src->qual_len[n] = defq_len[n];
  }
  src->num_quals = nq;
#endif

  /* fill in the defaults first */
  for (n=0,ma=m->defaults; n<maxmacparams; n++) {
    if (ma != NULL) {
      src->param[n] = ma->arglen==MACARG_REQUIRED ? NULL : ma->argname;
      src->param_len[n] = ma->arglen==MACARG_REQUIRED ? 0 : ma->arglen;
      ma = ma->argnext;
    }
    else {
      src->param[n] = emptystr;
      src->param_len[n] = 0;
    }
  }
    
  /* read macro arguments from operand field */
  s = skip(s);
  n = 0;
  while (!ISEOL(s) && n<maxmacparams) {
    if (n>=0 && m->vararg==n) {
      /* Varargs: take rest of line as argument */
      char *start = s;
      char *end = s;

      while (!ISEOL(s)) {
        s++;
        if (!isspace((unsigned char)*s))
          end = s;  /* remember last non-blank character */
      }
      src->param[n] = start;
      src->param_len[n] = end - start;
      n++;
      break;
    }
    else if ((s = parse_macro_arg(m,s,&param,&arg)) != NULL) {
      if (arg.len) {
        /* argument selected by keyword */
        if (n <= 0) {
          n = find_macarg_name(src,arg.name,arg.len);
          if (n >= 0) {
            src->param[n] = param.name;
            src->param_len[n] = param.len;
          }
          else
            general_error(72);  /* undefined macro argument name */
          n = -1;
        }
        else
          general_error(71);  /* cannot mix positional and keyword arguments */
      }
      else {
        /* argument selected by position n */
        if (n >= 0) {
          if (param.len > 0) {
            src->param[n] = param.name;
            src->param_len[n] = param.len;
          }
          n++;
        }
        else
          general_error(71);  /* cannot mix positional and keyword arguments */
      }
    }
    else
      break;

    s = skip(s);
    if (!(s = MACRO_PARAM_SEP(s)))  /* check for separator between params. */
      break;
  }

  if (n < 0)
    n = m->num_argnames>=0 ? m->num_argnames : 0;
  if (n > maxmacparams) {
    general_error(27,maxmacparams);  /* number of args exceeded */
    n = maxmacparams;
  }

  src->macro = m;
  src->num_params = n;      /* >=0 indicates macro source */

  for (n=0; n<maxmacparams; n++) {
    if (src->param[n] == NULL) {
      /* required, but missing */
      src->param[n] = emptystr;
      src->param_len[n] = 0;
      general_error(73,n+1);  /* required macro argument was left out */
    }
  }

  EXEC_MACRO(src);          /* syntax-module dependant initializations */
  cur_src = src;            /* execute! */
  return 1;
}


int leave_macro(void)
{
  if (cur_src->macro != NULL) {
    end_source(cur_src);
    return 0;  /* ok */
  }
  general_error(36);  /* no current macro to exit */
  return -1;
}


/* remove an already defined macro from the hash table */
int undef_macro(char *name)
{
  if (find_macro(name,strlen(name))) {
    rem_hashentry(macrohash,name,nocase_macros);
    return 1;
  }
  general_error(68);  /* macro does not exist */
  return 0;
}


static void start_repeat(char *rept_end)
{
  char buf[MAXPATHLEN];
  source *src;
  char *p,*val;
  int i;

  reptdir_list = NULL;
  if ((rept_cnt<0 && rept_cnt!=REPT_IRP && rept_cnt!=REPT_IRPC) ||
      cur_src==NULL || strlen(cur_src->name) + 24 >= MAXPATHLEN)
    ierror(0);

  if (rept_cnt != 0) {
    sprintf(buf,"REPEAT:%s:line %d",cur_src->name,cur_src->line);
    src = new_source(buf,NULL,rept_start,rept_end-rept_start);
    src->irpname = rept_name;
    src->irpvals = NULL;
    src->defsrc = rept_defsrc;
    src->defline = rept_defline;
#ifdef REPTNSYM
    src->reptn = 0;
    set_internal_abs(REPTNSYM,0);
#endif

    switch (rept_cnt) {
      case REPT_IRP:  /* iterate with comma separated values */
        p = rept_vals;
        if (!*p) {
          addmacarg(&src->irpvals,p,p);
          src->repeat = 1;
        }
        else {
          src->repeat = 0;
          while (val = parse_name(&p)) {
            addmacarg(&src->irpvals,val,val+strlen(val));
            myfree(val);
            src->repeat++;
            p = skip(p);
            if (*p == ',')
              p = skip(p+1);
          }
        }
        break;

      case REPT_IRPC:  /* iterate with each character */
        p = rept_vals;
        if (!*p) {
          addmacarg(&src->irpvals,p,p);
          src->repeat = 1;
        }
        else {
          src->repeat = 0;
          do {
            while (!isspace((unsigned char )*p) && *p!=',' && !ISEOL(p)) {
              addmacarg(&src->irpvals,p,p+1);
              src->repeat++;
              p++;
            }
            p = skip(p);
            if (*p == ',')
              p = skip(p+1);
          }
          while (!ISEOL(p));
        }
        break;

      default:  /* iterate rept_cnt times */
        src->repeat = (unsigned long)rept_cnt;
        break;
    }

    if (cur_src->macro != NULL) {
      /* repetition in a macro: get parameters */
      src->num_params = cur_src->num_params;
      for (i=0; i<src->num_params; i++) {
        src->param[i] = cur_src->param[i];
        src->param_len[i] = cur_src->param_len[i];
      }
#if MAX_QUALIFIERS > 0
      src->num_quals = cur_src->num_quals;
      for (i=0; i<src->num_quals; i++) {
        src->qual[i] = cur_src->qual[i];
        src->qual_len[i] = cur_src->qual_len[i];
      }
#endif
      src->argnames = cur_src->argnames;
    }

    if (src->repeat == 0)
      ierror(0);
    cur_src = src;  /* repeat it */
  }
}


static void add_macro(void)
{
  if (cur_macro!=NULL && cur_src!=NULL) {
    if (cur_macro->text != NULL) {
      hashdata data;

      cur_macro->size = cur_src->srcptr - cur_macro->text;
      cur_macro->next = first_macro;
      first_macro = cur_macro;
      data.ptr = cur_macro;
      add_hashentry(macrohash,cur_macro->name,data);
    }
    cur_macro = NULL;
  }
  else
    ierror(0);
}


/* copy macro parameter n to line buffer, return -1 when out of space */
int copy_macro_param(source *src,int n,char *d,int len)
{
  if (n < 0) {
    ierror(0);
  }
  else if ((n<src->num_params && n<maxmacparams) || n==IRPVAL) {
    int i;

    if (n == IRPVAL)
      n = MAXMACPARAMS;

    for (i=0; i<src->param_len[n] && len>0; i++,len--)
      *d++ = src->param[n][i];

    return i==src->param_len[n] ? i : -1;
  }
  return 0;
}


/* copy nth macro qualifier to line buffer, return -1 when out of space */
int copy_macro_qual(source *src,int n,char *d,int len)
{
#if MAX_QUALIFIERS > 0
  if (n < src->num_quals) {
    int i;
    for (i=0; i<src->qual_len[n] && len>0; i++,len--)
      *d++ = src->qual[n][i];
    return i==src->qual_len[n] ? i : -1;
  }
#endif
  return 0;
}

/* Switch to a named offset section which defines the structure. */
int new_structure(char *name)
{
  hashdata data;

  if (cur_struct) {
    general_error(48);  /* cannot declare structure within structure */
    return 0;
  }

  struct_prevsect = current_section;
  switch_offset_section(name,-1);
  data.ptr = cur_struct = current_section;
  add_hashentry(structhash,cur_struct->name,data);
  return 1;
}

/* Finish the structure definition and return the previous section. */
int end_structure(section **prev)
{
  if (cur_struct) {
    *prev = struct_prevsect;
    cur_struct = struct_prevsect = NULL;
    return 1;
  }
  general_error(49);  /* no structure */
  return 0;
}


section *find_structure(char *name,int name_len)
{
  hashdata data;
  section *s;

  if (cur_struct!=NULL && !strcmp(cur_struct->name,name))
    general_error(55);  /* illegal structure recursion */

  if (find_namelen(structhash,name,name_len,&data))
    s = data.ptr;
  else
    s = NULL;
  return s;
}


/* reads the next input line */
char *read_next_line(void)
{
  char *s,*srcend,*d;
  int nparam,len;
  int skip_listing = 0;
  char *rept_end = NULL;

  /* check if end of source is reached */
  for (;;) {
    srcend = cur_src->text + cur_src->size;
    if (cur_src->srcptr >= srcend || *(cur_src->srcptr) == '\0') {
      if (--cur_src->repeat > 0) {
        struct macarg *irpval;

        cur_src->srcptr = cur_src->text;  /* back to start */
        cur_src->line = 0;
        if (cur_src->irpname!=NULL && (irpval=cur_src->irpvals)!=NULL) {
          /* remove and deallocate leading irpval of last iteration */
          cur_src->irpvals = irpval->argnext;
          myfree(irpval);
        }
#ifdef REPTNSYM
        set_internal_abs(REPTNSYM,++cur_src->reptn);
#endif
      }
      else {
        if (cur_src->macro != NULL) {
          if (--cur_src->macro->recursions < 0)
            ierror(0);
        }
        myfree(cur_src->linebuf);  /* linebuf is no longer needed, saves memory */
        cur_src->linebuf = NULL;
        if (cur_src->parent == NULL)
          return NULL;  /* no parent source means end of assembly! */
        cur_src = cur_src->parent;  /* return to parent source */
#ifdef CARGSYM
        if (cur_src->cargexp) {
          symbol *carg = internal_abs(CARGSYM);
          carg->expr = cur_src->cargexp;  /* restore parent CARG */
        }
#endif
#ifdef REPTNSYM
        set_internal_abs(REPTNSYM,cur_src->reptn);  /* restore parent REPTN */
#endif
      }
    }
    else
      break;
  }

  cur_src->line++;
  s = cur_src->srcptr;
  d = cur_src->linebuf;
  len = cur_src->bufsize - 2;  /* excluding \0 at start and beginning */
  nparam = cur_src->num_params;

  /* line buffer starts with 0, to allow checks for left-hand character */
  *d++ = 0;

  if (enddir_list!=NULL && (size_t)(srcend-s)>enddir_minlen) {
    /* reading a definition, like a macro or a repeat-block, until an
       end directive is found */
    struct namelen *dir;
    int rept_nest = 1;

    if (nparam>=0 && cur_macro!=NULL)     /* @@@ needed? */
        general_error(26,cur_src->name);  /* macro definition inside macro */

    while (s <= (srcend-enddir_minlen)) {
      if (dir = dirlist_match(s,srcend,enddir_list)) {
        if (cur_macro != NULL) {
          add_macro();  /* link macro-definition into hash-table */
          enddir_list = NULL;
          break;
        }
        else if (--rept_nest == 0) {
          rept_end = s;
          enddir_list = NULL;
          break;
        }
        s += dir->len;
      }
      else if (cur_macro==NULL && reptdir_list!=NULL &&
               (dir = dirlist_match(s,srcend,reptdir_list)) != NULL) {
        s += dir->len;
        rept_nest++;
      }
#ifdef MACRO_IN_MACRO_CHECK  /* caution: misdetection in operands possible */
      else if (cur_macro!=NULL &&
               (dir = dirlist_match(s,srcend,macrdir_list)) != NULL) {
        general_error(26,cur_macro->name);  /* macro definition inside macro */
      }
#endif

      if (*s=='\"' || *s=='\'') {
        char c = *s++;

        while (s<=(srcend-enddir_minlen) && *s!=c && *s!='\n' && *s!='\r') {
          if (*s=='\\' && (*(s+1)=='\"' || *(s+1)=='\''))
            s = escape(s,NULL);
          else
            s++;
        }
      }

      if (ISEOL(s))
        s = skip_eol(s,srcend);

      if ((*s=='\n') ||
          (*s=='\r' && *(s-1)!='\n' && (s>=(srcend-1) || *(s+1)!='\n'))) {
        /* new line */
        if (listena)
          list_skipped_line(s);
        cur_src->srcptr = s + 1;
        cur_src->line++;
      }
      s++;
    }

    if (enddir_list) {
      if (cur_macro)
        general_error(25,cur_macro->name);  /* missing ENDM directive */
      else
        general_error(32);  /* missing ENDR directive */
    }

    /* ignore rest of line, treat as comment */
    s = skip_eol(s,srcend);
    if (listena) {
      list_skipped_line(s);
      skip_listing = 1;
    }
  }

  if (nparam<0 && cur_src->irpname!=NULL)
    nparam = 0;  /* expand current repeat-iterator symbol into source */

  /* copy next line to linebuf */
  while (s<srcend && *s!='\0') {
    int nc;

    if (nparam >= 0)
      nc = expand_macro(cur_src,&s,d,len);  /* try macro arg. expansion */
    else
      nc = 0;

    if (nc > 0) {
      /* expanded macro arguments */
      len -= nc;
      d += nc;
    }
    else if (nc == 0) {
      /* copy next character */
      if (*s == '\r') {
        if ((s>cur_src->srcptr && *(s-1)=='\n') ||
            (s<(srcend-1) && *(s+1)=='\n')) {
          /* ignore \r in \r\n and \n\r combinations */
          s++;
        }
        else {
          /* treat a single \r as \n */
          s++;
          break;
        }
      }
      else if (*s == '\n') {
        s++;
        break;
      }
      else if (len > 0) {
        *d++ = *s++;
        len--;
      }
      else
      	nc = -1;
    }

    if (nc < 0) {
      /* line buffer ran out of space, allocate a bigger one */
      int offs = d - cur_src->linebuf;

      /* double its size */
      len += cur_src->bufsize;
      cur_src->bufsize += cur_src->bufsize;
      cur_src->linebuf = myrealloc(cur_src->linebuf,cur_src->bufsize);
      d = cur_src->linebuf + offs;
      if (debug)
        printf("Doubled line buffer size to %lu bytes.\n",cur_src->bufsize);
    }
  }

  *d = '\0';
  cur_src->srcptr = s;
  s = cur_src->linebuf+1;

  if (listena && !skip_listing) {
    listing *new = new_listing(cur_src,cur_src->line);

    strncpy(new->txt,s,MAXLISTSRC);
    new->txt[MAXLISTSRC-1] = '\0';
  }
  if (rept_end)
    start_repeat(rept_end);
  return s;
}


int init_parse(void)
{
  macrohash = new_hashtable(MACROHTABSIZE);
  structhash = new_hashtable(STRUCTHTABSIZE);
  return 1;
}
