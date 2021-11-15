/* output_aout.c a.out output driver for vasm */
/* (c) in 2008-2016,2020 by Frank Wille */

#include "vasm.h"
#include "output_aout.h"
#if defined(OUTAOUT) && defined(MID)
static char *copyright="vasm a.out output module 0.8 (c) 2008-2016,2020 Frank Wille";

static section *sections[3];
static utaddr secsize[3];
static utaddr secoffs[3];
static int sectype[] = { N_TEXT, N_DATA, N_BSS };
static int secweak[] = { N_WEAKT, N_WEAKD, N_WEAKB };

static struct SymTabList aoutsymlist; 
static struct StrTabList aoutstrlist; 
static struct list treloclist;
static struct list dreloclist;

static int mid = -1;
static int isPIC = 1;

#define SECT_ALIGN 4  /* .text and .data are aligned to 32 bits */


static int aout_getinfo(symbol *sym)
{
  int type;

  switch (TYPE(sym)) {
    case TYPE_UNKNOWN:
    case TYPE_FILE:
    case TYPE_SECTION:  /* this will be ignored later */
      type = AUX_UNKNOWN;
      break;
    case TYPE_OBJECT:
      type = AUX_OBJECT;
      break;
    case TYPE_FUNCTION:
      type = AUX_FUNC;
      break;
    default:
      ierror(0);
      break;
  }
  return type;
}


static int aout_getbind(symbol *sym)
{
  if (sym->flags & WEAK)
    return BIND_WEAK;
  else if (sym->type!=IMPORT && !(sym->flags & EXPORT))
    return BIND_LOCAL;
  else if ((sym->type!=IMPORT && (sym->flags & EXPORT)) ||
           (sym->type==IMPORT && (sym->flags & COMMON)))
    return BIND_GLOBAL;
  else
    ierror(0);
  return -1;
}


static uint32_t aoutstd_getrinfo(rlist **rl,int xtern,char *sname,int be)
/* Convert vasm relocation type into standard a.out relocations, */
/* as used by M68k and x86 targets. */
/* For xtern=-1, return true when this relocation requires a base symbol. */
{
  nreloc *nr;

  if (nr = (nreloc *)(*rl)->reloc) {
    rlist *rl2 = (*rl)->next;
    uint32_t r=0,s=4;
    nreloc *nr2;
    int b=0;

    switch ((*rl)->type) {
      case REL_ABS: b=-1; break;
      case REL_PC: b=RSTDB_pcrel; break;
      case REL_SD: b=RSTDB_baserel; break;
      default: goto unsupp_reloc;
    }

    if (xtern == -1)  /* just query symbol-based relocation */
      return b==RSTDB_baserel || b==RSTDB_jmptable;

    nr2 = rl2!=NULL ? (nreloc *)rl2->reloc : NULL;

    if (nr->bitoffset==0 && (nr2==NULL || nr2->byteoffset!=nr->byteoffset)
        && (nr->mask & MAKEMASK(nr->size)) == MAKEMASK(nr->size)) {
      switch (nr->size) {
        case 8: s=0; break;
        case 16: s=1; break;
        case 32: s=2; break;
      }
    }
#ifdef VASM_CPU_JAGRISC
    else if (nr->size==16 && nr2!=NULL && nr2->size==16 &&
        nr2->byteoffset==nr->byteoffset &&
        ((nr->mask==0xffff && nr2->mask==0xffff0000) ||
         (nr->mask==0xffff0000 && nr2->mask==0xffff)) &&
        ((nr->bitoffset==0 && nr2->bitoffset==16) ||
         (nr->bitoffset==16 && nr2->bitoffset==0))) {
      /* Jaguar RISC MOVEI instruction with swapped words, indicated by
         a set RSTDB_copy bit. */
      b = RSTDB_copy;
      s = 2;
      *rl = (*rl)->next;  /* skip additional entry */
    }
#endif

    if (b!=0 && s<4) {
      if (b > 0)
        setbits(be,&r,sizeof(r)<<3,(unsigned)b,1,1);
      setbits(be,&r,sizeof(r)<<3,RSTDB_length,RSTDS_length,s);
      setbits(be,&r,sizeof(r)<<3,RSTDB_extern,RSTDS_extern,xtern?1:0);
      return readbits(be,&r,sizeof(r)<<3,RELB_reloc,RELS_reloc);
    }
  }

unsupp_reloc:
  unsupp_reloc_error(*rl);
  return ~0;
}


