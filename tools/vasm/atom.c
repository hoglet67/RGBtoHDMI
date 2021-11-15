/* atom.c - atomic objects from source */
/* (c) in 2010-2020 by Volker Barthelmann and Frank Wille */

#include "vasm.h"


/* searches mnemonic list and tries to parse (via the cpu module)
   the operands according to the mnemonic requirements; returns an
   instruction or 0 */
instruction *new_inst(char *inst,int len,int op_cnt,char **op,int *op_len)
{
#if MAX_OPERANDS!=0
  operand ops[MAX_OPERANDS];
  int j,k,mnemo_opcnt,omitted,skipped,again;
#endif
  int i,inst_found=0;
  hashdata data;
  instruction *new;

  new = mymalloc(sizeof(*new));
#if HAVE_INSTRUCTION_EXTENSION
  init_instruction_ext(&new->ext);
#endif
#if MAX_OPERANDS!=0 && CLEAR_OPERANDS_ON_START!=0
  /* reset operands to allow the cpu-backend to parse them only once */
  memset(ops,0,sizeof(ops));
#endif

  if (find_namelen_nc(mnemohash,inst,len,&data)) {
    i = data.idx;

    /* try all mnemonics with the same name until operands match */
    do {
      inst_found = 1;
      if (!MNEMONIC_VALID(i)) {
        i++;
        continue;  /* try next */
      }

#if MAX_OPERANDS!=0

#if CLEAR_OPERANDS_ON_MNEMO!=0
  /* reset all operands for every new mnemonic */
  memset(ops,0,sizeof(ops));
#endif

#if 0 /* @@@ was ALLOW_EMPTY_OPS */
      mnemo_opcnt = op_cnt<MAX_OPERANDS ? op_cnt : MAX_OPERANDS;
#else
      for (j=0; j<MAX_OPERANDS; j++)
        if (mnemonics[i].operand_type[j] == 0)
          break;
      mnemo_opcnt = j;	/* number of expected operands for this mnemonic */
#endif
      inst_found = 2;
      save_symbols();  /* make sure we can restore symbols to this point */

      for (j=k=omitted=skipped=0,again=-1; j<mnemo_opcnt; j++) {

        if (op_cnt+omitted < mnemo_opcnt &&
            OPERAND_OPTIONAL(&ops[j],mnemonics[i].operand_type[j])) {
          omitted++;
        }
        else {
          int rc;

          if (k >= op_cnt) {
            /* we may be missing mandatory operands */
            if (j == again)
              j++;  /* but probably not after PO_AGAIN */
            break;
          }

          rc = parse_operand(op[k],op_len[k],&ops[j],
                             mnemonics[i].operand_type[j]);

          if (rc == PO_CORRUPT) {
            /* operand has errors and will never match */
            myfree(new);
            restore_symbols();
            return 0;
          }
          if (rc == PO_NOMATCH)
            break;     /* operand type does not match */
          if (rc == PO_NEXT)
            continue;  /* after PO_AGAIN: use this arg. on the next operand */

          /* MATCH, proceed to next parsed operand */
          k++;
          if (rc == PO_SKIP) {
            /* but skip next operand type from table */
            j++;
            skipped++;
          }
          else if (rc == PO_AGAIN) {
            /* try to work on the same operand again with next arg. */
            again = j--;
          }
        }
      }

      if ((!IGNORE_FIRST_EXTRA_OP || mnemo_opcnt>0) &&
          (j<mnemo_opcnt || k<op_cnt)) {
        /* No match. Try next mnemonic. */
        i++;
        restore_symbols();
        continue;
      }

      /* Matched! Copy operands. */
      mnemo_opcnt -= skipped;
      for (j=0; j<mnemo_opcnt; j++) {
        new->op[j] = mymalloc(sizeof(operand));
        *new->op[j] = ops[j];
      }
      for(; j<MAX_OPERANDS; j++)
        new->op[j] = NULL;

#endif /* MAX_OPERANDS!=0 */

      new->code = i;
      return new;
    }
    while (i<mnemonic_cnt && !strnicmp(mnemonics[i].name,inst,len)
           && mnemonics[i].name[len]==0);
  }

  switch (inst_found) {
    case 1:
      general_error(8);  /* instruction not supported by cpu */
      break;
    case 2:
      general_error(0);  /* illegal operand types */
      break;
    default:
      general_error(1,cnvstr(inst,len));  /* completely unknown mnemonic */
      break;
  }
  myfree(new);
  return 0;
}


