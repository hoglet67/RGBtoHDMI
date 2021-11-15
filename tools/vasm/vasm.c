/* vasm.c  main module for vasm */
/* (c) in 2002-2021 by Volker Barthelmann */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "vasm.h"
#include "osdep.h"
#include "stabs.h"
#include "dwarf.h"

#define _VER "vasm 1.8l"
char *copyright = _VER " (c) in 2002-2021 Volker Barthelmann";
#ifdef AMIGA
static const char *_ver = "$VER: " _VER " " __AMIGADATE__ "\r\n";
#endif

/* The resolver will run another pass over the current section as long as any
   label location or atom size has changed. It gives up at MAXPASSES, which
   hopefully will never happen.
   During the first FASTOPTPHASE passes all instructions of a section will be
   optimized at the same time. After that the resolver enters a safe mode,
   where only a single instruction per pass is changed. */
#define MAXPASSES 1000
#define FASTOPTPHASE 200

source *cur_src;
char *filename,*debug_filename;
section *current_section;
char *inname,*outname;
taddr inst_alignment;
int done,secname_attr,unnamed_sections,nocase,no_symbols,asciiout;
int pic_check,final_pass,debug,exec_out,chklabels,warn_unalloc_ini_dat;
int nostdout;
struct stabdef *first_nlist,*last_nlist;
char *output_format="test";
unsigned long long taddrmask;
taddr taddrmin,taddrmax;
unsigned space_init;
char emptystr[]="";
char vasmsym_name[]="__VASM";
int num_secs;

static char *listname;
static FILE *outfile;
static char *dep_filename;

static section *first_section,*last_section;
#if NOT_NEEDED
static section *prev_sec,*prev_org;
#endif

/* stack for push/pop-section directives */
#define SECSTACKSIZE 64
static section *secstack[SECSTACKSIZE];
static int secstack_index;

/* MNEMOHTABSIZE should be defined by cpu module */
#ifndef MNEMOHTABSIZE
#define MNEMOHTABSIZE 0x1000
#endif
hashtable *mnemohash;

static int dwarf;
static int verbose=1,auto_import=1;
static int fail_on_warning;
static taddr sec_padding;

static char *output_copyright;
static void (*write_object)(FILE *,section *,symbol *);
static int (*output_args)(char *);


void leave(void)
{
  section *sec;
  symbol *sym;

  if(outfile){
    fclose(outfile);
    if (errors&&outname!=NULL)
      remove(outname);
  }

  if(debug){
    fprintf(stdout,"Sections:\n");
    for(sec=first_section;sec;sec=sec->next)
      print_section(stdout,sec);

    fprintf(stdout,"Symbols:\n");
    for(sym=first_symbol;sym;sym=sym->next){
      print_symbol(stdout,sym);
      fprintf(stdout,"\n");
    }
  }

  if(errors||(fail_on_warning&&warnings))
    exit(EXIT_FAILURE);
  else
    exit(EXIT_SUCCESS);
}

/* Convert all labels from an offset-section into absolute expressions. */
static void convert_offset_labels(void)
{
  symbol *sym;

  for (sym=first_symbol; sym; sym=sym->next) {
    if (sym->type==LABSYM && sym->sec!=NULL && (sym->sec->flags&UNALLOCATED)) {
      sym->type = EXPRESSION;
      sym->expr = number_expr(sym->pc);
      sym->sec = NULL;
    }
  }
}

/* Removes all unallocated (offset) sections from the list. */
static void remove_unalloc_sects(void)
{
  section *prev,*sec;

  for (sec=first_section,prev=NULL; sec; sec=sec->next) {
    if (sec->flags&UNALLOCATED) {
      if (prev)
        prev->next = sec->next;
      else
        first_section = sec->next;
    }
    else
      prev = sec;
  }
  last_section = prev;
}