static void aout_initwrite(section *firstsec)
{
  section *sec;

  if (mid == -1)
    mid = MID;

  initlist(&aoutstrlist.l);
  aoutstrlist.hashtab = mycalloc(STRHTABSIZE*sizeof(struct StrTabNode *));
  aoutstrlist.nextoffset = 4;  /* first string is always at offset 4 */
  initlist(&aoutsymlist.l);
  aoutsymlist.hashtab = mycalloc(SYMHTABSIZE*sizeof(struct SymbolNode *));
  aoutsymlist.nextindex = 0;
  initlist(&treloclist);
  initlist(&dreloclist);

  /* find exactly one .text, .data and .bss section for a.out */
  sections[S_TEXT] = sections[S_DATA] = sections[S_BSS] = NULL;
  secsize[S_TEXT] = secsize[S_DATA] = secsize[S_BSS] = 0;

  for (sec=firstsec; sec; sec=sec->next) {
    int i;

    /* section size is assumed to be in in (sec->pc - sec->org), otherwise
       we would have to calculate it from the atoms and store it there */
    if (get_sec_size(sec) > 0 || (sec->flags & HAS_SYMBOLS)) {
      i = get_sec_type(sec);
      if (i == S_MISS) {
        output_error(3,sec->attr);  /* section attributes not supported */
        i = S_TEXT;
      }
      if (i == S_ABS)
        continue;  /* ignore ORG sections for later */
      if (!sections[i]) {
        sections[i] = sec;
        secsize[i] = get_sec_size(sec);
        sec->idx = i;  /* section index 0:text, 1:data, 2:bss */
      }
      else
        output_error(7,sec->name);
    }
  }

  /* now scan for absolute ORG-sections and add their aligned size to .text */
  for (sec=firstsec; sec; sec=sec->next) {
    if (sec->flags & ABSOLUTE)
      secsize[S_TEXT] += balign(secsize[S_TEXT],sec->align) + get_sec_size(sec);
  }

  secoffs[S_TEXT] = 0;
  secoffs[S_DATA] = secsize[S_TEXT] + balign(secsize[S_TEXT],SECT_ALIGN);
  secoffs[S_BSS] = secoffs[S_DATA] + secsize[S_DATA] +
                  balign(secsize[S_DATA],SECT_ALIGN);
}


static uint32_t aout_addstr(char *s)
/* add a new symbol name to the string table and return its offset */
{
  struct StrTabNode **chain;
  struct StrTabNode *sn;

  if (s == NULL)
    return 0;
  if (*s == '\0')
    return 0;

  /* search string in hash table */
  chain = &aoutstrlist.hashtab[hashcode(s)%STRHTABSIZE];
  while (sn = *chain) {
    if (!strcmp(s,sn->str))
      return (sn->offset);  /* it's already in, return offset */
    chain = &sn->hashchain;
  }

  /* new string table entry */
  *chain = sn = mymalloc(sizeof(struct StrTabNode));
  sn->hashchain = NULL;
  sn->str = s;
  sn->offset = aoutstrlist.nextoffset;
  addtail(&aoutstrlist.l,&sn->n);
  aoutstrlist.nextoffset += strlen(s) + 1;
  return sn->offset;
}


static struct SymbolNode *aout_addsym(char *name,uint8_t type,int8_t other,
                                      int16_t desc,uint32_t value,int be)
/* append a new symbol to the symbol list */
{
  struct SymbolNode *sym = mycalloc(sizeof(struct SymbolNode));

  sym->name = name!=NULL ? name : emptystr;
  sym->index = aoutsymlist.nextindex++;
  setval(be,&sym->s.n_strx,4,aout_addstr(name));
  sym->s.n_type = type;
  sym->s.n_other = other;
  setval(be,&sym->s.n_desc,2,desc);
  setval(be,&sym->s.n_value,4,value);
  addtail(&aoutsymlist.l,&sym->n);
  return sym;
}


static uint32_t aout_addsymhash(char *name,taddr value,int bind,
                                int info,int type,int desc,int be)
/* add a new symbol, return its symbol table index */
{
  struct SymbolNode **chain,*sym;

  chain = &aoutsymlist.hashtab[hashcode(name?name:emptystr)%SYMHTABSIZE];
  while (sym = *chain)
    chain = &sym->hashchain;

  /* new symbol table entry */
  *chain = sym = aout_addsym(name,type,((bind&0xf)<<4)|(info&0xf),
                             desc,value,be);
  return sym->index;
}


