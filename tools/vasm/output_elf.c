/* output_elf.c ELF output driver for vasm */
/* (c) in 2002-2016,2020 by Frank Wille */

#include "vasm.h"
#include "output_elf.h"
#include "stabs.h"
#if ELFCPU && defined(OUTELF)
static char *copyright="vasm ELF output module 2.7 (c) 2002-2016,2020 Frank Wille";

static int keep_empty_sects;

static int be,cpu,bits;
static unsigned elfrelsize,shtreloc;

static hashtable *elfsymhash;
static struct list shdrlist,symlist,relalist;
static struct StrTabList shstrlist,strlist,stabstrlist;

static unsigned symtabidx,strtabidx,shstrtabidx;
static unsigned symindex,shdrindex;
static unsigned stabidx,stabstralign;
static utaddr stablen;
static char stabname[] = ".stab";


static unsigned addString(struct StrTabList *sl,char *s)
{
  struct StrTabNode *sn = mymalloc(sizeof(struct StrTabNode));
  unsigned idx = sl->index;

  sn->str = s;
  addtail(&(sl->l),&(sn->n));
  sl->index += (unsigned)strlen(s) + 1;
  return idx;
}


static void init_lists(void)
{
  elfsymhash = new_hashtable(ELFSYMHTABSIZE);
  initlist(&shdrlist);
  initlist(&symlist);
  initlist(&relalist);
  shstrlist.index = strlist.index = 0;
  initlist(&shstrlist.l);
  initlist(&strlist.l);
  symindex = shdrindex = stabidx = 0;
  addString(&shstrlist,emptystr);  /* first string is always "" */
  symtabidx = addString(&shstrlist,".symtab");
  strtabidx = addString(&shstrlist,".strtab");
  shstrtabidx = addString(&shstrlist,".shstrtab");
  addString(&strlist,emptystr);
  if (!no_symbols && first_nlist) {
    initlist(&stabstrlist.l);
    addString(&stabstrlist,emptystr);
  }
}


static struct Shdr32Node *addShdr32(void)
{
  struct Shdr32Node *s = mycalloc(sizeof(struct Shdr32Node));

  addtail(&shdrlist,&(s->n));
  shdrindex++;
  return s;
}


static struct Shdr64Node *addShdr64(void)
{
  struct Shdr64Node *s = mycalloc(sizeof(struct Shdr64Node));

  addtail(&shdrlist,&(s->n));
  shdrindex++;
  return s;
}


static struct Symbol32Node *addSymbol32(char *name)
{
  struct Symbol32Node *sn = mycalloc(sizeof(struct Symbol32Node));
  hashdata data;

  addtail(&symlist,&(sn->n));
  if (name) {
    sn->name = name;
    setval(be,sn->s.st_name,4,addString(&strlist,name));
  }
  data.ptr = sn;
  sn->idx = symindex++;
  add_hashentry(elfsymhash,name?name:emptystr,data);
  return sn;
}


static struct Symbol64Node *addSymbol64(char *name)
{
  struct Symbol64Node *sn = mycalloc(sizeof(struct Symbol64Node));
  hashdata data;

  addtail(&symlist,&(sn->n));
  if (name) {
    sn->name = name;
    setval(be,sn->s.st_name,4,addString(&strlist,name));
  }
  data.ptr = sn;
  sn->idx = symindex++;
  add_hashentry(elfsymhash,name?name:emptystr,data);
  return sn;
}


static void newSym32(char *name,elfull value,elfull size,uint8_t bind,
                     uint8_t type,unsigned shndx)
{
  struct Symbol32Node *elfsym = addSymbol32(name);

  setval(be,elfsym->s.st_value,4,value);
  setval(be,elfsym->s.st_size,4,size);
  elfsym->s.st_info[0] = ELF32_ST_INFO(bind,type);
  setval(be,elfsym->s.st_shndx,2,shndx);
}


static void newSym64(char *name,elfull value,elfull size,uint8_t bind,
                     uint8_t type,unsigned shndx)
{
  struct Symbol64Node *elfsym = addSymbol64(name);

  setval(be,elfsym->s.st_value,8,value);
  setval(be,elfsym->s.st_size,8,size);
  elfsym->s.st_info[0] = ELF64_ST_INFO(bind,type);
  setval(be,elfsym->s.st_shndx,2,shndx);
}


