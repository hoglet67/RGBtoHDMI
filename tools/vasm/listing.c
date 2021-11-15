/* listing.h - listing file */
/* (c) in 2020-2021 by Volker Barthelmann and Frank Wille */

#include "vasm.h"

#define MADDR(x) ((unsigned long long)((x)&taddrmask))
#define USEDMASK (USED|EXPORT|COMMON|WEAK|LOCAL|ABSLABEL)

typedef struct {
  const char *fmtname;
  void (*fmtfunction)(char *,section *);
} list_formats;

int produce_listing,listena,listnosyms;
int listformfeed=1,listlinesperpage=40;
listing *first_listing,*last_listing,*cur_listing;

static int listbpl=8;
static int listnoinc,listformat,listtitlecnt,listall,listlabelsonly;
static char **listtitles;
static int *listtitlelines;


int listing_option(char *arg)
{
  if (!strcmp("all",arg)) {
    listall = 1;
    return 1;
  }
  if (!strncmp("bpl=",arg,4)) {
    sscanf(&arg[4],"%i",&listbpl);
    if (listbpl<1 || listbpl>64) {
      listbpl = 8;
      general_error(78,"-Lbpl");
    }
    return 1;
  }
  if (!strncmp("fmt=",arg,4)) {
    set_listformat(&arg[4]);
    return 1;
  }
  if (!strcmp("lo",arg)) {
    listlabelsonly=1;
    return 1;
  }
  if (!strcmp("nf",arg)) {
    listformfeed=0;
    return 1;
  }
  if (!strcmp("ni",arg)) {
    listnoinc=1;
    return 1;
  }
  if (!strcmp("ns",arg)) {
    listnosyms=1;
    return 1;
  }
  if (arg[0] == 'l') {
    int i=arg[1]=='='?2:1;
    sscanf(&arg[i],"%i",&listlinesperpage);
    return 1;
  }
  return 0;
}

listing *new_listing(source *src,int line)
{
  listing *new = mymalloc(sizeof(*new));

  new->next = NULL;
  new->line = line;
  new->error = 0;
  new->atom = 0;
  new->sec = 0;
  new->pc = 0;
  new->src = src;

  if (first_listing) {
    last_listing->next = new;
    last_listing = new;
  }
  else
    first_listing = last_listing = new;
  cur_listing = new;
  return new;
}

void set_listing(int on)
{
  listena = on && produce_listing;
}

void set_list_title(char *p,int len)
{
  listtitlecnt++;
  listtitles=myrealloc(listtitles,listtitlecnt*sizeof(*listtitles));
  listtitles[listtitlecnt-1]=mymalloc(len+1);
  strncpy(listtitles[listtitlecnt-1],p,len);
  listtitles[listtitlecnt-1][len]=0;
  listtitlelines=myrealloc(listtitlelines,listtitlecnt*sizeof(*listtitlelines));
  listtitlelines[listtitlecnt-1]=cur_src->line;
}

static size_t get_symbols(symbol **symlist,uint32_t flags)
{
  size_t cnt;
  symbol *sym;

  for (cnt=0,sym=first_symbol; sym; sym=sym->next) {
    if ((sym->flags & VASMINTERN) || *sym->name==' ')
      continue;
    if (listall || sym->type!=EXPRESSION || (sym->flags & flags)) {
      cnt++;
      if (symlist != NULL)
        *symlist++ = sym;
    }
  }
  return cnt;
}

static int namecmp(const void *s1,const void *s2)
{
  return nocase ? stricmp((*(symbol **)s1)->name,(*(symbol **)s2)->name) :
                  strcmp((*(symbol **)s1)->name,(*(symbol **)s2)->name);
}

static int valcmp(const void *s1,const void *s2)
{
  return (int)(get_sym_value(*(symbol **)s1) - get_sym_value(*(symbol **)s2));
}

#if VASM_CPU_OIL
static void print_list_header(FILE *f,int cnt)
{
  if(cnt%listlinesperpage==0){
    if(cnt!=0&&listformfeed)
      fprintf(f,"\f");
    if(listtitlecnt>0){
      int i,t;
      for(i=0,t=-1;i<listtitlecnt;i++){
        if(listtitlelines[i]<=cnt+listlinesperpage)
          t=i;
      }
      if(t>=0){
        int sp=(120-strlen(listtitles[t]))/2;
        while(--sp)
          fprintf(f," ");
        fprintf(f,"%s\n",listtitles[t]);
      }
      cnt++;
    }
    fprintf(f,"Err  Line Loc.  S Object1  Object2  M Source\n");
  }
}