/* convert reloffs-atom into one or more space-atoms */
static void roffs_to_space(section *sec,atom *p)
{
  uint8_t padding[MAXPADBYTES];
  taddr space,padbytes,n;
  sblock *sb = NULL;

  if (eval_expr(p->content.roffs->offset,&space,sec,sec->pc) &&
      (p->content.roffs->fillval==NULL ||
       eval_expr(p->content.roffs->fillval,&n,sec,sec->pc))) {
    space = sec->org + space - sec->pc;

    if (space >= 0) {
      if (p->content.roffs->fillval == NULL) {
        memcpy(padding,sec->pad,MAXPADBYTES);
        padbytes = sec->padbytes;
      }
      else
        padbytes = make_padding(n,padding,MAXPADBYTES);

      if (space >= padbytes) {
        n = balign(sec->pc,padbytes);  /* alignment is automatic */
        space -= n;
        sec->pc += n;  /* Important! Fix the PC for new alignment. */
        sb = new_sblock(number_expr(space/padbytes),padbytes,0);
        memcpy(sb->fill,padding,padbytes);
        p->type = SPACE;
        p->content.sb = sb;
        p->align = padbytes;

        space %= padbytes;
        if (space > 0) {
          /* fill the rest with zeros */
          atom *a = new_space_atom(number_expr(space),1,0);
          a->next = p->next;
          p->next = a;
        }
      }
      else {
        p->type = SPACE;
        p->content.sb = new_sblock(number_expr(space),1,0);
      }
    }
    else
      general_error(20);  /* rorg is lower than current pc */
  }
  else
    general_error(30);  /* expression must be constant */
}

/* convert atom's alignment-bytes into space, then reset its alignment */
static void alignment_to_space(taddr nb,section *sec,atom *pa,atom *a)
{
  atom *sa;

  if (pa==NULL || nb<=0)
    ierror(0);
  if (a->type==SPACE && !a->content.sb->space && !(nb%a->content.sb->size)) {
    /* take the fill pattern from this atom for alignment */
    sa = new_space_atom(number_expr(nb/a->content.sb->size),
                        a->content.sb->size,a->content.sb->fill_exp);
  }
  else
    sa = new_space_atom(number_expr(nb),1,0);  /* fill with zeros */
  pa->next = sa;
  sa->next = a;
  a->align = 1;
  if (atom_size(sa,sec,sec->pc) != (size_t)nb)  /* calculate atom size */
    ierror(0);
}

/* append a new stabs (nlist) symbol/debugging definition */
static void new_stabdef(aoutnlist *nlist,section *sec)
{
  struct stabdef *new = mymalloc(sizeof(struct stabdef));

  new->next = NULL;
  new->name.ptr = nlist->name;
  new->type = nlist->type;
  new->other = nlist->other;
  new->desc = nlist->desc;
  new->base = NULL;
  if (nlist->value == NULL)
    new->value = 0;
  else if (!eval_expr(nlist->value,&new->value,sec,sec->pc)) {
    int btype = find_base(nlist->value,&new->base,sec,sec->pc);
    if (btype==BASE_ILLEGAL || btype==BASE_PCREL) {
       new->base = NULL;
       general_error(38);  /* illegal relocation */
    }
    else if (new->base != NULL)
      new->base->flags |= REFERENCED;
  }
  if (last_nlist)
    last_nlist = last_nlist->next = new;
  else
    first_nlist = last_nlist = new;
}

/* emit internal debug info, triggered by a VASMDEBUG atom */
void vasmdebug(const char *f,section *s,atom *a)
{
  if (a->next != NULL) {
    a = a->next;
    printf("%s: (%s+0x%llx) %2d:%lu(%u) ",
           f,s->name,ULLTADDR(s->pc),a->type,(unsigned long)a->lastsize,a->changes);
    print_atom(stdout,a);
    putchar('\n');
  }
}