static void addRel32(elfull o,elfull a,elfull i,elfull r)
{
  if (RELA) {
    struct Rela32Node *rn = mymalloc(sizeof(struct Rela32Node));

    setval(be,rn->r.r_offset,4,o);
    setval(be,rn->r.r_addend,4,a);
    setval(be,rn->r.r_info,4,ELF32_R_INFO(i,r));
    addtail(&relalist,&(rn->n));
  }
  else {
    struct Rel32Node *rn = mymalloc(sizeof(struct Rel32Node));

    setval(be,rn->r.r_offset,4,o);
    setval(be,rn->r.r_info,4,ELF32_R_INFO(i,r));
    addtail(&relalist,&(rn->n));
  }
}


static void addRel64(elfull o,elfull a,elfull i,elfull r)
{
  if (RELA) {
    struct Rela64Node *rn = mymalloc(sizeof(struct Rela64Node));

    setval(be,rn->r.r_offset,8,o);
    setval(be,rn->r.r_addend,8,a);
    setval(be,rn->r.r_info,8,ELF64_R_INFO(i,r));
    addtail(&relalist,&(rn->n));
  }
  else {
    struct Rel64Node *rn = mymalloc(sizeof(struct Rel64Node));

    setval(be,rn->r.r_offset,8,o);
    setval(be,rn->r.r_info,8,ELF64_R_INFO(i,r));
    addtail(&relalist,&(rn->n));
  }
}


static void *makeShdr32(elfull name,elfull type,elfull flags,elfull offset,
                        elfull size,elfull info,elfull align,elfull entsize)
{
  struct Shdr32Node *shn;

  shn = addShdr32();
  setval(be,shn->s.sh_name,4,name);
  setval(be,shn->s.sh_type,4,type);
  setval(be,shn->s.sh_flags,4,flags);
  setval(be,shn->s.sh_offset,4,offset);
  setval(be,shn->s.sh_size,4,size);
  setval(be,shn->s.sh_info,4,info);
  setval(be,shn->s.sh_addralign,4,align);
  setval(be,shn->s.sh_entsize,4,entsize);
  /* @@@ set sh_addr to org? */
  return shn;
}


static void *makeShdr64(elfull name,elfull type,elfull flags,elfull offset,
                        elfull size,elfull info,elfull align,elfull entsize)
{
  struct Shdr64Node *shn;

  shn = addShdr64();
  setval(be,shn->s.sh_name,4,name);
  setval(be,shn->s.sh_type,4,type);
  setval(be,shn->s.sh_flags,8,flags);
  setval(be,shn->s.sh_offset,8,offset);
  setval(be,shn->s.sh_size,8,size);
  setval(be,shn->s.sh_info,4,info);
  setval(be,shn->s.sh_addralign,8,align);
  setval(be,shn->s.sh_entsize,8,entsize);
  /* @@@ set sh_addr to org? */
  return shn;
}


static unsigned findelfsymbol(char *name)
/* find symbol with given name in symlist, return its index */
{
  hashdata data;

  if (find_name(elfsymhash,name,&data))
    return ((struct Symbol32Node *)data.ptr)->idx;
  return 0;
}


static void init_ident(unsigned char *id,uint8_t class)
{
  static char elfid[4] = { 0x7f,'E','L','F' };

  memcpy(&id[EI_MAG0],elfid,4);
  id[EI_CLASS] = class;
  id[EI_DATA] = be ? ELFDATA2MSB : ELFDATA2LSB;
  id[EI_VERSION] = EV_CURRENT;
  memset(&id[EI_PAD],0,EI_NIDENT-EI_PAD);
}


static uint32_t elf_sec_type(section *s)
/* scan section attributes for type */
{
  if (!strncmp(s->name,".note",5))
    return SHT_NOTE;

  switch (get_sec_type(s)) {
    case S_TEXT:
    case S_DATA:
      return SHT_PROGBITS;
    case S_BSS:
      return SHT_NOBITS;
  }
#if 0
  output_error(3,attr);  /* section attributes not supported */
  return SHT_NULL;
#else
  return SHT_PROGBITS;
#endif
}


static utaddr get_sec_flags(char *a)
/* scan section attributes for flags (read, write, alloc, execute) */
{
  utaddr f = 0;

  while (*a) {
    switch (*a++) {
      case 'a':
        f |= SHF_ALLOC;
        break;
      case 'w':
        f |= SHF_WRITE;
        break;
      case 'x':
        f |= SHF_EXECINSTR;
        break;
    }
  }
  return f;
}