static int aout_findsym(char *name,int be)
/* find a symbol by its name, return symbol table index or -1 */
{
  struct SymbolNode **chain = &aoutsymlist.hashtab[hashcode(name)%SYMHTABSIZE];
  struct SymbolNode *sym;

  while (sym = *chain) {
    if (!strcmp(name,sym->name) && !(sym->s.n_type & N_STAB))
      return ((int)sym->index);
    chain = &sym->hashchain;
  }
  return (-1);
}


static void aout_symconvert(symbol *sym,int symbind,int syminfo,int be)
/* convert vasm symbol into a.out symbol(s) */
{
  taddr val = get_sym_value(sym);
  taddr size = get_sym_size(sym);
  int ext = (symbind == BIND_GLOBAL) ? N_EXT : 0;
  int type = 0;

  if (TYPE(sym) == TYPE_SECTION) {
    return;   /* section symbols are ignored in a.out! */
  }
  else if (TYPE(sym) == TYPE_FILE) {
    type = N_FN | N_EXT;  /* special case: file name symbol */
    size = 0;
  }
  else {
    if (sym->flags & COMMON) {
      /* common symbol */
      #if 0 /* GNU binutils prefers N_UNDF with val!=0 instead of N_COMM! */
      type = N_COMM | ext;
      #else
      type = N_UNDF | N_EXT;
      #endif
      val = size;
      size = 0;
    }
    else if (sym->flags & WEAK) {
      /* weak symbol */
      switch (sym->type) {
        case LABSYM: type=secweak[sym->sec->idx]; break;
        case IMPORT: type=N_WEAKU; break;
        case EXPRESSION: type=N_WEAKA; break;
        default: ierror(0); break;
      }
    }
    else if (sym->sec) {
      /* address symbol */
      if (!(sym->sec->flags & ABSOLUTE)) {
        type = sectype[sym->sec->idx] | ext;
        val += secoffs[sym->sec->idx];  /* a.out requires to add sec. offset */
      }
      else  /* absolute ORG section: convert labels to ABS symbols */
        type = N_ABS | ext;
    }
    else if (sym->type==EXPRESSION) {
      if (sym->flags & EXPORT) {
        /* absolute symbol */
        type = N_ABS | ext;
      }
      else
        return;  /* ignore local expressions */
    }
    /* @@@ else if (indirect symbols?) {
      aout_addsymhash(sym->name,0,symbind,0,N_INDR|ext,0,be);
      aout_addsymhash(sym->indir_name,0,0,0,N_UNDF|N_EXT,0,be);
      return;
    }*/
    else
      ierror(0);
  }

  aout_addsymhash(sym->name,val,symbind,syminfo,type,0,be);
  if (size) {
    /* append N_SIZE symbol declaring the previous symbol's size */
    aout_addsymhash(sym->name,size,symbind,syminfo,N_SIZE,0,be);
  }
}


static void aout_addsymlist(symbol *sym,int bind,int type,int be)
/* add all symbols with specified bind and type to the a.out symbol list */
{
  for (; sym; sym=sym->next) {
    /* ignore symbols preceded by a '.' and internal symbols */
    if ((sym->type!=IMPORT || (sym->flags&WEAK) || (sym->flags&COMMON))
        && *sym->name != '.' && *sym->name!=' ' && !(sym->flags&VASMINTERN)) {
      int syminfo = aout_getinfo(sym);
      int symbind = aout_getbind(sym);

      if (symbind == bind && (!type || (syminfo == type))) {
        aout_symconvert(sym,symbind,syminfo,be);
      }
    }
  }
}


static void aout_debugsyms(int be)
/* add stabs to the a.out symbol list */
{
  struct stabdef *nlist = first_nlist;
  uint32_t val;

  while (nlist != NULL) {
    val = nlist->value;
    if (nlist->base != NULL) {
      /* include section base offset in symbol value */
      if (LOCREF(nlist->base)) {
        if (!(nlist->base->sec->flags & ABSOLUTE))
          val += secoffs[nlist->base->sec->idx];
      }
      else
        ierror(0);  /* @@@ handle external references! How? */
    }
    aout_addsym(nlist->name.ptr,nlist->type,nlist->other,nlist->desc,val,be);
    nlist = nlist->next;
  }
}