static void write_listing_old(char *listname,section *first_section)
{
  FILE *f;
  int nsecs,i,cnt=0,nl;
  section *secp;
  listing *p;
  atom *a;
  symbol *sym;
  taddr pc;
  char rel;

  if(!(f=fopen(listname,"w"))){
    general_error(13,listname);
    return;
  }
  for(nsecs=0,secp=first_section;secp;secp=secp->next)
    secp->idx=nsecs++;
  for(p=first_listing;p;p=p->next){
    if(!p->src||p->src->id!=0)
      continue;
    print_list_header(f,cnt++);
    if(p->error!=0)
      fprintf(f,"%04d ",p->error);
    else
      fprintf(f,"     ");
    fprintf(f,"%4d ",p->line);
    a=p->atom;
    while(a&&a->type!=DATA&&a->next&&a->next->line==a->line&&a->next->src==a->src)
      a=a->next;
    if(a&&a->type==DATA){
      int size=a->content.db->size;
      char *dp=a->content.db->data;
      pc=p->pc;
      fprintf(f,"%05lX %d ",(unsigned long)pc,(int)(p->sec?p->sec->idx:0));
      for(i=0;i<8;i++){
        if(i==4)
          fprintf(f," ");
        if(i<size){
          fprintf(f,"%02X",(unsigned char)*dp++);
          pc++;
        }else
          fprintf(f,"  ");
        /* append following atoms with align 1 directly */
        if(i==size-1&&i<7&&a->next&&a->next->align<=a->align&&a->next->type==DATA&&a->next->line==a->line&&a->next->src==a->src){
          a=a->next;
          size+=a->content.db->size;
          dp=a->content.db->data;
        }
      }
      fprintf(f," ");
      if(a->content.db->relocs){
        symbol *s=((nreloc *)(a->content.db->relocs->reloc))->sym;
        if(s->type==IMPORT)
          rel='X';
        else
          rel='0'+p->sec->idx;
      }else
        rel='A';
      fprintf(f,"%c ",rel);
    }else
      fprintf(f,"                           ");

    fprintf(f," %-.77s",p->txt);

    /* bei laengeren Daten den Rest ueberspringen */
    /* Block entfernen, wenn alles ausgegeben werden soll */
    if(a&&a->type==DATA&&i<a->content.db->size){
      pc+=a->content.db->size-i;
      i=a->content.db->size;
    }

    /* restliche DATA-Zeilen, wenn noetig */
    while(a){
      if(a->type==DATA){
        int size=a->content.db->size;
        char *dp=a->content.db->data+i;

        if(i<size){
          for(;i<size;i++){
            if((i&7)==0){
              fprintf(f,"\n");
              print_list_header(f,cnt++);
              fprintf(f,"          %05lX %d ",(unsigned long)pc,(int)(p->sec?p->sec->idx:0));
            }else if((i&3)==0)
              fprintf(f," ");
            fprintf(f,"%02X",(unsigned char)*dp++);
            pc++;
            /* append following atoms with align 1 directly */
            if(i==size-1&&a->next&&a->next->align<=a->align&&a->next->type==DATA&&a->next->line==a->line&&a->next->src==a->src){
              a=a->next;
              size+=a->content.db->size;
              dp=a->content.db->data;
            }
          }
          i=8-(i&7);
          if(i>=4)
            fprintf(f," ");
          while(i--){
            fprintf(f,"  ");
          }
          fprintf(f," %c",rel);
        }
        i=0;
      }
      if(a->next&&a->next->line==a->line&&a->next->src==a->src){
        a=a->next;
        pc=pcalign(a,pc);
        if(a->type==DATA&&a->content.db->relocs){
          symbol *s=((nreloc *)(a->content.db->relocs->reloc))->sym;
          if(s->type==IMPORT)
            rel='X';
          else
            rel='0'+p->sec->idx;
        }else
          rel='A';
      }else
        a=0;
    }
    fprintf(f,"\n");
  }
  fprintf(f,"\n\nSections:\n");
  for(secp=first_section;secp;secp=secp->next)
    fprintf(f,"%d  %s\n",(int)secp->idx,secp->name);
  if(!listnosyms){
    fprintf(f,"\n\nSymbols:\n");
    {
      symbol *last=0,*cur,*symo;
      for(symo=first_symbol;symo;symo=symo->next){
        cur=0;
        for(sym=first_symbol;sym;sym=sym->next){
          if(!last||stricmp(sym->name,last->name)>0)
            if(!cur||stricmp(sym->name,cur->name)<0)
              cur=sym;
        }
        if(cur){
          print_symbol(f,cur);
          fprintf(f,"\n");
          last=cur;
        }
      }
    }
  }
  if(errors==0)
    fprintf(f,"\nThere have been no errors.\n");
  else
    fprintf(f,"\nThere have been %d errors!\n",errors);
  fclose(f);
  for(p=first_listing;p;){
    listing *m=p->next;
    myfree(p);
    p=m;
  }
}
#else
static void write_listing_old(char *listname,section *first_section)
{
  unsigned long i,maxsrc=0;
  FILE *f;
  int nsecs;
  section *secp;
  listing *p;
  atom *a;
  symbol *sym;
  taddr pc;

  if(!(f=fopen(listname,"w"))){
    general_error(13,listname);
    return;
  }
  for(nsecs=1,secp=first_section;secp;secp=secp->next)
    secp->idx=nsecs++;
  for(p=first_listing;p;p=p->next){
    char err[6];
    if(p->error!=0)
      sprintf(err,"E%04d",p->error);
    else
      sprintf(err,"     ");
    if(p->src&&p->src->id>maxsrc)
      maxsrc=p->src->id;
    fprintf(f,"F%02d:%04d %s %s",(int)(p->src?p->src->id:0),p->line,err,p->txt);
    a=p->atom;
    pc=p->pc;
    while(a){
      if(a->type==DATA){
        unsigned long size=a->content.db->size;
        for(i=0;i<size&&i<32;i++){
          if((i&15)==0)
            fprintf(f,"\n               S%02d:%08lX: ",(int)(p->sec?p->sec->idx:0),(unsigned long)(pc));
          fprintf(f," %02X",(unsigned char)a->content.db->data[i]);
          pc++;
        }
        if(a->content.db->relocs)
          fprintf(f," [R]");
      }
      if(a->next&&a->next->list==a->list){
        a=a->next;
        pc=pcalign(a,pc);
      }else
        a=0;
    }
    fprintf(f,"\n");
  }
  fprintf(f,"\n\nSections:\n");
  for(secp=first_section;secp;secp=secp->next)
    fprintf(f,"S%02d  %s\n",(int)secp->idx,secp->name);
  fprintf(f,"\n\nSources:\n");
  for(i=0;i<=maxsrc;i++){
    for(p=first_listing;p;p=p->next){
      if(p->src&&p->src->id==i){
        fprintf(f,"F%02lu  %s\n",i,p->src->name);
        break;
      }
    }
  }
  fprintf(f,"\n\nSymbols:\n");
  for(sym=first_symbol;sym;sym=sym->next){
    print_symbol(f,sym);
    fprintf(f,"\n");
  }
  if(errors==0)
    fprintf(f,"\nThere have been no errors.\n");
  else
    fprintf(f,"\nThere have been %d errors!\n",errors);
  fclose(f);
  for(p=first_listing;p;){
    listing *m=p->next;
    myfree(p);
    p=m;
  }
}
#endif