static int resolve_section(section *sec)
{
  taddr rorg_pc,org_pc;
  int fastphase=FASTOPTPHASE;
  int pass=0;
  int extrapass,rorg;
  size_t size;
  atom *p;

  do{
    done=1;
    rorg=0;
    if (++pass>=MAXPASSES){
      general_error(7,sec->name);
      break;
    }
    extrapass=pass<=fastphase;
    if(debug)
      printf("resolve_section(%s) pass %d%s",sec->name,pass,
             pass<=fastphase?" (fast)\n":"\n");
    sec->pc=sec->org;
    for(p=sec->first;p;p=p->next){
      sec->pc=pcalign(p,sec->pc);
      if(cur_src=p->src)
        cur_src->line=p->line;
#if HAVE_CPU_OPTS
      if(p->type==OPTS){
        cpu_opts(p->content.opts);
      }
      else
#endif
      if(p->type==RORG){
        if(rorg)
          general_error(43);  /* reloc org is already set */
        rorg_pc=*p->content.rorg;
        org_pc=sec->pc;
        sec->pc=rorg_pc;
        sec->flags|=ABSOLUTE;
        rorg=1;
      }
      else if(p->type==RORGEND&&rorg){
        sec->pc=org_pc+(sec->pc-rorg_pc);
        rorg_pc=0;
        sec->flags&=~ABSOLUTE;
        rorg=0;
      }
      else if(p->type==LABEL){
        symbol *label=p->content.label;
        if(label->type!=LABSYM)
          ierror(0);
        if(label->pc!=sec->pc){
          if(debug)
            printf("moving label %s from %lu to %lu\n",label->name,
                   (unsigned long)label->pc,(unsigned long)sec->pc);
          done=0;
          label->pc=sec->pc;
        }
      }
      else if(p->type==VASMDEBUG)
        vasmdebug("resolve_section",sec,p);
      if(pass>fastphase&&!done&&p->type==INSTRUCTION){
        /* entered safe mode: optimize only one instruction every pass */
        sec->pc+=p->lastsize;
        continue;
      }
      if(p->changes>MAXSIZECHANGES){
        /* atom changed size too frequently, set warning flag */
        if(debug)
          printf("setting resolve-warning flag for atom type %d at %lu\n",
                 p->type,(unsigned long)sec->pc);
        sec->flags|=RESOLVE_WARN;
        size=atom_size(p,sec,sec->pc);
        sec->flags&=~RESOLVE_WARN;
      }
      else
        size=atom_size(p,sec,sec->pc);
      if(size!=p->lastsize){
        if(debug)
          printf("modify size of atom type %d at %lu from %lu to %lu\n",
                 p->type,(unsigned long)sec->pc,(unsigned long)p->lastsize,
                 (unsigned long)size);
        done=0;
        if(pass>fastphase)
          p->changes++;  /* now count size modifications of atoms */
        else if(size>p->lastsize)
          extrapass=0;   /* no extra pass, when an atom became larger */
        p->lastsize=size;
      }
      sec->pc+=size;
    }
    if(rorg){
      sec->pc=org_pc+(sec->pc-rorg_pc);
      sec->flags&=~ABSOLUTE;  /* workaround for misssing RORGEND */
    }
    /* Extend the fast-optimization phase, when there was no atom which
       became larger than in the previous pass. */
    if(extrapass) fastphase++;
  }while(errors==0&&!done);
  return pass;
}

static void bvunite(bvtype *dest,bvtype *src,size_t len)
{
  len/=sizeof(bvtype);
  for(;len>0;len--)
    *dest++|=*src++;
}

static void resolve(void)
{
  section *sec;
  bvtype *todo;
  int finished;

  final_pass=0;
  if(debug)
    printf("resolve()\n");

  for(num_secs=0, sec=first_section;sec;sec=sec->next)
    sec->idx=num_secs++;

  todo=mymalloc(BVSIZE(num_secs));
  memset(todo,~(bvtype)0,BVSIZE(num_secs));

  do{
    finished=1;
    for(sec=first_section;sec;sec=sec->next)
      if(BTST(todo, sec->idx)){
	int passes;
	finished=0;
	passes = resolve_section(sec);
	BCLR(todo, sec->idx);
	if(passes>1){
	  if(sec->deps)
	    bvunite(todo, sec->deps, BVSIZE(num_secs));
	}
      }
  }while(!finished);
}

