/* output_o65.c Andre Fachat's o65 relocatable binary format for vasm */
/* (c) in 2021 by Frank Wille */

#include <time.h>
#include "vasm.h"

#if defined(OUTO65) && defined(VASM_CPU_650X)
static char *out_copyright="vasm o65 output module 0.1 (c) 2021 Frank Wille";

#define S_ZERO (S_BSS+1)
#define NSECS (S_ZERO+1)        /* o65 supports text, data, bss, zero */

/* o65 mode flags */
#define MO65_65816     (1<<15)
#define MO65_PAGED     (1<<14)
#define MO65_LONG      (1<<13)
#define MO65_OBJ       (1<<12)
#define MO65_SIMPLE    (1<<11)
#define MO65_CHAIN     (1<<10)
#define MO65_BSSZERO   (1<<9)

struct o65Import {
  struct o65Import *next;
  char *name;
  int idx;
};

struct o65Reloc {
  struct o65Reloc *next;
  utaddr offset;
  uint8_t type;        /* o65 absolute relocation type */
  uint8_t segid;
  uint16_t import;     /* valid when segid=SEG_UND */
  taddr addend;
};

/* segid values */
#define SEG_UND 0      /* reference to external symbol */
#define SEG_ABS 1      /* reference to absolute symbol */
#define SEG_SEC 2      /* reference to text(2), data(3), bss(4), zero(5) */

static section *sections[NSECS];
static utaddr secsizes[NSECS];
static utaddr secorgs[NSECS];
static int org_is_set[NSECS];
static int o65size = 2; /* word size may be 2 bytes or 4 bytes */

static int secalign;    /* minimum alignment of all sections (LS zero-bits) */
static int stacksz;
static int paged;
static int bsszero;

static int fopts;
static const char *foauthor;
static const char *foname;

#define IMPHTABSIZE 0x100
static hashtable *importhash;
static struct o65Import *importlist;
static int num_imports;
static struct o65Reloc *relocs[S_DATA+1];  /* text and data relocs */


static int is_bss_name(const char *p)
{
  size_t len = strlen(p);

  while (len >= 3) {
    if (toupper((unsigned char)p[0]) == 'B' &&
        toupper((unsigned char)p[1]) == 'S' &&
        toupper((unsigned char)p[2]) == 'S')
      return 1;
    p++;
    len--;
  }
  return 0;

}


static void fwsize(FILE *f,unsigned w)
{
  if (o65size == 2)
    fw16(f,w,0);
  else if (o65size == 4)
    fw32(f,w,0);
  else
    ierror(0);
}


static void o65_initwrite(section *sec)
{
  utaddr addr;
  int i,valid;

  /* find exactly one .text, .data, .bss and .zero section */
  for (; sec; sec=sec->next) {
    /* section size is assumed to be in in (sec->pc - sec->org), otherwise
       we would have to calculate it from the atoms and store it there */
    if ((sec->pc - sec->org) > 0 || (sec->flags & HAS_SYMBOLS)) {
      i = get_sec_type(sec);

      if (i == S_ABS)
        i = (sections[S_TEXT]!=NULL&&sections[S_DATA]==NULL) ? S_DATA : S_TEXT;
      else if (i<S_TEXT || i>S_BSS) {
        output_error(3,sec->attr);  /* section attributes not supported */
        i = S_TEXT;
      }
      else if (i == S_BSS && !is_bss_name(sec->name))
        i = S_ZERO;

      if (!sections[i]) {
        sections[i] = sec;
        secsizes[i] = get_sec_size(sec);
        sec->idx = i;  /* section index 0:text, 1:data, 2:bss, 3:zero */
        while (sec->align > (taddr)1<<secalign)
          ++secalign;
      }
      else
        output_error(7,sec->name);
    }
  }

  /* calculate section start addresses */
  for (i=0,valid=0; i<NSECS; i++) {
    if (sec = sections[i]) {
      if (!(sec->flags & ABSOLUTE)) {
        if (!org_is_set[i]) {
          if (!valid) {
            addr = sec->org;
            valid = 1;
          }
          if (exec_out && sec->org==0) {
            /* sections will be consecutive in executables */
            sec->org = secorgs[i] = addr;
          }
        }
        else {
          /* start address set by option */
          addr = sec->org = secorgs[i];
          valid = 1;
        }
      }
      else {
        /* absolute code always has its fixed address */
        addr = secorgs[i] = sec->org;
        valid = 1;
      }
      addr += secsizes[i];
    }
  }
}