static uint8_t get_sym_info(symbol *s)
/* determine symbol-info: function, object, section, etc. */
{
  switch (TYPE(s)) {
    case TYPE_OBJECT:
      return STT_OBJECT;
    case TYPE_FUNCTION:
      return STT_FUNC;
    case TYPE_SECTION:
      return STT_SECTION;
    case TYPE_FILE:
      return STT_FILE;
  }
  return STT_NOTYPE;
}


static unsigned get_sym_index(symbol *s)
{
  if (s->flags & COMMON)
    return SHN_COMMON;
  if (s->type == IMPORT)
    return SHN_UNDEF;
  if (s->sec)
    return (unsigned)s->sec->idx;
  return SHN_ABS;
}


static utaddr get_reloc_type(rlist **rl,
                             utaddr *roffset,taddr *addend,symbol **refsym)
{
  rlist *rl2;
  utaddr mask;
  int pos,size;

  utaddr t = 0;

  *roffset = 0;
  *addend = 0;
  *refsym = NULL;

#ifdef VASM_CPU_M68K
#include "elf_reloc_68k.h"
#endif

#ifdef VASM_CPU_PPC
#include "elf_reloc_ppc.h"
#endif

#ifdef VASM_CPU_ARM
#include "elf_reloc_arm.h"
#endif

#ifdef VASM_CPU_X86
  if (bytespertaddr == 8) {
#include "elf_reloc_x86_64.h"
  }
  else {
#include "elf_reloc_386.h"
  }
#endif

#ifdef VASM_CPU_JAGRISC
#include "elf_reloc_jag.h"
#endif

  if (!t)
    unsupp_reloc_error(*rl);

  return t;
}


static utaddr make_relocs(rlist *rl,utaddr pc,
                          void (*newsym)(char *,elfull,elfull,uint8_t,
                                         uint8_t,unsigned),
                          void (*addrel)(elfull,elfull,elfull,elfull))
/* convert all of an atom's relocations into ELF32/ELF64 relocs */
{
  utaddr ro = 0;

  if (rl) {
    do {
      utaddr rtype,offset;
      taddr addend;
      symbol *refsym;

      if (rtype = get_reloc_type(&rl,&offset,&addend,&refsym)) {

        if (LOCREF(refsym)) {
          /* this is a local relocation */
          addrel(pc+offset,addend,refsym->sec->idx,rtype);
          ro += elfrelsize;
        }
        else if (EXTREF(refsym)) {
          /* this is an external symbol reference */
          unsigned idx = findelfsymbol(refsym->name);

          if (idx == 0) {
            /* create a new symbol, which can be referenced */
            idx = symindex;
            newsym(refsym->name,0,0,STB_GLOBAL,STT_NOTYPE,0);
          }
          addrel(pc+offset,addend,idx,rtype);
          ro += elfrelsize;
        }
        else
          ierror(0);
      }
    }
    while (rl = rl->next);
  }

  return ro;
}


static utaddr make_stabreloc(utaddr pc,struct stabdef *nlist,
                             void (*newsym)(char *,elfull,elfull,uint8_t,
                                            uint8_t,unsigned),
                             void (*addrel)(elfull,elfull,elfull,elfull))
{
  rlist dummyrl;
  rlist *rl = &dummyrl;
  nreloc nrel;

  nrel.byteoffset = offsetof(struct nlist32,n_value);
  nrel.bitoffset = 0;
  nrel.size = bits;
  nrel.mask = ~0;
  nrel.addend = nlist->value;
  nrel.sym = nlist->base;

  rl->next = NULL;
  rl->reloc = &nrel;
  rl->type = REL_ABS;

  return make_relocs(rl,pc,newsym,addrel);
}


/* create .rel(a)XXX section header */
static void make_relsechdr(char *sname,utaddr roffs,utaddr len,unsigned idx,
                           void *(*makeshdr)(elfull,elfull,elfull,elfull,
                                             elfull,elfull,elfull,elfull))
{
  char *rname = mymalloc(strlen(sname) + 6);
 
  if (RELA)
    sprintf(rname,".rela%s",sname);
  else
    sprintf(rname,".rel%s",sname);

  makeshdr(addString(&shstrlist,rname),shtreloc,0,
           roffs, /* relative offset - will be fixed later! */
           len,idx,bytespertaddr,elfrelsize);
}


