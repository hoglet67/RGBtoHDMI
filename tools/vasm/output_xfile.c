/* output_xfile.c Sharp X68000 Xfile output driver for vasm */
/* (c) in 2018,2020,2021 by Frank Wille */

#include "vasm.h"
#include "output_xfile.h"
#if defined(OUTXFIL) && defined(VASM_CPU_M68K)
static char *copyright="vasm xfile output module 0.3 (c) 2018,2020,2021 Frank Wille";

static int max_relocs_per_atom;
static section *sections[3];
static utaddr secsize[3];
static utaddr secoffs[3];
static utaddr sdabase,lastoffs;

#define SECT_ALIGN 2  /* Xfile sections have to be aligned to 16 bits */


static void xfile_initwrite(section *sec,symbol *sym)
{
  int i;

  /* find exactly one .text, .data and .bss section for xfile */
  sections[S_TEXT] = sections[S_DATA] = sections[S_BSS] = NULL;
  secsize[S_TEXT] = secsize[S_DATA] = secsize[S_BSS] = 0;

  for (; sec; sec=sec->next) {
    /* section size is assumed to be in in (sec->pc - sec->org), otherwise
       we would have to calculate it from the atoms and store it there */
    if ((sec->pc - sec->org) > 0 || (sec->flags & HAS_SYMBOLS)) {
      i = get_sec_type(sec);
      if (i<S_TEXT || i>S_BSS) {
        output_error(3,sec->attr);  /* section attributes not supported */
        i = S_TEXT;
      }
      if (!sections[i]) {
        sections[i] = sec;
        secsize[i] = (get_sec_size(sec) + SECT_ALIGN - 1) /
                     SECT_ALIGN * SECT_ALIGN;
        sec->idx = i;  /* section index 0:text, 1:data, 2:bss */
      }
      else
        output_error(7,sec->name);
    }
  }

  secoffs[S_TEXT] = 0;
  secoffs[S_DATA] = secsize[S_TEXT] + balign(secsize[S_TEXT],SECT_ALIGN);
  secoffs[S_BSS] = secoffs[S_DATA] + secsize[S_DATA] +
                  balign(secsize[S_DATA],SECT_ALIGN);
  /* define small data base as .data+32768 @@@FIXME! */
  sdabase = secoffs[S_DATA] + 0x8000;
}


static void xfile_header(FILE *f,unsigned long tsize,unsigned long dsize,
                         unsigned long bsize)
{
  XFILE hdr;

  memset(&hdr,0,sizeof(XFILE));
  setval(1,hdr.x_id,2,0x4855);  /* "HU" ID for Human68k */
  setval(1,hdr.x_textsz,4,tsize);
  setval(1,hdr.x_datasz,4,dsize);
  setval(1,hdr.x_heapsz,4,bsize);
  /* x_relocsz and x_syminfsz will be patched later */
  fwdata(f,&hdr,sizeof(XFILE));
}


static void checkdefined(rlist *rl,section *sec,taddr pc,atom *a)
{
  if (rl->type <= LAST_STANDARD_RELOC) {
    nreloc *r = (nreloc *)rl->reloc;

    if (EXTREF(r->sym))
      output_atom_error(8,a,r->sym->name,sec->name,
                        (unsigned long)pc+r->byteoffset,rl->type);
  }
  else
    ierror(0);
}


static taddr xfile_sym_value(symbol *sym)
{
  taddr val = get_sym_value(sym);

  /* all sections form a contiguous block, so add section offset */
  if (sym->type==LABSYM && sym->sec!=NULL)
    val += secoffs[sym->sec->idx];

  return val;
}


static void do_relocs(section *asec,taddr pc,atom *a)
/* Try to resolve all relocations in a DATA or SPACE atom.
   Very simple implementation which can only handle basic 68k relocs. */
{
  int rcnt = 0;
  section *sec;
  rlist *rl;

  if (a->type == DATA)
    rl = a->content.db->relocs;
  else if (a->type == SPACE)
    rl = a->content.sb->relocs;
  else
    rl = NULL;

  while (rl) {
    switch (rl->type) {
      case REL_SD:
        checkdefined(rl,asec,pc,a);
        patch_nreloc(a,rl,1,
                     (xfile_sym_value(((nreloc *)rl->reloc)->sym)
                      + nreloc_real_addend(rl->reloc)) - sdabase,1);
        break;
      case REL_PC:
        checkdefined(rl,asec,pc,a);
        patch_nreloc(a,rl,1,
                     (xfile_sym_value(((nreloc *)rl->reloc)->sym)
                      + nreloc_real_addend(rl->reloc)) -
                     (pc + ((nreloc *)rl->reloc)->byteoffset),1);
        break;
      case REL_ABS:
        rcnt++;
        checkdefined(rl,asec,pc,a);
        sec = ((nreloc *)rl->reloc)->sym->sec;
        if (!patch_nreloc(a,rl,0,
                          secoffs[sec?sec->idx:0] +
                          ((nreloc *)rl->reloc)->addend,1))
          break;  /* field overflow */
        if (((nreloc *)rl->reloc)->size == 32)
          break;  /* only support 32-bit absolute */
      default:
        unsupp_reloc_error(rl);
        break;
    }
    if (a->type == SPACE)
      break;  /* all SPACE relocs are identical, one is enough */
    rl = rl->next;
  }

  if (rcnt > max_relocs_per_atom)
    max_relocs_per_atom = rcnt;
}