static void aout_addreloclist(struct list *rlst,uint32_t raddr,
                              uint32_t rindex,uint32_t rinfo,int be)
/* add new relocation_info to .text or .data reloc-list */
{
  struct RelocNode *rn = mymalloc(sizeof(struct RelocNode));

  setval(be,rn->r.r_address,4,raddr);
  setbits(be,rn->r.r_info,32,RELB_symbolnum,RELS_symbolnum,rindex);
  setbits(be,rn->r.r_info,32,RELB_reloc,RELS_reloc,rinfo);
  addtail(rlst,&rn->n);

  if (isPIC && !readbits(be,rn->r.r_info,32,RSTDB_pcrel,1)
      && !readbits(be,rn->r.r_info,32,RSTDB_baserel,1)) {
    /* the relocation is probably absolute, so it is no PIC anymore */
    isPIC = 0;
  }
}


static uint32_t aout_convert_rlist(int be,atom *a,int secid,
                                   struct list *rlst,taddr pc,
                                   uint32_t (*getrinfo)
                                            (rlist **,int,char *,int))
/* convert all of an atom's relocs into a.out relocations */
{
  uint32_t rsize = 0;
  rlist *rl;

  if (a->type == DATA)
    rl = a->content.db->relocs;
  else if (a->type == SPACE)
    rl = a->content.sb->relocs;
  else
    rl = NULL;

  if (rl == NULL)
    return 0;  /* no relocs or not the right atom type */

  do {
    nreloc *r = (nreloc *)rl->reloc;
    symbol *refsym = r->sym;
    taddr val = get_sym_value(refsym);
    taddr add = nreloc_real_addend(r);
#if SDAHACK
    int based = getrinfo(&rl,-1,sections[secid]->name,be) != 0;
#endif

    if (LOCREF(refsym)) {
      /* this is a local relocation */
      int rsecid = refsym->sec->idx;

      aout_addreloclist(rlst,pc+r->byteoffset,sectype[rsecid],
                        getrinfo(&rl,0,sections[secid]->name,be),
                        be);
#if SDAHACK
      if (!based)  /* @@@ 'based' does not really happen under Unix */
#endif
        val += secoffs[rsecid];
      rsize += sizeof(struct relocation_info);
    }
    else if (EXTREF(refsym)) {
      /* this is an external symbol reference */
      int symidx;

      if ((symidx = aout_findsym(refsym->name,be)) == -1)
        symidx = aout_addsymhash(refsym->name,0,0,0,N_UNDF|N_EXT,0,be);
      aout_addreloclist(rlst,pc+r->byteoffset,symidx,
                        getrinfo(&rl,1,sections[secid]->name,be),
                        be);
      rsize += sizeof(struct relocation_info);
    }
    else
      ierror(0);

    /* patch addend for a.out */
    if (rl->type == REL_PC)
      val -= pc + r->byteoffset;
    if (a->type == DATA)
      setval(be,a->content.db->data+r->byteoffset,r->size>>3,val+add);
    else if (a->type==SPACE && a->content.sb->space!=0) {
      setval(be,a->content.sb->fill,r->size>>3,val+add);
      a->content.sb->space = 0;  /* we only need to patch 'fill' once */
    }
  }
  while (rl = rl->next);

  return rsize;
}


static uint32_t aout_addrelocs(int be,int secid,struct list *rlst,
                               uint32_t (*getrinfo)(rlist **,int,char *,int))
/* creates a.out relocations for a single section (.text or .data) */
{
  uint32_t rtabsize=0;

  if (sections[secid]) {
    atom *a;
    taddr pc=0,npc;

    for (a=sections[secid]->first; a; a=a->next) {
      npc = pcalign(a,pc);
      rtabsize += aout_convert_rlist(be,a,secid,rlst,npc,getrinfo);
      pc = npc + atom_size(a,sections[secid],npc);
    }
  }
  return rtabsize;
}


static void aout_header(FILE *f,uint32_t mag,uint32_t flag,
                        uint32_t tsize,uint32_t dsize,uint32_t bsize,
                        uint32_t syms,uint32_t entry,
                        uint32_t trsize,uint32_t drsize,int be)
/* write an a.out header */
{
  struct aout_hdr h;

  SETMIDMAG(&h,mag,mid,flag);
  setval(be,h.a_text,4,tsize);
  setval(be,h.a_data,4,dsize);
  setval(be,h.a_bss,4,bsize);
  setval(be,h.a_syms,4,syms);
  setval(be,h.a_entry,4,entry);
  setval(be,h.a_trsize,4,trsize);
  setval(be,h.a_drsize,4,drsize);
  fwdata(f,&h,sizeof(struct aout_hdr));
}