instruction *copy_inst(instruction *ip)
{
#if MAX_OPERANDS!=0
  static operand newop[MAX_OPERANDS];
#endif
  static instruction newip;
  int i;

  newip.code = ip->code;
#if MAX_QUALIFIERS!=0
  for (i=0; i<MAX_QUALIFIERS; i++)
    newip.qualifiers[i] = ip->qualifiers[i];
#endif
#if MAX_OPERANDS!=0
  for (i=0; i<MAX_OPERANDS; i++) {
    if (ip->op[i] != NULL) {
      newip.op[i] = &newop[i];
      *newip.op[i] = *ip->op[i];
    }
    else
      newip.op[i] = NULL;
  }
#endif
#if HAVE_INSTRUCTION_EXTENSION
  memcpy(&newip.ext,&ip->ext,sizeof(instruction_ext));
#endif
  return &newip;
}


dblock *new_dblock(void)
{
  dblock *new = mymalloc(sizeof(*new));

  new->size = 0;
  new->data = 0;
  new->relocs = 0;
  return new;
}


sblock *new_sblock(expr *space,size_t size,expr *fill)
{
  sblock *sb = mymalloc(sizeof(sblock));

  sb->space = 0;
  sb->space_exp = space;
  sb->size = size;
  if (!(sb->fill_exp = fill))
    memset(sb->fill,space_init,MAXPADBYTES);
  sb->relocs = 0;
  sb->maxalignbytes = 0;
  sb->flags = 0;
  return sb;
}


static size_t space_size(sblock *sb,section *sec,taddr pc)
{
  utaddr space=0;

  if (eval_expr(sb->space_exp,(taddr *)&space,sec,pc) || !final_pass)
    sb->space = space;
  else
    general_error(30);  /* expression must be constant */

  if (final_pass && sb->fill_exp) {
    if (sb->size <= sizeof(taddr)) {
      /* space is filled with an expression which may also need relocations */
      symbol *base=NULL;
      taddr fill;
      utaddr i;

      if (!eval_expr(sb->fill_exp,&fill,sec,pc)) {
        if (find_base(sb->fill_exp,&base,sec,pc)==BASE_ILLEGAL)
          general_error(38);  /* illegal relocation */
      }
      copy_cpu_taddr(sb->fill,fill,sb->size);
      if (base && !sb->relocs) {
        /* generate relocations */
        for (i=0; i<space; i++)
          add_extnreloc(&sb->relocs,base,fill,REL_ABS,
                        0,sb->size<<3,sb->size*i);
      }
    }
    else
      general_error(30);  /* expression must be constant */
  }

  return sb->size * space;
}


static size_t roffs_size(reloffs *roffs,section *sec,taddr pc)
{
  taddr offs;

  eval_expr(roffs->offset,&offs,sec,pc);
  offs = sec->org + offs - pc;
  return offs>0 ? offs : 0;
}


/* adds an atom to the specified section; if sec==0, the current
   section is used */
