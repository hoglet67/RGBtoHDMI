/* reloc.c - relocation support functions */
/* (c) in 2010-2016,2020 by Volker Barthelmann and Frank Wille */

#include "vasm.h"


nreloc *new_nreloc(void)
{
  nreloc *new = mymalloc(sizeof(*new));
  new->mask = -1;
  new->byteoffset = new->bitoffset = new->size = 0;
  new->addend = 0;
  return new;
}


rlist *add_extnreloc(rlist **relocs,symbol *sym,taddr addend,int type,
                     size_t bitoffs,size_t size,size_t byteoffs)
/* add_extnreloc() can specify byteoffset and bitoffset directly.
   Use add_nreloc() for the old interface, which calculates
   byteoffset and bitoffset from offset. */
{
  rlist *rl;
  nreloc *r;

  if (sym->flags & ABSLABEL)
    return NULL;  /* no relocation, when symbol is from an ORG-section */

  /* mark symbol as referenced, so we can find unreferenced imported symbols */
  sym->flags |= REFERENCED;

  r = new_nreloc();
  r->byteoffset = byteoffs;
  r->bitoffset = bitoffs;
  r->size = size;
  r->sym = sym;
  r->addend = addend;
  rl = mymalloc(sizeof(rlist));
  rl->type = type;
  rl->reloc = r;
  rl->next = *relocs;
  *relocs = rl;

  return rl;
}


rlist *add_extnreloc_masked(rlist **relocs,symbol *sym,taddr addend,int type,
                            size_t bitoffs,size_t size,size_t byteoffs,
                            taddr mask)
/* add_extnreloc_masked() can specify byteoffset and bitoffset directly.
   Use add_nreloc_masked() for the old interface, which calculates
   byteoffset and bitoffset from offset. */
{
  rlist *rl;
  nreloc *r;

  if (rl = add_extnreloc(relocs,sym,addend,type,bitoffs,size,byteoffs)) {
    r = rl->reloc;
    r->mask = mask;
  }
  return rl;
}


int is_pc_reloc(symbol *sym,section *cur_sec)
/* Returns true when the symbol needs a REL_PC relocation to reference it
   pc-relative. This is the case when it is externally defined or a label
   from a different section (so the linker has to resolve the distance). */
{
  if (EXTREF(sym))
    return 1;
  else if (LOCREF(sym))
    return (cur_sec->flags & ABSOLUTE) ? 0 : sym->sec != cur_sec;
  ierror(0);
  return 0;
}


void do_pic_check(rlist *rl)
/* generate an error on a non-PC-relative relocation */
{
  nreloc *r;
  int t;

  while (rl) {
    t = rl->type;
    if (t==REL_ABS || t==REL_UABS) {
      r = (nreloc *)rl->reloc;

      if (r->sym->type==LABSYM ||
          (r->sym->type==IMPORT && (r->sym->flags&EXPORT)))
        general_error(34);  /* relocation not allowed */
    }
    rl = rl->next;
  }
}


taddr nreloc_real_addend(nreloc *nrel)
{
  /* In vasm the addend includes the symbol's section offset for LABSYMs */
  if (nrel->sym->type == LABSYM)
    return nrel->addend - nrel->sym->pc;
  return nrel->addend;
}


void unsupp_reloc_error(rlist *rl)
{
  if (rl->type <= LAST_STANDARD_RELOC) {
    nreloc *r = (nreloc *)rl->reloc;

    /* reloc type not supported */
    output_error(4,rl->type,r->size,(unsigned long)r->mask,
                 r->sym->name,(unsigned long)r->addend);
  }
  else
    output_error(5,rl->type);
}


void print_reloc(FILE *f,int type,nreloc *p)
{
  if (type<=LAST_STANDARD_RELOC){
    static const char *rname[] = {
      "none","abs","pc","got","gotrel","gotoff","globdat","plt","pltrel",
      "pltoff","sd","uabs","localpc","loadrel","copy","jmpslot","secoff"
    };
    fprintf(f,"r%s(%u,%u-%u,0x%llx,0x%llx,",rname[type],
            (unsigned)p->byteoffset,(unsigned)p->bitoffset,
            (unsigned)(p->bitoffset+p->size-1),
            ULLTADDR(p->mask),ULLTADDR(p->addend));
  }
#ifdef VASM_CPU_PPC
  else if (type<=LAST_PPC_RELOC){
    static const char *rname[] = {
      "sd2","sd21","sdi16","drel","brel"
    };
    fprintf(f,"r%s(%u,%u-%u,0x%llx,0x%llx,",
            rname[type-(LAST_STANDARD_RELOC+1)],
            (unsigned)p->byteoffset,(unsigned)p->bitoffset,
            (unsigned)(p->bitoffset+p->size-1),
            ULLTADDR(p->mask),ULLTADDR(p->addend));
  }
#endif
  else
    fprintf(f,"unknown reloc(");

  print_symbol(f,p->sym);
  fprintf(f,") ");
}