static void assemble(void)
{
  taddr basepc,rorg_pc,org_pc;
  struct dwarf_info dinfo;
  int bss,rorg;
  section *sec;
  atom *p,*pp;

  convert_offset_labels();
  if(dwarf){
    dinfo.version=dwarf;
    dinfo.producer=cnvstr(copyright,strchr(copyright,'(')-copyright-1);
    source_debug_init(1,&dinfo);
  }
  final_pass=1;
  rorg=0;
  for(sec=first_section;sec;sec=sec->next){
    source *lasterrsrc=NULL;
    utaddr oldpc;
    int lasterrline=0,ovflw=0;
    sec->pc=sec->org;
    bss=strchr(sec->attr,'u')!=NULL;
    for(p=sec->first,pp=NULL;p;p=p->next){
      basepc=sec->pc;
      sec->pc=pcalign(p,sec->pc);
      if(cur_src=p->src)
        cur_src->line=p->line;
      if(p->list&&p->list->atom==p){
        p->list->sec=sec;
        p->list->pc=sec->pc;
      }
      if(p->changes>MAXSIZECHANGES)
        sec->flags|=RESOLVE_WARN;
      /* print a warning on auto-aligned instructions or data */
      if(sec->pc!=basepc){
        atom *aa;
        if(p->type==LABEL&&p->next!=NULL&&p->next->line==p->line)
          aa=p->next; /* next atom in same line, look at it instead of label */
        else
          aa=p;
        if(aa->type==INSTRUCTION)
          general_error(50);  /* instruction has been auto-aligned */
        else if(aa->type==DATA||aa->type==DATADEF)
          general_error(57);  /* data has been auto-aligned */
        if(rorg)
          alignment_to_space(sec->pc-basepc,sec,pp,p);
      }
      else if(rorg)
        p->align=1;  /* disable ineffective alignment in relocated org block */
      if(p->type==RORG){
        rorg_pc=*p->content.rorg;
        org_pc=sec->pc;
        sec->pc=rorg_pc;
        sec->flags|=ABSOLUTE;
        rorg=1;
      }
      else if(p->type==RORGEND){
        if(rorg){
          sec->pc=org_pc+(sec->pc-rorg_pc);
          rorg_pc=0;
          sec->flags&=~ABSOLUTE;
          rorg=0;
        }
        else
          general_error(44);  /* reloc org was not set */
      }
      else if(p->type==INSTRUCTION){
        dblock *db;
        cur_listing=p->list;
        db=eval_instruction(p->content.inst,sec,sec->pc);
        if(pic_check)
          do_pic_check(db->relocs);
        cur_listing=0;
        if(debug){
          if(db->size!=(p->content.inst->code>=0?
                        instruction_size(p->content.inst,sec,sec->pc):0))
            ierror(0);
        }
        if(dwarf){
          if(cur_src->defsrc)
            dwarf_line(&dinfo,sec,cur_src->defsrc->srcfile->index,
                       cur_src->defline+cur_src->line);
          else
            dwarf_line(&dinfo,sec,cur_src->srcfile->index,cur_src->line);
        }
        /*FIXME: sauber freigeben */
        myfree(p->content.inst);
        p->content.db=db;
        p->type=DATA;
      }
      else if(p->type==DATADEF){
        dblock *db;
        cur_listing=p->list;
        db=eval_data(p->content.defb->op,p->content.defb->bitsize,sec,sec->pc);
        if(pic_check)
          do_pic_check(db->relocs);
        cur_listing=0;
        /*FIXME: sauber freigeben */
        myfree(p->content.defb);
        p->content.db=db;
        p->type=DATA;
      }
      else if(p->type==ROFFS)
        roffs_to_space(sec,p);
#if HAVE_CPU_OPTS
      else if(p->type==OPTS)
        cpu_opts(p->content.opts);
#endif
      else if(p->type==PRINTTEXT&&!nostdout)
        printf("%s",p->content.ptext);
      else if(p->type==PRINTEXPR&&!nostdout)
        atom_printexpr(p->content.pexpr,sec,sec->pc);
      else if(p->type==ASSERT){
        assertion *ast=p->content.assert;
        taddr val;
        if(ast->assert_exp!=NULL) {
          eval_expr(ast->assert_exp,&val,sec,sec->pc);
          if(val==0)
            general_error(47,ast->expstr,ast->msgstr?ast->msgstr:emptystr);
        }
        else /* ASSERT without expression, used for user-FAIL directives */
          general_error(19,ast->msgstr?ast->msgstr:emptystr);
      }
      else if(p->type==NLIST)
        new_stabdef(p->content.nlist,sec);
      else if(p->type==VASMDEBUG)
        vasmdebug("assemble",sec,p);
      if(p->type==DATA&&bss){
        if(lasterrsrc!=p->src||lasterrline!=p->line){
          if(sec->flags&UNALLOCATED){
            if(warn_unalloc_ini_dat)
            general_error(54);  /* initialized data in offset section */
          }
          else
            general_error(31);  /* initialized data in bss */
          lasterrsrc=p->src;
          lasterrline=p->line;
        }
      }
      oldpc=sec->pc;
      sec->pc+=atom_size(p,sec,sec->pc);
      if((utaddr)sec->pc!=oldpc){
        if((utaddr)(sec->pc-1)<oldpc||ovflw)
          general_error(45);  /* address space overflow */
        ovflw=sec->pc==0;
      }
      sec->flags&=~RESOLVE_WARN;
      pp=p;  /* prev atom */
    }
    /* leave RORG-mode, when section ends */
    if(rorg){
      sec->pc=org_pc+(sec->pc-rorg_pc);
      rorg_pc=0;
      sec->flags&=~ABSOLUTE;
      rorg=0;
    }
    if(dwarf)
      dwarf_end_sequence(&dinfo,sec);
  }
  remove_unalloc_sects();
  if(dwarf)
    dwarf_finish(&dinfo);
}

static void undef_syms(void)
{
  symbol *sym;

  for(sym=first_symbol;sym;sym=sym->next){
    if (!auto_import&&sym->type==IMPORT&&!(sym->flags&(EXPORT|COMMON|WEAK)))
      general_error(22,sym->name);
    else if (sym->type==IMPORT&&!(sym->flags&REFERENCED))
      general_error(61,sym->name);
  }
}