static utaddr prog_sec_hdrs(section *sec,utaddr soffset,
                            void *(*makeshdr)(elfull,elfull,elfull,elfull,
                                              elfull,elfull,elfull,elfull),
                            void (*newsym)(char *,elfull,elfull,
                                           uint8_t,uint8_t,
                                           unsigned))
{
  section *secp;
  struct stabdef *nlist = first_nlist;

  /* generate section headers for program sections */
  for (secp=sec; secp; secp=secp->next) {
    if (keep_empty_sects ||
        get_sec_size(secp)!=0 || (secp->flags & HAS_SYMBOLS)) {
      uint32_t type = elf_sec_type(secp);

      /* add section base symbol */
      newsym(NULL,0,0,STB_LOCAL,STT_SECTION,shdrindex);

      secp->idx = shdrindex;
      makeshdr(addString(&shstrlist,secp->name),
               type,get_sec_flags(secp->attr),soffset,
               get_sec_size(secp),0,secp->align,0);

      if (type != SHT_NOBITS)
        soffset += get_sec_size(secp);
    }
    else
      secp->idx = 0;
  }

  /* look for stabs (32 bits only) */
  if (!no_symbols && bits==32 && nlist!=NULL) {
    struct Shdr32Node *shn;
    char *cuname = NULL;

    /* count them, set name of compilation unit */
    while (nlist != NULL) {
      stablen++;
      if (nlist->type==N_SO && nlist->name.ptr!=NULL && *nlist->name.ptr!='\0')
        cuname = nlist->name.ptr;
      nlist = nlist->next;
    }
    /* add all symbol strings to .stabstr, cu name should be first(?) */
    addString(&stabstrlist,cuname!=NULL?cuname:filename);
    nlist = first_nlist;
    while (nlist != NULL) {
      nlist->name.idx = nlist->name.ptr != NULL ?
                        addString(&stabstrlist,nlist->name.ptr) : 0;
      nlist = nlist->next;
    }
    /* make .stab section, preceded by a compilation unit header (stablen+1) */
    stabidx = shdrindex;
    shn = makeshdr(addString(&shstrlist,stabname),SHT_PROGBITS,0,soffset,
                   (stablen+1)*sizeof(struct nlist32),0,4,
                   sizeof(struct nlist32));
    soffset += (stablen+1) * sizeof(struct nlist32);
    setval(be,shn->s.sh_link,4,shdrindex);  /* associated .stabstr section */
    /* make .stabstr section */
    makeshdr(addString(&shstrlist,".stabstr"),SHT_STRTAB,0,soffset,
             stabstrlist.index,0,1,0);
    soffset += stabstrlist.index;
    stabstralign = balign(soffset,4);
    soffset += stabstralign;
  }

  return soffset;
}


static unsigned build_symbol_table(symbol *first,
                                   void (*newsym)(char *,elfull,elfull,
                                                  uint8_t,uint8_t,
                                                  unsigned))
{
  symbol *symp;
  unsigned firstglobal;

  /* file name symbol, when defined */
  if (filename)
    newsym(filename,0,0,STB_LOCAL,STT_FILE,SHN_ABS);

  if (!no_symbols)  /* symbols with local binding first */
    for (symp=first; symp; symp=symp->next)
      if (*symp->name!='.' && *symp->name!=' ' && !(symp->flags&VASMINTERN))
        if (symp->type!=IMPORT && !(symp->flags & (EXPORT|WEAK)))
          newsym(symp->name,get_sym_value(symp),get_sym_size(symp),
                 STB_LOCAL,get_sym_info(symp),get_sym_index(symp));

  firstglobal = symindex;  /* now the global and weak symbols */

  for (symp=first; symp; symp=symp->next)
    if (*symp->name != '.'  && !(symp->flags&VASMINTERN))
      if ((symp->type!=IMPORT && (symp->flags & (EXPORT|WEAK))) ||
          (symp->type==IMPORT && (symp->flags & (COMMON|WEAK))))
        newsym(symp->name,get_sym_value(symp),get_sym_size(symp),
               (symp->flags & WEAK) ? STB_WEAK : STB_GLOBAL,
               get_sym_info(symp),get_sym_index(symp));

  return firstglobal;
}