static void aout_writesection(FILE *f,section *sec,taddr sec_align)
{
  if (sec) {
    atom *a;
    taddr pc=0,npc;

    for (a=sec->first; a; a=a->next) {
      npc = fwpcalign(f,a,sec,pc);
      if (a->type == DATA)
        fwdata(f,a->content.db->data,a->content.db->size);
      else if (a->type == SPACE)
        fwsblock(f,a->content.sb);
      pc = npc + atom_size(a,sec,npc);
    }
    fwalign(f,pc,sec_align);
  }
}


static void aout_writeorg(FILE *f,section *sec,taddr sec_align)
/* write all absolute ORG-sections appended to .text */
{
  taddr pc = get_sec_size(sections[S_TEXT]);
  taddr npc;
  atom *a;

  for (; sec; sec=sec->next) {
    if (sec->flags & ABSOLUTE) {
      fwalign(f,pc,sec->align);
      for (a=sec->first; a; a=a->next) {
        npc = fwpcalign(f,a,sec,pc);
        if (a->type == DATA)
          fwdata(f,a->content.db->data,a->content.db->size);
        else if (a->type == SPACE)
          fwsblock(f,a->content.sb);
        pc = npc + atom_size(a,sec,npc);
      }
    }
  }
  fwalign(f,pc,sec_align);
}


void aout_writerelocs(FILE *f,struct list *l)
{
  struct RelocNode *rn;

  while (rn = (struct RelocNode *)remhead(l))
    fwdata(f,&rn->r,sizeof(struct relocation_info));
}


void aout_writesymbols(FILE *f)
{
  struct SymbolNode *sym;

  while (sym = (struct SymbolNode *)remhead(&aoutsymlist.l))
    fwdata(f,&sym->s,sizeof(struct nlist32));
}


void aout_writestrings(FILE *f,int be)
{
  if (aoutstrlist.nextoffset > 4) {
    struct StrTabNode *stn;

    fw32(f,aoutstrlist.nextoffset,be);
    while (stn = (struct StrTabNode *)remhead(&aoutstrlist.l))
      fwdata(f,stn->str,strlen(stn->str)+1);
  }
}


static void write_output(FILE *f,section *sec,symbol *sym)
{
  int be = BIGENDIAN;
  uint32_t trsize,drsize;

  aout_initwrite(sec);
  aout_addsymlist(sym,BIND_GLOBAL,0,be);
  aout_addsymlist(sym,BIND_WEAK,0,be);
  if (!no_symbols) {
    aout_addsymlist(sym,BIND_LOCAL,0,be);
    aout_debugsyms(be);
  }
  trsize = aout_addrelocs(be,S_TEXT,&treloclist,aoutstd_getrinfo);
  drsize = aout_addrelocs(be,S_DATA,&dreloclist,aoutstd_getrinfo);

  aout_header(f,OMAGIC,isPIC?EX_PIC:0,
              secsize[S_TEXT] + balign(secsize[S_TEXT],SECT_ALIGN),
              secsize[S_DATA] + balign(secsize[S_DATA],SECT_ALIGN),
              secsize[S_BSS],
              aoutsymlist.nextindex * sizeof(struct nlist32),
              0,trsize,drsize,be);
  aout_writesection(f,sections[S_TEXT],0);
  aout_writeorg(f,sec,SECT_ALIGN);
  aout_writesection(f,sections[S_DATA],SECT_ALIGN);
  aout_writerelocs(f,&treloclist);
  aout_writerelocs(f,&dreloclist);
  aout_writesymbols(f);
  aout_writestrings(f,be);
}


static int output_args(char *p)
{
  if (!strncmp(p,"-mid=",5)) {
    mid = atoi(p+5);
    return 1;
  }
  return 0;
}


int init_output_aout(char **cp,void (**wo)(FILE *,section *,symbol *),
                     int (**oa)(char *))
{
  *cp = copyright;
  *wo = write_output;
  *oa = output_args;
  unnamed_sections = 1;  /* output format doesn't support named sections */
  secname_attr = 1;
  return 1;
}

#else

int init_output_aout(char **cp,void (**wo)(FILE *,section *,symbol *),
                     int (**oa)(char *))
{
  return 0;
}
#endif
