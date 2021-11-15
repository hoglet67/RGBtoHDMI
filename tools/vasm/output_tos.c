/* output_tos.c Atari TOS executable output driver for vasm */
/* (c) in 2009-2016,2020,2021 by Frank Wille */

#include "vasm.h"
#include "output_tos.h"
#if defined(OUTTOS) && defined(VASM_CPU_M68K)
static char *copyright="vasm tos output module 1.2 (c) 2009-2016,2020,2021 Frank Wille";
int tos_hisoft_dri = 1;

static int tosflags,textbasedsyms;
static int max_relocs_per_atom;
static section *sections[3];
static utaddr secsize[3];
static utaddr secoffs[3];
static utaddr sdabase,lastoffs;

#define SECT_ALIGN 2  /* TOS sections have to be aligned to 16 bits */


static int tos_initwrite(section *sec,symbol *sym)
{
  int nsyms = 0;
  int i;

  /* find exactly one .text, .data and .bss section for a.out */
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

  max_relocs_per_atom = 1;
  secoffs[S_TEXT] = 0;
  secoffs[S_DATA] = secsize[S_TEXT] + balign(secsize[S_TEXT],SECT_ALIGN);
  secoffs[S_BSS] = secoffs[S_DATA] + secsize[S_DATA] +
                  balign(secsize[S_DATA],SECT_ALIGN);
  /* define small data base as .data+32768 @@@FIXME! */
  sdabase = secoffs[S_DATA] + 0x8000;

  /* count symbols */
  for (; sym; sym=sym->next) {
    /* ignore symbols preceded by a '.' and internal symbols */
    if (*sym->name!='.' && *sym->name!=' ') {
      if (!(sym->flags & (VASMINTERN|COMMON)) && sym->type == LABSYM) {
        nsyms++;
        if ((strlen(sym->name) > DRI_NAMELEN) && tos_hisoft_dri)
          nsyms++;  /* extra symbol for long name */
      }
    }
    else {
      if (!strcmp(sym->name," TOSFLAGS")) {
        if (tosflags == 0)  /* not defined by command line? */
          tosflags = (int)get_sym_value(sym);
      }
      sym->flags |= VASMINTERN;
    }
  }
  return no_symbols ? 0 : nsyms;
}


static void tos_header(FILE *f,unsigned long tsize,unsigned long dsize,
                        unsigned long bsize,unsigned long ssize,
                        unsigned long flags)
{
  PH hdr;

  setval(1,hdr.ph_branch,2,0x601a);
  setval(1,hdr.ph_tlen,4,tsize);
  setval(1,hdr.ph_dlen,4,dsize);
  setval(1,hdr.ph_blen,4,bsize);
  setval(1,hdr.ph_slen,4,ssize);
  setval(1,hdr.ph_magic,4,0);
  setval(1,hdr.ph_flags,4,flags);
  setval(1,hdr.ph_abs,2,0);
  fwdata(f,&hdr,sizeof(PH));
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


static taddr tos_sym_value(symbol *sym,int textbased)
{
  taddr val = get_sym_value(sym);

  /* all sections form a contiguous block, so add section offset */
  if (textbased && sym->type==LABSYM && sym->sec!=NULL)
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
                     (tos_sym_value(((nreloc *)rl->reloc)->sym,1)
                      + nreloc_real_addend(rl->reloc)) - sdabase,1);
        break;
      case REL_PC:
        checkdefined(rl,asec,pc,a);
        patch_nreloc(a,rl,1,
                     (tos_sym_value(((nreloc *)rl->reloc)->sym,1)
                     + nreloc_real_addend(rl->reloc)) -
                     (pc + ((nreloc *)rl->reloc)->byteoffset),1);
        break;
      case REL_ABS:
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
    rcnt++;
    if (a->type == SPACE)
      break;  /* all SPACE relocs are identical, one is enough */
    rl = rl->next;
  }

  if (rcnt > max_relocs_per_atom)
    max_relocs_per_atom = rcnt;
}