#ifdef VASM_CPU_650X
static uint16_t o65_cpu(void)
{
  uint16_t c;

  if (cpu_type & ILL)
    c = 4;  /* NMOS6502 including illegal opcodes */
  else if (cpu_type & HU6280)
    c = 1;  /* @@@ HU6280 bit does not exit - flag as C02 */
  else if (cpu_type & CSGCE02)
    c = 3;  /* 65CE02 */
  else if (cpu_type & WDC02)
    c = 2;  /* @@@ WDC02 has more instructions than SC02 */
  else if (cpu_type & M65C02)
    c = 1;  /* 65C02 */
  else
    c = 0;
  return c << 4;
}
#else  /* 65816 */
static uint16_t o65_cpu(void)
{
  return 0;
}
#endif


static int o65_simple(void)
{
  /* return true, when text, data, bss are consecutive */
  utaddr addr;
  int i;

  for (i=0; i<=S_BSS; i++) {
    if (sections[i] != NULL)
      break;
  }
  if (i <= S_BSS) {
    for (addr=secorgs[i]; i<=S_BSS; i++) {
      if (sections[i] != NULL) {
        if ((utaddr)secorgs[i] != addr)
          return 0;
        addr += secsizes[i];
      }
    }
  }
  return 1;
}


static void o65_fopt(FILE *f,int type,const char *data,size_t len)
{
  if (len <= 253) {
    fw8(f,len+2);
    fw8(f,type);
    fwdata(f,data,len);
  }
  else
    output_error(14,type,len+2);  /* max file option size exceeded */
}


static void o65_header(FILE *f)
{
  static const uint8_t o65magic[] = { 1,0,'o','6','5',0 };
  uint16_t mode;
  int i;

  /* magic id */
  fwdata(f,o65magic,sizeof(o65magic));

  /* mode */
  mode = secalign>2 ? 3 : (secalign&3);
  mode |= o65_cpu();
  if (paged)
    mode |= MO65_PAGED;
  if (exec_out) {
    if (bsszero)
      mode |= MO65_BSSZERO;
    if (o65_simple())
      mode |= MO65_SIMPLE;
  }
  else
    mode |= MO65_OBJ;
  fw16(f,mode,0);

  /* section origins and sizes */
  for (i=0; i<NSECS; i++) {
    fwsize(f,secorgs[i]);
    fwsize(f,secsizes[i]);
  }
  fwsize(f,stacksz);

  if (fopts) {
    /* file options */
    if ((fopts & 2) && foname==NULL)
      foname = (const char *)outname;

    if (foname)
      o65_fopt(f,0,foname,strlen(foname)+1);

    if (fopts & 2) {
      /* write assembler name and version */
      extern char *copyright;
      char *prod = cnvstr(copyright,strchr(copyright,'(')-copyright-1);

      o65_fopt(f,2,prod,strlen(prod)+1);
      myfree(prod);
    }

    if (foauthor)
      o65_fopt(f,3,foauthor,strlen(foauthor)+1);

    if (fopts & 2) {
      /* write creation date */
      char datebuf[32];
      time_t now;

      (void)time(&now);
      strftime(datebuf,32,"%a %b %d %T %Z %Y",localtime(&now));
      o65_fopt(f,4,datebuf,strlen(datebuf)+1);
    }
  }
  fw8(f,0);
}


static int o65_rtype(rlist *rl)
{
  if (rl->type == REL_ABS) {
    nreloc *r = (nreloc *)rl->reloc;

    if (r->bitoffset)
      return 0;

    if ((r->mask & 0xffffff) != 0xffffff && r->size==8) {
      if ((r->mask & 0xffffff) == 0xff0000)
        return 0xa0;  /* segment-byte of 24-bit address */
      else if ((r->mask & 0xffff) == 0xff00)
        return 0x40;  /* high-byte of 16-bit address */
      else if (r->mask == 0xff)
        return 0x20;  /* low-byte of 16-bit address */
    }
    else if (r->size == 8)
      return 0x20;  /* probably an 8-bit xref to an absolute value */
    else if (r->size == 16)
      return 0x80;  /* 16-bit address */
    else if (r->size == 24)
      return 0xc0;  /* 24-bit address */
  }
  return 0;
}