void add_atom(section *sec,atom *a)
{
  if (!sec) {
    sec = default_section();
    if (!sec) {
      general_error(3);
      return;
    }
  }

  a->changes = 0;
  a->src = cur_src;
  a->line = cur_src!=NULL ? cur_src->line : 0;

  if (sec->last) {
    atom *pa = sec->last;

    pa->next = a;
    /* make sure that a label on the same line gets the same alignment */
    if (pa->type==LABEL && pa->line==a->line &&
        (a->type==INSTRUCTION || a->type==DATADEF || a->type==SPACE))
      pa->align = a->align;
  }
  else
    sec->first = a;
  a->next = 0;
  sec->last = a;

  sec->pc = pcalign(a,sec->pc);
  a->lastsize = atom_size(a,sec,sec->pc);
  sec->pc += a->lastsize;
  if (a->align > sec->align)
    sec->align = a->align;

  if (listena) {
    a->list = last_listing;
    if (last_listing) {
      if (!last_listing->atom)
        last_listing->atom = a;
    }
  }
  else
    a->list = 0;
}


size_t atom_size(atom *p,section *sec,taddr pc)
{
  switch(p->type) {
    case VASMDEBUG:
    case LABEL:
    case LINE:
    case OPTS:
    case PRINTTEXT:
    case PRINTEXPR:
    case RORG:
    case RORGEND:
    case ASSERT:
    case NLIST:  /* it has a size, but not in the current section */
      return 0;
    case DATA:
      return p->content.db->size;
    case INSTRUCTION:
      return p->content.inst->code>=0?
             instruction_size(p->content.inst,sec,pc):0;
    case SPACE:
      return space_size(p->content.sb,sec,pc);
    case DATADEF:
      return (p->content.defb->bitsize+7)/8;
    case ROFFS:
      return roffs_size(p->content.roffs,sec,pc);
    default:
      ierror(0);
      break;
  }
  return 0;
}


static void print_instruction(FILE *f,instruction *p)
{
  int i;

  printf("inst %d(%s) ",p->code,p->code>=0?mnemonics[p->code].name:"deleted");
#if MAX_OPERANDS!=0
  for (i=0; i<MAX_OPERANDS; i++)
    printf("%p ",(void *)p->op[i]);
#endif
}


void print_atom(FILE *f,atom *p)
{
  size_t i;
  rlist *rl;

  switch (p->type) {
    case VASMDEBUG:
      fprintf(f,"vasm debug directive");
      break;
    case LABEL:
      fprintf(f,"symbol: ");
      print_symbol(f,p->content.label);
      break;
    case DATA:
      fprintf(f,"data(%lu): ",(unsigned long)p->content.db->size);
      for (i=0;i<p->content.db->size;i++)
        fprintf(f,"%02x ",p->content.db->data[i]);
      for (rl=p->content.db->relocs; rl; rl=rl->next)
        print_reloc(f,rl->type,rl->reloc);
      break;
    case INSTRUCTION:
      print_instruction(f,p->content.inst);
      break;
    case SPACE:
      fprintf(f,"space(%lu,fill=",
              (unsigned long)(p->content.sb->space*p->content.sb->size));
      for (i=0; i<p->content.sb->size; i++)
        fprintf(f,"%02x%c",(unsigned char)p->content.sb->fill[i],
                (i==p->content.sb->size-1)?')':' ');
      for (rl=p->content.sb->relocs; rl; rl=rl->next)
        print_reloc(f,rl->type,rl->reloc);
      break;
    case DATADEF:
      fprintf(f,"datadef(%lu bits)",(unsigned long)p->content.defb->bitsize);
      break;
    case LINE:
      fprintf(f,"line: %d of %s",p->content.srcline,getdebugname());
      break;
#if HAVE_CPU_OPTS
    case OPTS:
      print_cpu_opts(f,p->content.opts);
      break;
#endif
    case PRINTTEXT:
      fprintf(f,"text: \"%s\"",p->content.ptext);
      break;
    case PRINTEXPR:
      fprintf(f,"expr: ");
      print_expr(f,p->content.pexpr->print_exp);
      break;
    case ROFFS:
      fprintf(f,"roffs: offset ");
      print_expr(f,p->content.roffs->offset);
      fprintf(f,",fill=");
      if (p->content.roffs->fillval)
        print_expr(f,p->content.roffs->fillval);
      else
        fprintf(f,"none");
      break;
    case RORG:
      fprintf(f,"rorg: relocate to 0x%llx",ULLTADDR(*p->content.rorg));
      break;
    case RORGEND:
      fprintf(f,"rorg end");
      break;
    case ASSERT:
      fprintf(f,"assert: %s (message: %s)\n",p->content.assert->expstr,
              p->content.assert->msgstr?p->content.assert->msgstr:emptystr);
      break;
    case NLIST:
      fprintf(f,"nlist: %s (type %d, other %d, desc %d) with value ",
              p->content.nlist->name!=NULL ? p->content.nlist->name : "<NULL>",
              p->content.nlist->type,p->content.nlist->other,
              p->content.nlist->desc);
      if (p->content.nlist->value != NULL)
        print_expr(f,p->content.nlist->value);
      else
        fprintf(f,"NULL");
      break;
    default:
      ierror(0);
  }
}