static void make_reloc_sections(section *sec,
                                void (*newsym)(char *,elfull,elfull,
                                               uint8_t,uint8_t,
                                               unsigned),
                                void (*addrel)(elfull,elfull,elfull,elfull),
                                void *(*makeshdr)(elfull,elfull,elfull,elfull,
                                                  elfull,elfull,elfull,elfull))
{
  struct stabdef *nlist = first_nlist;
  utaddr roffset = 0;
  utaddr basero,pc;
  section *secp;

  /* ".rela.xxx" or ".rel.xxx" relocation sections */
  for (secp=sec; secp; secp=secp->next) {
    if (secp->idx) {
      atom *a;
      utaddr npc;

      for (a=secp->first,basero=roffset,pc=0; a; a=a->next) {
        npc = pcalign(a,pc);
        if (a->type == DATA)
          roffset += make_relocs(a->content.db->relocs,npc,newsym,addrel);
        if (a->type == SPACE)
          roffset += make_relocs(a->content.sb->relocs,npc,newsym,addrel);
        pc = npc + atom_size(a,secp,npc);
      }

      if (basero != roffset)  /* were there any relocations? */
        make_relsechdr(secp->name,basero,roffset-basero,secp->idx,makeshdr);
    }
  }

  if (!no_symbols) {
    /* look for relocations in .stab */
    basero = roffset;
    pc = sizeof(struct nlist32);  /* skip compilation unit header */
    while (nlist != NULL) {
      if (nlist->base != NULL)
        roffset += make_stabreloc(pc,nlist,newsym,addrel);
      nlist = nlist->next;
      pc += sizeof(struct nlist32);
    }
    /* create .rel(a).stab when needed */
    if (basero != roffset)
      make_relsechdr(stabname,basero,roffset-basero,stabidx,makeshdr);
  }
}


static void write_strtab(FILE *f,struct StrTabList *strl)
{
  struct StrTabNode *stn;

  while (stn = (struct StrTabNode *)remhead(&(strl->l)))
    fwdata(f,stn->str,strlen(stn->str)+1);
}


static void write_section_data(FILE *f,section *sec)
{
  struct stabdef *nlist = first_nlist;
  section *secp;

  for (secp=sec; secp; secp=secp->next) {
    if (secp->idx && elf_sec_type(secp)!=SHT_NOBITS) {
      atom *a;
      utaddr pc=0,npc;

      for (a=secp->first; a; a=a->next) {
        npc = fwpcalign(f,a,secp,pc);

        if (a->type == DATA) {
          fwdata(f,a->content.db->data,a->content.db->size);
        }
        else if (a->type == SPACE) {
          fwsblock(f,a->content.sb);
        }

        pc = npc + atom_size(a,secp,npc);
      }
    }
  }

  if (!no_symbols && nlist!=NULL) {
    /* write compilation unit header - precedes nlist entries */
    fw32(f,1,be);  /* source name is first entry in .stabstr */
    fw32(f,stablen,be);
    fw32(f,stabstrlist.index,be);
    /* write .stab */
    while (nlist != NULL) {
      struct nlist32 n;

      setval(be,&n.n_strx,4,nlist->name.idx);
      n.n_type = nlist->type;
      n.n_other = nlist->other;
      setval(be,&n.n_desc,2,nlist->desc);
      setval(be,&n.n_value,4,nlist->value);
      fwdata(f,&n,sizeof(struct nlist32));
      nlist = nlist->next;
    }
    /* write .stabstr and align */
    write_strtab(f,&stabstrlist);
    fwspace(f,stabstralign);
  }
}