static void add_o65reloc(int secno,taddr offs,uint8_t type,uint8_t segid,
                         uint16_t import,taddr addend)
/* add to section's reloc-list, sorted by offset */
{
  struct o65Reloc *new,*r,**link;

  if (secno!=S_TEXT && secno!=S_DATA)
    ierror(0);

  new = mymalloc(sizeof(struct o65Reloc));
  new->offset = offs;
  new->type = type;
  new->segid = segid;
  new->import = import;
  new->addend = addend;

  link = &relocs[secno];
  while (r = *link) {
    if ((utaddr)offs < (utaddr)r->offset)
      break;
    link = &r->next;
  }
  new->next = r;
  *link = new;
}


static int add_import(char *name)
{
  static struct o65Import *last;
  hashdata data;

  if (!find_name(importhash,name,&data)) {
    struct o65Import *impnode = mymalloc(sizeof(struct o65Import));

    impnode->next = NULL;
    impnode->name = name;
    impnode->idx = num_imports++;
    if (importlist) {
      last->next = impnode;
      last = impnode;
    }
    else
      importlist = last = impnode;
    data.ptr = impnode;
    add_hashentry(importhash,name,data);
  }
  return ((struct o65Import *)data.ptr)->idx;
}


static void do_relocs(int secno,taddr offs,atom *p)
/* Try to resolve all relocations in a DATA or SPACE atom. */
{
  rlist *rl;

  if (p->type == DATA)
    rl = p->content.db->relocs;
  else if (p->type == SPACE)
    rl = p->content.sb->relocs;
  else
    rl = NULL;

  for (; rl!=NULL; rl=rl->next) {
    uint8_t type,segid;
    int import;

    if (type = o65_rtype(rl)) {
      symbol *sym = ((nreloc *)rl->reloc)->sym;
      taddr a = ((nreloc *)rl->reloc)->addend;

      if (sym->type == IMPORT) {
        /* reference to external symbol, add its name to import-list */
        import = add_import(sym->name);
        segid = SEG_UND;
      }
      else {
        if (sym->sec == NULL)
          ierror(0);
        segid = SEG_SEC + sym->sec->idx;
        a += sym->sec->org;
      }

      switch (type) {
        case 0x20:  /* address low-byte */
          patch_nreloc(p,rl,0,a&0xff,0);
          break;
        case 0x40:  /* address high-byte */
          patch_nreloc(p,rl,0,(a&0xff00)>>8,0);
          break;
        case 0xa0:  /* segment-byte of 24-bit address */
          patch_nreloc(p,rl,0,(a&0xff0000)>>16,0);
          break;
        default:
          patch_nreloc(p,rl,1,a,0);
          break;
      }
      add_o65reloc(secno,offs+((nreloc *)rl->reloc)->byteoffset,
                   type,segid,import,a);
    }
    else
      unsupp_reloc_error(rl);
  }
}


static void o65_writesection(FILE *f,section *sec)
{
  if (sec) {
    int secno = sec->idx;
    utaddr pc = sec->org;
    utaddr npc;
    atom *a;

    for (a=sec->first; a; a=a->next) {
      npc = fwpcalign(f,a,sec,pc);
      do_relocs(secno,npc-sec->org,a);
      if (a->type == DATA)
        fwdata(f,a->content.db->data,a->content.db->size);
      else if (a->type == SPACE)
        fwsblock(f,a->content.sb);
      pc = npc + atom_size(a,sec,npc);
    }
  }
}


static void o65_writeimports(FILE *f)
{
  struct o65Import *impnode;

  fwsize(f,num_imports);
  for (impnode=importlist; impnode!=NULL; impnode=impnode->next) {
    fwdata(f,impnode->name,strlen(impnode->name)+1);
    num_imports--;
  }
  if (num_imports)
    ierror(0);
}


static void o65_writerelocs(FILE *f,struct o65Reloc *r)
{
  int lastoffs = -1;
  int newoffs,diff;

  while (r) {
    newoffs = r->offset;
    if ((diff = newoffs - lastoffs) <= 0)
      ierror(0);
    lastoffs = newoffs;

    /* write offset to next relocation field */
    while (diff > 254) {
      fw8(f,255);
      diff -= 254;
    }
    fw8(f,diff);

    /* write reloc type and additional bytes */ 
    fw8(f,r->type|r->segid);
    if (r->segid == SEG_UND)
      fwsize(f,r->import);  /* index of imported symbol name */
    switch (r->type) {
      case 0x40:  /* address high-byte */
        if (!paged)
          fw8(f,r->addend&0xff);  /* write low-byte */
        break;
      case 0xa0:  /* segment-byte of 24-bit address */
        fw16(f,r->addend&0xffff,0);
        break;
    }

    r = r->next;
  }
  fw8(f,0);  /* end of reloc table */
}