static void xfile_writesection(FILE *f,section *sec,taddr sec_align)
{
  if (sec) {
    utaddr pc = secoffs[sec->idx];
    utaddr npc;
    atom *a;

    for (a=sec->first; a; a=a->next) {
      npc = fwpcalign(f,a,sec,pc);
      do_relocs(sec,npc,a);
      if (a->type == DATA)
        fwdata(f,a->content.db->data,a->content.db->size);
      else if (a->type == SPACE)
        fwsblock(f,a->content.sb);
      pc = npc + atom_size(a,sec,npc);
    }
    fwalign(f,pc,sec_align);
  }
}


static size_t xfile_symboltable(FILE *f,symbol *sym)
/* The symbol table only contains expression (absolute) and label symbols,
   but doesn't differentiate between local and global scope. */
{
  static int labtype[] = { XSYM_TEXT,XSYM_DATA,XSYM_BSS };
  size_t len = 0;
  char *p;

  for (; sym; sym=sym->next) {
    if (!(sym->flags & VASMINTERN)
        && (sym->type==LABSYM || sym->type==EXPRESSION)) {

      if (sym->flags & WEAK)
        output_error(10,sym->name);  /* weak symbol not supported */

      /* symbol type */
      if (sym->type == LABSYM)
        fw16(f,labtype[sym->sec->idx],1);
      else /* EXPRESSION */
      	fw16(f,XSYM_ABS,1);

      /* symbol value */
      fw32(f,xfile_sym_value(sym),1);
      len += 6;  /* 2 bytes type, 4 bytes value */

      /* symbol name - 16-bit aligned */
      p = sym->name;
      do {
        fw8(f,*p);
        len++;
      } while (*p++);
      if (len & 1) {
        fw8(f,0);
        len++;
      }
    }
  }
  return len;
}


static int offscmp(const void *offs1,const void *offs2)
{
  return *(int *)offs1 - *(int *)offs2;
}


static size_t xfile_writerelocs(FILE *f,section *sec)
{
  int *sortoffs = mymalloc(max_relocs_per_atom*sizeof(int));
  size_t sz = 0;

  if (sec) {
    utaddr pc = secoffs[sec->idx];
    utaddr npc;
    atom *a;
    rlist *rl;

    for (a=sec->first; a; a=a->next) {
      int nrel = 0;

      npc = pcalign(a,pc);

      if (a->type == DATA)
        rl = a->content.db->relocs;
      else if (a->type == SPACE)
        rl = a->content.sb->relocs;
      else
        rl = NULL;

      while (rl) {
        if (rl->type==REL_ABS && ((nreloc *)rl->reloc)->size==32)
          sortoffs[nrel++] = ((nreloc *)rl->reloc)->byteoffset;
        rl = rl->next;
      }

      if (nrel) {
        int i;

        /* first sort the atom's relocs */
        if (nrel > 1)
          qsort(sortoffs,nrel,sizeof(int),offscmp);

        /* write distances in bytes between them */
        for (i=0; i<nrel; i++) {
          /* determine 16bit distance to next relocation */
          utaddr newoffs = npc + sortoffs[i];
          taddr diff = newoffs - lastoffs;

          if (diff < 0) {
            ierror(0);
          }
          else if (diff > 0xffff) {
            /* write a large distance >= 64K */
            fw16(f,1,1);
            fw32(f,diff,1);
            sz += 6;
          }
          else {
            fw16(f,diff,1);
            sz += 2;
          }
          lastoffs = newoffs;
        }
      }
      pc = npc + atom_size(a,sec,npc);
    }
  }

  myfree(sortoffs);
  return sz;
}


static void write_output(FILE *f,section *sec,symbol *sym)
{
  size_t relocsz,syminfsz;

  xfile_initwrite(sec,sym);
  max_relocs_per_atom = 1;

  xfile_header(f,secsize[S_TEXT],secsize[S_DATA],secsize[S_BSS]);
  xfile_writesection(f,sections[S_TEXT],SECT_ALIGN);
  xfile_writesection(f,sections[S_DATA],SECT_ALIGN);
  relocsz = xfile_writerelocs(f,sections[S_TEXT]);
  relocsz += xfile_writerelocs(f,sections[S_DATA]);
  syminfsz = no_symbols ? 0 : xfile_symboltable(f,sym);

  /* finally patch reloc- and symbol-table size into the header */
  fseek(f,offsetof(XFILE,x_relocsz),SEEK_SET);
  fw32(f,relocsz,1);
  fseek(f,offsetof(XFILE,x_syminfsz),SEEK_SET);
  fw32(f,syminfsz,1);
}


static int output_args(char *p)
{
#if 0
  if (!strncmp(p,"-startoffs=",11)) {
    startoffs = atoi(p+11);
    return 1;
  }
#endif
  return 0;
}


int init_output_xfile(char **cp,void (**wo)(FILE *,section *,symbol *),
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

int init_output_xfile(char **cp,void (**wo)(FILE *,section *,symbol *),
                      int (**oa)(char *))
{
  return 0;
}
#endif