static void tos_writesection(FILE *f,section *sec,taddr sec_align)
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


static void write_dri_sym(FILE *f,char *name,int type,taddr value)
{
  struct DRIsym stab;
  int longname = (strlen(name) > DRI_NAMELEN) && tos_hisoft_dri;

  strncpy(stab.name,name,DRI_NAMELEN);
  setval(1,stab.type,sizeof(stab.type),longname?(type|STYP_LONGNAME):type);
  setval(1,stab.value,sizeof(stab.value),value);
  fwdata(f,&stab,sizeof(struct DRIsym));

  if (longname) {
    char rest_of_name[sizeof(struct DRIsym)];

    memset(rest_of_name,0,sizeof(struct DRIsym));
    strncpy(rest_of_name,name+DRI_NAMELEN,sizeof(struct DRIsym));
    fwdata(f,rest_of_name,sizeof(struct DRIsym));
  }
}


static void tos_symboltable(FILE *f,symbol *sym)
{
  static int labtype[] = { STYP_TEXT,STYP_DATA,STYP_BSS };
  int t;

  for (; sym; sym=sym->next) {
    /* The Devpac DRI symbol table in executables contains all labels,
       no matter if global or local. But no equates or other types. */
    if (!(sym->flags & (VASMINTERN|COMMON)) && sym->type == LABSYM) {
      if (sym->flags & WEAK)
        output_error(10,sym->name);  /* weak symbol not supported */
      t = labtype[sym->sec->idx] | STYP_DEFINED | STYP_GLOBAL;
      write_dri_sym(f,sym->name,t,tos_sym_value(sym,textbasedsyms));
    }
  }
}


static int offscmp(const void *offs1,const void *offs2)
{
  return *(int *)offs1 - *(int *)offs2;
}


static int tos_writerelocs(FILE *f,section *sec)
{
  int n = 0;
  int *sortoffs = mymalloc(max_relocs_per_atom*sizeof(int));

  if (sec) {
    utaddr pc = secoffs[sec->idx];
    utaddr npc;
    atom *a;
    rlist *rl;

    for (a=sec->first; a; a=a->next) {
      int nrel=0;

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

        /* write differences between them */
        n += nrel;
        for (i=0; i<nrel; i++) {
          utaddr newoffs = npc + sortoffs[i];

          if (lastoffs) {
            /* determine 8bit difference to next relocation */
            taddr diff = newoffs - lastoffs;

            if (diff < 0)
              ierror(0);
            while (diff > 254) {
              fw8(f,1);
              diff -= 254;
            }
            fw8(f,(uint8_t)diff);
          }
          else  /* first entry is a 32 bits offset */
            fw32(f,newoffs,1);
          lastoffs = newoffs;
        }
      }
      pc = npc + atom_size(a,sec,npc);
    }
  }

  myfree(sortoffs);
  return n;
}


static void write_output(FILE *f,section *sec,symbol *sym)
{
  int nsyms = tos_initwrite(sec,sym);
  int nrelocs = 0;

  tos_header(f,secsize[S_TEXT],secsize[S_DATA],secsize[S_BSS],
             nsyms*sizeof(struct DRIsym),tosflags);
  tos_writesection(f,sections[S_TEXT],SECT_ALIGN);
  tos_writesection(f,sections[S_DATA],SECT_ALIGN);
  if (nsyms)
    tos_symboltable(f,sym);
  nrelocs += tos_writerelocs(f,sections[S_TEXT]);
  nrelocs += tos_writerelocs(f,sections[S_DATA]);
  if (nrelocs)
    fw8(f,0);
  else
    fw32(f,0,1);
}


static int output_args(char *p)
{
  if (!strncmp(p,"-tos-flags=",11)) {
    tosflags = atoi(p+11);
    return 1;
  }
  else if (!strcmp(p,"-monst")) {
    textbasedsyms = 1;
    return 1;
  }
  return 0;
}


int init_output_tos(char **cp,void (**wo)(FILE *,section *,symbol *),
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

int init_output_tos(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  return 0;
}
#endif