static void o65_writeglobals(FILE *f,symbol *sym)
{
  long cntoffs = ftell(f);
  size_t n = 0;

  fwsize(f,0);  /* remember to write number of globals here */

  for (; sym!=NULL; sym=sym->next) {
    if ((sym->type==LABSYM || sym->type==EXPRESSION) &&
        !(sym->flags & VASMINTERN) && (sym->flags & EXPORT)) {

      if (sym->flags & WEAK) {
        output_error(10,sym->name);  /* weak symbol not supported */
        continue;
      }

      /* write global label or expression symbol */
      fwdata(f,sym->name,strlen(sym->name)+1);
      if (sym->type == LABSYM) {
        if (sym->sec == NULL)
          ierror(0);
        fw8(f,SEG_SEC+sym->sec->idx);
        fwsize(f,sym->sec->org+sym->pc);
      }
      else {
        fw8(f,SEG_ABS);
        fwsize(f,get_sym_value(sym));
      }
      n++;
    }
  }

  /* patch number of exported symbols */
  if (n) {
    fseek(f,cntoffs,SEEK_SET);
    fwsize(f,n);
    fseek(f,0,SEEK_END);
  }
}


static void write_output(FILE *f,section *sec,symbol *sym)
{
  importhash = new_hashtable(IMPHTABSIZE);
  o65_initwrite(sec);
  o65_header(f);
  o65_writesection(f,sections[S_TEXT]);
  o65_writesection(f,sections[S_DATA]);
  o65_writeimports(f);
  o65_writerelocs(f,relocs[S_TEXT]);
  o65_writerelocs(f,relocs[S_DATA]);
  o65_writeglobals(f,sym);
}


static int common_args(char *p)
{
  long val;

  if (!strncmp(p,"-bss=",5)) {
    sscanf(p+5,"%li",&val);
    secorgs[S_BSS] = val;
    org_is_set[S_BSS] = 1;
  }
  else if (!strncmp(p,"-data=",6)) {
    sscanf(p+6,"%li",&val);
    secorgs[S_DATA] = val;
    org_is_set[S_DATA] = 1;
  }
  else if (!strcmp(p,"-fopts"))
    fopts |= 3;
  else if (!strncmp(p,"-foauthor=",10)) {
    fopts |= 1;
    foauthor = get_str_arg(p+10);
  }
  else if (!strncmp(p,"-foname=",8)) {
    fopts |= 1;
    foname = get_str_arg(p+8);
  }
  else if (!strcmp(p,"-paged"))
    paged = 1;
  else if (!strncmp(p,"-secalign=",10)) {
    sscanf(p+10,"%li",&val);
    if (val<0 || val>3)
      general_error(78,p);
    else
      secalign = val>2 ? 8 : val;
  }
  else if (!strncmp(p,"-stack=",7)) {
    sscanf(p+7,"%li",&val);
    if (val < 0)
      general_error(78,p);
    else
      stacksz = val;
  }
  else if (!strncmp(p,"-text=",6)) {
    sscanf(p+6,"%li",&val);
    secorgs[S_TEXT] = val;
    org_is_set[S_TEXT] = 1;
  }
  else if (!strncmp(p,"-zero=",6)) {
    sscanf(p+6,"%li",&val);
    secorgs[S_ZERO] = val;
    org_is_set[S_ZERO] = 1;
  }
  else
    return 0;

  return 1;
}


static int exec_args(char *p)
{
  if (!strcmp(p,"-bsszero")) {
    bsszero = 1;
    return 1;
  }
  return common_args(p);
}


int init_output_o65(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  *cp = out_copyright;
  *wo = write_output;
  *oa = exec_out ? exec_args : common_args;
  secname_attr = 1;
  return 1;
}

#else

int init_output_o65(char **cp,void (**wo)(FILE *,section *,symbol *),
                    int (**oa)(char *))
{
  return 0;
}
#endif