/* prints and formats an expression from a PRINTEXPR atom */
void atom_printexpr(printexpr *pexp,section *sec,taddr pc)
{
  taddr t;
  long long v;
  int i;

  eval_expr(pexp->print_exp,&t,sec,pc);
  if (pexp->type==PEXP_SDEC && (t&(1LL<<(pexp->size-1)))!=0) {
    /* signed decimal */
    v = -1;
    v &= ~(long long)MAKEMASK(pexp->size);
  }
  else
    v = 0;
  v |= t & MAKEMASK(pexp->size);

  switch (pexp->type) {
    case PEXP_HEX:
      printf("%llX",(unsigned long long)v);
      break;
    case PEXP_SDEC:
      printf("%lld",v);
      break;
    case PEXP_UDEC:
      printf("%llu",(unsigned long long)v);
      break;
    case PEXP_BIN:
      for (i=pexp->size-1; i>=0; i--)
        putchar((v & (1LL<<i)) ? '1' : '0');
      break;
    case PEXP_ASC:
      for (i=((pexp->size+7)>>3)-1; i>=0; i--) {
        unsigned char c = (v>>(i*8))&0xff;
        putchar(isprint(c) ? c : '.');
      }
      break;
    default:
      ierror(0);
      break;
  }
}


atom *clone_atom(atom *a)
{
  atom *new = mymalloc(sizeof(atom));
  void *p;

  memcpy(new,a,sizeof(atom));

  switch (a->type) {
    /* INSTRUCTION and DATADEF have to be cloned as well, because they will
       be deallocated and transformed into DATA during assemble() */
    case INSTRUCTION:
      p = mymalloc(sizeof(instruction));
      memcpy(p,a->content.inst,sizeof(instruction));
      new->content.inst = p;
      break;
    case DATADEF:
      p = mymalloc(sizeof(defblock));
      memcpy(p,a->content.defb,sizeof(defblock));
      new->content.defb = p;
      break;
    default:
      break;
  }

  new->next = 0;
  new->src = NULL;
  new->line = 0;
  new->list = NULL;
  return new;
}


atom *add_data_atom(section *sec,size_t sz,taddr alignment,taddr c)
{
  dblock *db = new_dblock();
  atom *a;

  db->size = sz;
  db->data = mymalloc(sz);
  if (sz > 1)
    setval(BIGENDIAN,db->data,sz,c);
  else
    *(db->data) = c;

  a = new_data_atom(db,alignment);
  add_atom(sec,a);
  return a;
}


void add_leb128_atom(section *sec,taddr c)
{
  taddr b;

  do {
    b = c & 0x7f;
    if ((c >>= 7) != 0)
      b |= 0x80;
    add_data_atom(sec,1,1,b);
  } while (c != 0);
}


void add_sleb128_atom(section *sec,taddr c)
{
  int done = 0;
  taddr b;

  do {
    b = c & 0x7f;
    c >>= 7;  /* assumes arithmetic shifts! */
    if ((c==0 && !(b&0x40)) || (c==-1 && (b&0x40)))
      done = 1;
    else
      b |= 0x80;
    add_data_atom(sec,1,1,b);
  } while (!done);
}