static void fix_labels(void)
{
  symbol *sym,*base;
  taddr val;

  for(sym=first_symbol;sym;sym=sym->next){
    /* turn all absolute mode labels into absolute symbols */
    if((sym->flags&ABSLABEL)&&sym->type==LABSYM){
      sym->type=EXPRESSION;
      sym->flags&=~(TYPE_MASK|COMMON);
      sym->sec=NULL;
      sym->size=NULL;
      sym->align=0;
      sym->expr=number_expr(sym->pc);
    }
    /* expressions which are based on a label are turned into a new label */
    else if(sym->type==EXPRESSION){
      if(!eval_expr(sym->expr,&val,NULL,0)){
        if(find_base(sym->expr,&base,NULL,0)==BASE_OK){
          /* turn into an offseted label symbol from the base's section */
          sym->type=base->type;
          sym->sec=base->sec;
          sym->pc=val;
          sym->align=1;
        }else
          general_error(53,sym->name);  /* non-relocatable expr. in equate */
      }
    }
  }
}

static void trim_uninitialized(section *sec)
{
  for (; sec!=NULL; sec=sec->next) {
    atom *a;
    utaddr pc;

    sec->last = NULL;
    for (a=sec->first,pc=sec->org; a; a=a->next) {
      pc = pcalign(a,pc);
      pc += atom_size(a,sec,pc);
      if (a->type==DATA ||
          (a->type==SPACE && !(a->content.sb->flags & SPC_UNINITIALIZED))) {
        /* remember last initialized atom and pc of this section */
        sec->pc = pc;
        sec->last = a;
      }
    }
  }
}

static void statistics(void)
{
  section *sec;
  unsigned long long size;

  printf("\n");
  for(sec=first_section;sec;sec=sec->next){
    size=(utaddr)(sec->pc)-(utaddr)(sec->org);
    printf("%s(%s%lu):\t%12llu byte%c\n",sec->name,sec->attr,
           (unsigned long)sec->align,size,size==1?' ':'s');
  }
}

static int init_output(char *fmt)
{
  if(!strcmp(fmt,"test"))
    return init_output_test(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"elf"))
    return init_output_elf(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"bin"))
    return init_output_bin(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"srec"))
    return init_output_srec(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"vobj"))
    return init_output_vobj(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"hunk"))
    return init_output_hunk(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"aout"))
    return init_output_aout(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"hunkexe")){
    exec_out=1;  /* executable format */
    return init_output_hunk(&output_copyright,&write_object,&output_args);
  }
  if(!strcmp(fmt,"tos")){
    exec_out=1;  /* executable format */
    return init_output_tos(&output_copyright,&write_object,&output_args);
  }
  if(!strcmp(fmt,"xfile")){
    exec_out=1;  /* executable format */
    return init_output_xfile(&output_copyright,&write_object,&output_args);
  }
  if(!strcmp(fmt,"cdef"))
    return init_output_cdef(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"ihex"))
    return init_output_ihex(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"o65"))
    return init_output_o65(&output_copyright,&write_object,&output_args);
  if(!strcmp(fmt,"o65exe")) {
    exec_out=1;  /* executable format */
    return init_output_o65(&output_copyright,&write_object,&output_args);
  }
  return 0;
}

static int init_main(void)
{
  int i;
  char *last;
  hashdata data;
  mnemohash=new_hashtable(MNEMOHTABSIZE);
  i=0;
  while(i<mnemonic_cnt){
    data.idx=i;
    last=mnemonics[i].name;
    add_hashentry(mnemohash,mnemonics[i].name,data);
    do{
      i++;
    }while(i<mnemonic_cnt&&!strcmp(last,mnemonics[i].name));
  }
  if(debug){
    if(mnemohash->collisions)
      printf("*** %d mnemonic collisions!!\n",mnemohash->collisions);
  }
  new_include_path(emptystr);  /* index 0: current work directory */
  taddrmask=MAKEMASK(bytespertaddr<<3);
  taddrmax=((utaddr)~0)>>1;
  taddrmin=~taddrmax;
  inst_alignment=INST_ALIGN;
  return 1;
}

void set_default_output_format(char *fmt)
{
  output_format=fmt;
}

static void include_main_source(void)
{
  if (inname) {
    char *filepart;

    if ((filepart = get_filepart(inname)) != inname) {
      /* main source is not in current dir., set compile-directory path */
      compile_dir = cnvstr(inname,filepart-inname);
      main_include_path(compile_dir);
    }
    else
      compile_dir = NULL;

    if (include_source(filepart)) {
      setfilename(filepart);
      setdebugname(inname);
    }
  }
  else
    general_error(15);
}