static void write_listing_wide(char *listname,section *first_section)
{
  int addrw = bytespertaddr*2;  /* width of address field */
  source *lastsrc = NULL;
  FILE *f;
  section *secp;
  listing *l;
  int i;

  if (!(f = fopen(listname,"w"))) {
    general_error(13,listname);
    return;
  }

  fprintf(f,"Sections:\n");
  for (secp=first_section,i=0; secp; secp=secp->next,i++) {
    secp->idx = i;
    fprintf(f,"%02X: \"%s\" (%llX-%llX)\n",(unsigned)i,secp->name,
            MADDR(secp->org),MADDR(secp->pc));
  }
  fputc('\n',f);

  for (l=first_listing; l; l=l->next) {
    atom *a = l->atom;
    taddr pc = l->pc;
    int flag = 0;
    char stype = ':';
    size_t spc;

    if (l->src) {
      if (l->src->num_params >= 0)
        stype = 'M';
      else if (!strncmp(l->src->name,"REPEAT:",7))
        stype = 'R';
      else {
        if (listnoinc && l->src->parent!=NULL)
          continue;
        if (l->src != lastsrc) {
          fprintf(f,"\nSource: \"%s\"\n",l->src->name);
          lastsrc = l->src;
        }
      }
    }
    i = 0;

    while (a) {
      size_t dlen;

      spc = 0;
      if (a->type == DATA) {
        dlen = a->content.db->size;
        for (; i<dlen; i++,pc++) {
          if (!(i % listbpl)) {
            if (i) {
              if (!flag) {
                fprintf(f,"\t%6d%c %s\n",l->line,stype,l->txt);
                flag = 1;
              }
              else
                fputc('\n',f);
            }
            fprintf(f,"%02X:%0*llX ",
                    (unsigned)(l->sec?l->sec->idx:0),
                    addrw,MADDR(pc));
          }
          fprintf(f,"%02X",(unsigned)a->content.db->data[i]);
        }
      }
      else if (a->type == SPACE) {
        dlen = a->content.sb->size;
        if (spc = a->content.sb->space * dlen) {
          for (; i<dlen; i++,pc++) {
            if (!(i % listbpl)) {
              if (i) {
                if (!flag) {
                  fprintf(f,"\t%6d%c %s\n",l->line,stype,l->txt);
                  flag = 1;
                }
                else
                  fputc('\n',f);
              }
              fprintf(f,"%02X:%0*llX ",
                      (unsigned)(l->sec?l->sec->idx:0),
                      addrw,MADDR(pc));
            }
            fprintf(f,"%02X",(unsigned)a->content.sb->fill[i]);
          }
          spc -= dlen;
        }
      }
      if (i) {
        if (!flag) {
          fprintf(f,"%*c%6d%c %s",2*(listbpl-i)+1,'\t',l->line,stype,l->txt);
          if (spc) {
            fprintf(f,"\n%02X:%0*llX *",
                    (unsigned)(l->sec?l->sec->idx:0),
                    addrw,MADDR(pc));
            pc += spc;
          }
          flag = 1;
        }
        fputc('\n',f);
        i = 0;
      }

      if (a->next!=NULL && a->next->list==a->list) {
        a = a->next;
        pc = pcalign(a,pc);
      }
      else
        a = NULL;
    }
    if (!flag)  /* no data generated for this source line */
      fprintf(f,"%*c%6d%c %s\n",4+addrw+2*listbpl+1,'\t',l->line,stype,l->txt);
    if (l->error)
      fprintf(f,"%*c     ^-ERROR:%04d\n",4+addrw+2*listbpl+1,'\t',l->error);
  }

  if (!listnosyms) {
    size_t nsyms = get_symbols(NULL,USEDMASK);
    symbol *sym,**symlist;

    fprintf(f,"\n\nSymbols by name:\n");
    symlist = (symbol **)mymalloc(nsyms*sizeof(symbol *));
    get_symbols(symlist,USEDMASK);
    qsort(symlist,nsyms,sizeof(symbol *),namecmp);  /* sort by name */
    for (i=0; i<nsyms; i++) {
      sym = symlist[i];
      fprintf(f,"%-31.31s ",sym->name);
      if (sym->flags & COMMON) {
        fprintf(f,"COM %lld bytes, alignment %lld",
                (long long)get_sym_size(sym),(long long)sym->align);
      }
      else {
        char type;

        switch (sym->type) {
          case LABSYM:
            fprintf(f,"%02X:%0*llX",
                    (unsigned)(sym->sec?sym->sec->idx:0),
                    addrw,MADDR(sym->pc));
            break;
          case IMPORT:
            fprintf(f,"external");
            break;
          default:
            if (sym->flags & EQUATE)
              type = 'E';
            else if (sym->flags & REGLIST)
              type = 'R';
            else if (sym->flags & ABSLABEL)
              type = 'A';
            else
              type = 'S';
            fprintf(f," %c:%0*llX",
                    type,addrw,MADDR(get_sym_value(sym)));
        }
      }
      if (sym->flags & EXPORT)
        fprintf(f," EXP");
      if (sym->flags & LOCAL)
        fprintf(f," LOC");
      if (sym->flags & WEAK)
        fprintf(f," WEAK");
      fputc('\n',f);
    }

    if (listlabelsonly)
      fprintf(f,"\nLabels by address:\n");
    else
      fprintf(f,"\nSymbols by value:\n");
    qsort(symlist,nsyms,sizeof(symbol *),valcmp);  /* sort by value/address */
    for (i=0; i<nsyms; i++) {
      sym = symlist[i];
      if (sym->type==IMPORT || (sym->flags & (COMMON|REGLIST)))
        continue;
      if (listlabelsonly) {
        if (sym->type!=LABSYM &&
            (sym->type!=EXPRESSION || !(sym->flags & ABSLABEL)))
          continue;
      }
      fprintf(f,"%0*llX %s\n",addrw,MADDR(get_sym_value(sym)),sym->name);
    }

    myfree(symlist);
  }
}

static list_formats list_format_table[] = {
  { "wide", write_listing_wide },
  { "old", write_listing_old }
};

void set_listformat(const char *fmtname)
{
  int i;

  for (i=0; i<sizeof(list_format_table)/sizeof(list_format_table[0]); i++) {
    if (!stricmp(fmtname,list_format_table[i].fmtname)) {
      listformat = i;
      return;
    }
  }
  general_error(80,fmtname);  /* format selection ignored */
}

void write_listing(char *listname,section *first_section)
{
  list_format_table[listformat].fmtfunction(listname,first_section);
}