static void write_ELF64(FILE *f,section *sec,symbol *sym)
{
  struct Elf64_Ehdr header;
  unsigned firstglobal,align1,align2,i;
  utaddr soffset=sizeof(struct Elf64_Ehdr);
  struct Shdr64Node *shn;
  struct Symbol64Node *elfsym;

  elfrelsize = RELA ? sizeof(struct Elf64_Rela) : sizeof(struct Elf64_Rel);

  /* initialize ELF header */
  memset(&header,0,sizeof(struct Elf64_Ehdr));
  init_ident(header.e_ident,ELFCLASS64);
  setval(be,header.e_type,2,ET_REL);
  setval(be,header.e_machine,2,cpu);
  setval(be,header.e_version,4,EV_CURRENT);
  setval(be,header.e_ehsize,2,sizeof(struct Elf64_Ehdr));
  setval(be,header.e_shentsize,2,sizeof(struct Elf64_Shdr));

  init_lists();
  addShdr64();        /* first section header is always zero */
  addSymbol64(NULL);  /* first symbol is empty */

  /* make program section headers, symbols and relocations */
  soffset = prog_sec_hdrs(sec,soffset,makeShdr64,newSym64);
  firstglobal = build_symbol_table(sym,newSym64);
  make_reloc_sections(sec,newSym64,addRel64,makeShdr64);

  /* ".shstrtab" section header string table */
  makeShdr64(shstrtabidx,SHT_STRTAB,0,
             soffset,shstrlist.index,0,1,0);
  soffset += shstrlist.index;
  align1 = balign(soffset,4);
  soffset += align1;

  /* set last values in ELF header */
  setval(be,header.e_shoff,8,soffset);  /* remember offset of Shdr table */
  soffset += (shdrindex+2)*sizeof(struct Elf64_Shdr);
  setval(be,header.e_shstrndx,2,shdrindex-1);
  setval(be,header.e_shnum,2,shdrindex+2);

  /* ".symtab" symbol table */
  shn = makeShdr64(symtabidx,SHT_SYMTAB,0,soffset,
                   symindex*sizeof(struct Elf64_Sym),
                   firstglobal,8,sizeof(struct Elf64_Sym));
  setval(be,shn->s.sh_link,4,shdrindex);  /* associated .strtab section */
  soffset += symindex * sizeof(struct Elf64_Sym);

  /* ".strtab" string table */
  makeShdr64(strtabidx,SHT_STRTAB,0,soffset,strlist.index,0,1,0);
  soffset += strlist.index;
  align2 = balign(soffset,4);
  soffset += align2;  /* offset for first Reloc-entry */

  /* write ELF header */
  fwdata(f,&header,sizeof(struct Elf64_Ehdr));

  /* write initialized section contents */
  write_section_data(f,sec);

  /* write .shstrtab string table */
  write_strtab(f,&shstrlist);

  /* write section headers */
  fwspace(f,align1);
  i = 0;
  while (shn = (struct Shdr64Node *)remhead(&shdrlist)) {
    if (readval(be,shn->s.sh_type,4) == shtreloc) {
      /* set correct offset and link to symtab */
      setval(be,shn->s.sh_offset,8,readval(be,shn->s.sh_offset,8)+soffset);
      setval(be,shn->s.sh_link,4,shdrindex-2); /* index of associated symtab */
    }
    fwdata(f,&(shn->s),sizeof(struct Elf64_Shdr));
    i++;
  }

  /* write symbol table */
  while (elfsym = (struct Symbol64Node *)remhead(&symlist))
    fwdata(f,&(elfsym->s),sizeof(struct Elf64_Sym));

  /* write .strtab string table */
  write_strtab(f,&strlist);

  /* write relocations */
  fwspace(f,align2);
  if (RELA) {
    struct Rela64Node *rn;

    while (rn = (struct Rela64Node *)remhead(&relalist))
      fwdata(f,&(rn->r),sizeof(struct Elf64_Rela));
  }
  else {
    struct Rel64Node *rn;

    while (rn = (struct Rel64Node *)remhead(&relalist))
      fwdata(f,&(rn->r),sizeof(struct Elf64_Rel));
  }
}