int main(int argc,char **argv)
{
  int i;
  for(i=1;i<argc;i++){
    if(argv[i][0]=='-'&&argv[i][1]=='F'){
      output_format=argv[i]+2;
      argv[i][0]=0;
    }
    if(!strcmp("-quiet",argv[i])){
      verbose=0;
      argv[i][0]=0;
    }
    if(!strcmp("-debug",argv[i])){
      debug=1;
      argv[i][0]=0;
    }
  }
  if(!init_output(output_format))
    general_error(16,output_format);
  if(!init_main())
    general_error(10,"main");
  if(!init_symbol())
    general_error(10,"symbol");
  if(!init_osdep())
    general_error(10,"osdep");
  if(verbose)
    printf("%s\n%s\n%s\n%s\n",copyright,cpu_copyright,syntax_copyright,output_copyright);
  for(i=1;i<argc;i++){
    if(argv[i][0]==0)
      continue;
    if(argv[i][0]!='-'){
      if(inname)
        general_error(11);
      inname=argv[i];
      continue;
    }
    if(!strcmp("-o",argv[i])&&i<argc-1){
      if(outname)
        general_error(28,argv[i]);
      outname=argv[++i];
      continue;
    }
    if(!strncmp("-L",argv[i],2)){
      if(!argv[i][2]&&i<argc-1){
        if(listname)
          general_error(28,argv[i]);
        listname=argv[++i];
        produce_listing=1;
        set_listing(1);
        continue;
      }
      else if (listing_option(&argv[i][2]))
        continue;
    }
    if(!strncmp("-D",argv[i],2)){
      char *def=NULL;
      expr *val;
      if(argv[i][2])
        def=&argv[i][2];
      else if (i<argc-1)
        def=argv[++i];
      if(def){
        char *s=def;
        if(ISIDSTART(*s)){
          s++;
          while(ISIDCHAR(*s))
            s++;
          def=cnvstr(def,s-def);
          if(*s=='='){
            s++;
            val=parse_expr(&s);
          }
          else
            val=number_expr(1);
          if(*s)
            general_error(23,'D');  /* trailing garbage after option */
          new_equate(def,val);
          myfree(def);
          continue;
        }
      }
    }
    if(!strncmp("-I",argv[i],2)){
      char *path=NULL;
      if(argv[i][2])
        path=&argv[i][2];
      else if (i<argc-1)
        path=argv[++i];
      if(path){
        new_include_path(path);
        continue;
      }
    }
    if(!strncmp("-depend=",argv[i],8) || !strncmp("-dependall=",argv[i],11)){
      depend_all=argv[i][7]!='=';
      if(!strcmp("list",&argv[i][depend_all?11:8])){
        depend=DEPEND_LIST;
        continue;
      }
      else if(!strcmp("make",&argv[i][depend_all?11:8])){
        depend=DEPEND_MAKE;
        continue;
      }
    }
    if(!strcmp("-depfile",argv[i])&&i<argc-1){
      if(dep_filename)
        general_error(28,argv[i]);
      dep_filename=argv[++i];
      continue;
    }
    if(!strcmp("-unnamed-sections",argv[i])){
      unnamed_sections=1;
      continue;
    }
    if(!strcmp("-ignore-mult-inc",argv[i])){
      ignore_multinc=1;
      continue;
    }
    if(!strcmp("-nocase",argv[i])){
      nocase=1;
      continue;
    }
    if(!strncmp("-nomsg=",argv[i],7)){
      int mno;
      sscanf(argv[i]+7,"%i",&mno);
      disable_message(mno);
      continue;
    }
    if(!strcmp("-nosym",argv[i])){
      no_symbols=1;
      continue;
    }
    if(!strncmp("-nowarn=",argv[i],8)){
      int wno;
      sscanf(argv[i]+8,"%i",&wno);
      disable_warning(wno);
      continue;
    }
    else if(!strcmp("-w",argv[i])){
      no_warn=1;
      continue;
    }
    else if(!strcmp("-wfail",argv[i])){
      fail_on_warning=1;
      continue;
    }
    if(!strncmp("-maxerrors=",argv[i],11)){
      sscanf(argv[i]+11,"%i",&max_errors);
      continue;
    }
    else if(!strcmp("-pic",argv[i])){
      pic_check=1;
      continue;
    }
    else if(!strncmp("-maxmacrecurs=",argv[i],14)){
      sscanf(argv[i]+14,"%i",&maxmacrecurs);
      continue;
    }
    else if(!strcmp("-unsshift",argv[i])){
      unsigned_shift=1;
      continue;
    }
    else if(!strcmp("-chklabels",argv[i])){
      chklabels=1;
      continue;
    }
    else if(!strcmp("-noialign",argv[i])){
      inst_alignment=1;
      continue;
    }
    else if(!strncmp("-dwarf",argv[i],6)){
      if(argv[i][6]=='=')
        sscanf(argv[i]+7,"%i",&dwarf);  /* get DWARF version */
      else
        dwarf=3;  /* default to DWARF3 */
      continue;
    }
    else if(!strncmp("-pad=",argv[i],5)){
      long long ullpadding;
      sscanf(argv[i]+5,"%lli",&ullpadding);
      sec_padding=(taddr)ullpadding;
      continue;
    }
    else if(!strncmp("-uspc=",argv[i],6)){
      sscanf(argv[i]+6,"%u",&space_init);
      continue;
    }
    if(cpu_args(argv[i]))
      continue;
    if(syntax_args(argv[i]))
      continue;
    if(output_args(argv[i]))
      continue;
    if(!strcmp("-esc",argv[i])){
      esc_sequences=1;
      continue;
    }
    if(!strcmp("-noesc",argv[i])){
      esc_sequences=0;
      continue;
    }
    if (!strncmp("-x",argv[i],2)){
      auto_import=0;
      continue;
    }
    general_error(14,argv[i]);
  }
  nostdout=depend&&dep_filename==NULL; /* dependencies to stdout nothing else */
  include_main_source();
  internal_abs(vasmsym_name);
  if(!init_parse())
    general_error(10,"parse");
  if(!init_syntax())
    general_error(10,"syntax");
  if(!init_cpu())
    general_error(10,"cpu");
  parse();
  listena=0;
  if(errors==0||produce_listing)
    resolve();
  if(errors==0||produce_listing)
    assemble();
  cur_src=NULL;
  if(errors==0)
    undef_syms();
  fix_labels();
  if(produce_listing){
    if(!listname)
      listname="a.lst";
    write_listing(listname,first_section);
  }
  if(errors==0){
    if(depend&&dep_filename==NULL){
      /* dependencies to stdout, no object output */
      write_depends(stdout);
    } else {
      trim_uninitialized(first_section);
      if(verbose)
        statistics();
      if(depend&&dep_filename!=NULL){
        /* write dependencies to a named file first */
        FILE *depfile = fopen(dep_filename,"w");
        if (depfile){
          write_depends(depfile);
          fclose(depfile);
        }
        else
          general_error(13,dep_filename);
      }
      /* write the object file */
      if(!outname)
        outname="a.out";
      outfile=fopen(outname,asciiout?"w":"wb");
      if(!outfile)
        general_error(13,outname);
      else
        write_object(outfile,first_section,first_symbol);
    }
  }
  leave();
  return 0; /* not reached */
}