atom *add_bytes_atom(section *sec,void *p,size_t sz)
{
  dblock *db = new_dblock();
  atom *a;

  db->size = sz;
  db->data = mymalloc(sz);
  memcpy(db->data,p,sz);
  a = new_data_atom(db,1);
  add_atom(sec,a);
  return a;
}


atom *new_atom(int type,taddr align)
{
  atom *new = mymalloc(sizeof(*new));

  new->next = NULL;
  new->type = type;
  new->align = align;
  return new;
}


atom *new_inst_atom(instruction *p)
{
  atom *new = new_atom(INSTRUCTION,inst_alignment);

  new->content.inst = p;
  return new;
}


atom *new_data_atom(dblock *p,taddr align)
{
  atom *new = new_atom(DATA,align);

  new->content.db = p;
  return new;
}


atom *new_label_atom(symbol *p)
{
  atom *new = new_atom(LABEL,1);

  new->content.label = p;
  return new;
}


atom *new_space_atom(expr *space,size_t size,expr *fill)
{
  atom *new = new_atom(SPACE,1);

  if (size<1)
    ierror(0);  /* usually an error in syntax-module */
  new->content.sb = new_sblock(space,size,fill);
  return new;
}  


atom *new_datadef_atom(size_t bitsize,operand *op)
{
  atom *new = new_atom(DATADEF,DATA_ALIGN(bitsize));

  new->content.defb = mymalloc(sizeof(*new->content.defb));
  new->content.defb->bitsize = bitsize;
  new->content.defb->op = op;
  return new;
}


atom *new_srcline_atom(int line)
{
  atom *new = new_atom(LINE,1);

  new->content.srcline = line;
  return new;
}


atom *new_opts_atom(void *o)
{
  atom *new = new_atom(OPTS,1);

  new->content.opts = o;
  return new;
}


atom *new_text_atom(char *txt)
{
  atom *new = new_atom(PRINTTEXT,1);

  new->content.ptext = txt ? txt : "\n";
  return new;
}


atom *new_expr_atom(expr *exp,int type,int size)
{
  atom *new = new_atom(PRINTEXPR,1);

  new->content.pexpr = mymalloc(sizeof(*new->content.pexpr));
  if (exp==NULL || type<PEXP_HEX || type>PEXP_ASC || size<1
      || size>sizeof(long long)*8)
    ierror(0);
  new->content.pexpr->print_exp = exp;
  new->content.pexpr->type = type;
  new->content.pexpr->size = size;
  return new;
}


atom *new_roffs_atom(expr *offs,expr *fill)
{
  atom *new = new_atom(ROFFS,1);

  new->content.roffs = mymalloc(sizeof(*new->content.roffs));
  new->content.roffs->offset = offs;
  new->content.roffs->fillval = fill;
  return new;
}


atom *new_rorg_atom(taddr raddr)
{
  atom *new = new_atom(RORG,1);
  taddr *newrorg = mymalloc(sizeof(taddr));

  *newrorg = raddr;
  new->content.rorg = newrorg;
  return new;
}


atom *new_rorgend_atom(void)
{
  return new_atom(RORGEND,1);
}


atom *new_assert_atom(expr *aexp,char *exp,char *msg)
{
  atom *new = new_atom(ASSERT,1);

  new->content.assert = mymalloc(sizeof(*new->content.assert));
  new->content.assert->assert_exp = aexp;
  new->content.assert->expstr = exp;
  new->content.assert->msgstr = msg;
  return new;
}


atom *new_nlist_atom(char *name,int type,int other,int desc,expr *value)
{
  atom *new = new_atom(NLIST,1);

  new->content.nlist = mymalloc(sizeof(*new->content.nlist));
  new->content.nlist->name = name;
  new->content.nlist->type = type;
  new->content.nlist->other = other;
  new->content.nlist->desc = desc;
  new->content.nlist->value = value;
  return new;
}