static void write_ELF32(FILE *f,section *sec,symbol *sym)
{
  struct Elf32_Ehdr header;
  unsigned firstglobal,align1,align2,i;
  utaddr soffset=sizeof(struct Elf32_Ehdr);
  struct Shdr32Node *shn;
  struct Symbol32Node *elfsym;

  elfrelsize = RELA ? sizeof(struct Elf32_Rela) : sizeof(struct Elf32_Rel);

  /* initialize ELF header */
  memset(&header,0,sizeof(struct Elf32_Ehdr));
  init_ident(header.e_ident,ELFCLASS32);
  setval(be,header.e_type,2,ET_REL);
  setval(be,header.e_machine,2,cpu);
  setval(be,header.e_version,4,EV_CURRENT);
#ifdef VASM_CPU_ARM
  setval(be,header.e_flags,4,0x04000000);  /* EABI version 4 */
#endif
  setval(be,header.e_ehsize,2,sizeof(struct Elf32_Ehdr));
  setval(be,header.e_shentsize,2,sizeof(struct Elf32_Shdr));

  init_lists();
  addShdr32();        /* first section header is always zero */
  addSymbol32(NULL);  /* first symbol is empty */

  /* make program section headers, symbols and relocations */
  soffset = prog_sec_hdrs(sec,soffset,makeShdr32,newSym32);
  firstglobal = build_symbol_table(sym,newSym32);
  make_reloc_sections(sec,newSym32,addRel32,makeShdr32);

  /* ".shstrtab" section header string table */
  makeShdr32(shstrtabidx,SHT_STRTAB,0,
             soffset,shstrlist.index,0,1,0);
  soffset += shstrlist.index;
  align1 = balign(soffset,4);
  soffset += align1;

  /* set last values in ELF header */
  setval(be,header.e_shoff,4,soffset);  /* remember offset of Shdr table */
  soffset += (shdrindex+2)*sizeof(struct Elf32_Shdr);
  setval(be,header.e_shstrndx,2,shdrindex-1);
  setval(be,header.e_shnum,2,shdrindex+2);

  /* ".symtab" symbol table */
  shn = makeShdr32(symtabidx,SHT_SYMTAB,0,soffset,
                   symindex*sizeof(struct Elf32_Sym),
                   firstglobal,4,sizeof(struct Elf32_Sym));
  setval(be,shn->s.sh_link,4,shdrindex);  /* associated .strtab section */
  soffset += symindex * sizeof(struct Elf32_Sym);

  /* ".strtab" string table */
  makeShdr32(strtabidx,SHT_STRTAB,0,soffset,strlist.index,0,1,0);
  soffset += strlist.index;
  align2 = balign(soffset,4);
  soffset += align2;  /* offset for first Reloc-entry */

  /* write ELF header */
  fwdata(f,&header,sizeof(struct Elf32_Ehdr));

  /* write initialized section contents */
  write_section_data(f,sec);

  /* write .shstrtab string table */
  write_strtab(f,&shstrlist);

  /* write section headers */
  fwspace(f,align1);
  i = 0;
  while (shn = (struct Shdr32Node *)remhead(&shdrlist)) {
    if (readval(be,shn->s.sh_type,4) == shtreloc) {
      /* set correct offset and link to symtab */
      setval(be,shn->s.sh_offset,4,readval(be,shn->s.sh_offset,4)+soffset);
      setval(be,shn->s.sh_link,4,shdrindex-2); /* index of associated symtab */
    }
    fwdata(f,&(shn->s),sizeof(struct Elf32_Shdr));
    i++;
  }

  /* write symbol table */
  while (elfsym = (struct Symbol32Node *)remhead(&symlist))
    fwdata(f,&(elfsym->s),sizeof(struct Elf32_Sym));

  /* write .strtab string table */
  write_strtab(f,&strlist);

  /* write relocations */
  fwspace(f,align2);
  if (RELA) {
    struct Rela32Node *rn;

    while (rn = (struct Rela32Node *)remhead(&relalist))
      fwdata(f,&(rn->r),sizeof(struct Elf32_Rela));
  }
  else {
    struct Rel32Node *rn;

    while (rn = (struct Rel32Node *)remhead(&relalist))
      fwdata(f,&(rn->r),sizeof(struct Elf32_Rel));
  }
}


static void write_output(FILE *f,section *sec,symbol *sym)
{
  cpu = ELFCPU;    /* cpu ID */
  be = BIGENDIAN;  /* true for big endian */
  bits = bytespertaddr * bitsperbyte;
  shtreloc = RELA ? SHT_RELA : SHT_REL;

  if (bits==32 && cpu!=EM_NONE)
    write_ELF32(f,sec,sym);
  else if (bits==64 && cpu!=EM_NONE)
    write_ELF64(f,sec,sym);
  else
    output_error(1,cpuname);  /* output module doesn't support cpu */

  if (debug && elfsymhash->collisions)
    printf("*** %d ELF symbol collisions!\n",elfsymhash->collisions);
}


static int output_args(char *p)
{
  if (!strcmp(p,"-keepempty")) {
    keep_empty_sects = 1;
    return 1;
  }
  return 0;
}


int init_output_elf(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  *cp = copyright;
  *wo = write_output;
  *oa = output_args;
  return 1;
}

#else

int init_output_elf(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  return 0;
}

#endif