/* searches a section by name and attr (if secname_attr set) */
section *find_section(char *name,char *attr)
{
  section *p;
  if(secname_attr){
    for(p=first_section;p;p=p->next){
      if(!strcmp(name,p->name) && !strcmp(attr,p->attr))
        return p;
    }
  }
  else{
    for(p=first_section;p;p=p->next){
      if(!strcmp(name,p->name))
        return p;
    }
  }
  return 0;
}

/* try to find a matching section name for the given attributes */
static char *name_from_attr(char *attr)
{
  while(*attr) {
    switch(*attr++) {
      case 'c': return "text";
      case 'd': return "data";
      case 'u': return "bss";
    }
  }
  return emptystr;
}

/* set current section, remember last */
void set_section(section *s)
{
#if NOT_NEEDED
  if (current_section!=NULL && !(current_section->flags & UNALLOCATED)) {
    if (current_section->flags & ABSOLUTE)
      prev_org = current_section;
    else
      prev_sec = current_section;
  }
#endif
#if HAVE_CPU_OPTS
  if (s!=NULL && !(s->flags & UNALLOCATED))
    cpu_opts_init(s);  /* set initial cpu opts before the first atom */
#endif
  current_section = s;
}

/* creates a new section with given attributes and alignment;
   does not switch to this section automatically */
section *new_section(char *name,char *attr,int align)
{
  section *p;
  if(unnamed_sections)
    name=name_from_attr(attr);
  if(p=find_section(name,attr))
    return p;
  p=mymalloc(sizeof(*p));
  p->next=0;
  p->deps=0;
  p->name=mystrdup(name);
  p->attr=mystrdup(attr);
  p->first=p->last=0;
  p->align=align;
  p->org=p->pc=0;
  p->flags=0;
  p->memattr=0;
  memset(p->pad,0,MAXPADBYTES);
  if(sec_padding)
    p->padbytes=make_padding(sec_padding,p->pad,MAXPADBYTES);
  else
    p->padbytes=1;
  if(last_section)
    last_section=last_section->next=p;
  else
    first_section=last_section=p;
  return p;
}

/* create a dummy code section for each new ORG directive */
section *new_org(taddr org)
{
  char buf[16];
  section *sec;

  sprintf(buf,"seg%llx",ULLTADDR(org));
  sec = new_section(buf,"acrwx",1);
  sec->org = sec->pc = org;
  sec->flags |= ABSOLUTE;  /* absolute destination address */
  return sec;
}

/* switches current section to the section with the specified name */
void switch_section(char *name,char *attr)
{
  section *p;
  if(unnamed_sections)
    name=name_from_attr(attr);
  p=find_section(name,attr);
  if(!p)
    general_error(2,name);
  else
    set_section(p);
}

/* Switches current section to an offset section. Create a new section when
   it doesn't exist yet or needs a different offset. */
void switch_offset_section(char *name,taddr offs)
{
  static unsigned long id;
  char unique_name[14];
  section *sec;

  if (!name) {
    if (offs != -1)
      ++id;
    sprintf(unique_name,"OFFSET%06lu",id);
    name = unique_name;
  }
  sec = new_section(name,"u",1);
  sec->flags |= UNALLOCATED;
  if (offs != -1)
    sec->org = sec->pc = offs;
  set_section(sec);
}

/* returns current_section or the syntax module's default section,
   when undefined */
section *default_section(void)
{
  section *sec = current_section;

  if (!sec && defsectname && defsecttype) {
    sec = new_section(defsectname,defsecttype,1);
    switch_section(defsectname,defsecttype);
  }
  return sec;
}

#if NOT_NEEDED
/* restore last relocatable section */
section *restore_section(void)
{
  if (prev_sec)
    return prev_sec;
  if (defsectname && defsecttype)
    return new_section(defsectname,defsecttype,1);
  return NULL;  /* no previous section or default section defined */
}

/* restore last absolute section */
section *restore_org(void)
{
  if (prev_org)
    return prev_org;
  return new_org(0);  /* no previous org: default to ORG 0 */
}
#endif /* NOT_NEEDED */

/* push current section onto the stack, does not switch to a new section */
void push_section(void)
{
  if (current_section) {
    if (secstack_index < SECSTACKSIZE)
      secstack[secstack_index++] = current_section;
    else
      general_error(76);  /* section stack overflow */
  }
  else
    general_error(3);  /* no current section */
}

/* pull the top section from the stack and switch to it */
section *pop_section(void)
{
  if (secstack_index > 0)
    set_section(secstack[--secstack_index]);
  else
    general_error(77);  /* section stack empty */
  return current_section;
}

/* end a relocated ORG block */
int end_rorg(void)
{
  section *s = default_section();

  if (s == NULL) {
    general_error(3);
    return 0;
  }
  if (s->flags & IN_RORG) {
    add_atom(s,new_rorgend_atom());
    if (s->flags & PREVABS)
      s->flags |= ABSOLUTE;
    else
      s->flags &= ~ABSOLUTE;
    s->flags &= ~IN_RORG;
    return 1;
  }
  general_error(44);  /* no Rorg block to end */
  return 0;
}

/* end a relocated ORG block when currently active */
void try_end_rorg(void)
{
  if (current_section!=NULL && (current_section->flags&IN_RORG))
    end_rorg();
}

/* start a relocated ORG block */
void start_rorg(taddr rorg)
{
  section *s = default_section();

  if (s == NULL) {
    general_error(3);
    return;
  }
  if (s->flags & IN_RORG)
    end_rorg();  /* we are already in a ROrg-block, so close it first */
  add_atom(s,new_rorg_atom(rorg));
  s->flags |= IN_RORG;
  if (!(s->flags & ABSOLUTE)) {
    s->flags &= ~PREVABS;
    s->flags |= ABSOLUTE;  /* make section absolute during the ROrg-block */
  }
  else
    s->flags |= PREVABS;
}

void print_section(FILE *f,section *sec)
{
  atom *p;
  taddr pc=sec->org;
  fprintf(f,"section %s (attr=<%s> align=%llu):\n",
          sec->name,sec->attr,ULLTADDR(sec->align));
  for(p=sec->first;p;p=p->next){
    pc=pcalign(p,pc);
    fprintf(f,"%8llx: ",ULLTADDR(pc));
    print_atom(f,p);
    fprintf(f,"\n");
    pc+=atom_size(p,sec,pc);
  }
}
