/* source.c - source files, include paths and dependencies */
/* (c) in 2020 by Volker Barthelmann and Frank Wille */

#include "vasm.h"
#include "osdep.h"
#include "dwarf.h"

#define SRCREADINC (64*1024)  /* extend buffer in these steps when reading */

char *compile_dir;
int ignore_multinc,depend,depend_all;

static struct include_path *first_incpath;
static struct source_file *first_source;
static struct deplist *first_depend,*last_depend;


void source_debug_init(int type,void *data)
{
  if (type) {
    /* @@@ currently we only support DWARF source level debugging here */
    dwarf_init((struct dwarf_info *)data,first_incpath,first_source);
  }
}


static void add_depend(char *name)
{
  if (depend) {
    struct deplist *d = first_depend;

    /* check if an entry with the same file name already exists */
    while (d != NULL) {
      if (!strcmp(d->filename,name))
        return;
      d = d->next;
    }

    /* append new dependency record */
    d = mymalloc(sizeof(struct deplist));
    d->next = NULL;
    if (name[0]=='.'&&(name[1]=='/'||name[1]=='\\'))
      name += 2;  /* skip "./" in paths */
    d->filename = mystrdup(name);
    if (last_depend)
      last_depend = last_depend->next = d;
    else
      first_depend = last_depend = d;
  }
}


void write_depends(FILE *f)
{
  struct deplist *d = first_depend;

  if (depend==DEPEND_MAKE && d!=NULL && outname!=NULL)
    fprintf(f,"%s:",outname);

  while (d != NULL) {
    switch (depend) {
      case DEPEND_LIST:
        fprintf(f,"%s\n",d->filename);
        break;
      case DEPEND_MAKE:
        if (str_is_graph(d->filename))
          fprintf(f," %s",d->filename);
        else
          fprintf(f," \"%s\"",d->filename);
        break;
      default:
        ierror(0);
    }
    d = d->next;
  }

  if (depend == DEPEND_MAKE)
    fputc('\n',f);
}


static FILE *open_path(char *compdir,char *path,char *name,char *mode)
{
  char pathbuf[MAXPATHLEN];
  FILE *f;

  if (strlen(compdir) + strlen(path) + strlen(name) + 1 <= MAXPATHLEN) {
    strcpy(pathbuf,compdir);
    strcat(pathbuf,path);
    strcat(pathbuf,name);

    if (f = fopen(pathbuf,mode)) {
      if (depend_all || !abs_path(pathbuf))
        add_depend(pathbuf);
      return f;
    }
  }
  return NULL;
}


static FILE *locate_file(char *filename,char *mode,struct include_path **ipath_used)
{
  struct include_path *ipath;
  FILE *f;

  if (abs_path(filename)) {
    /* file name is absolute, then don't use any include paths */
    if (f = fopen(filename,mode)) {
      if (depend_all)
        add_depend(filename);
      if (ipath_used)
        *ipath_used = NULL;  /* no path used, file name was absolute */
      return f;
    }
  }
  else {
    /* locate file name in all known include paths */
    for (ipath=first_incpath; ipath; ipath=ipath->next) {
      if ((f = open_path(emptystr,ipath->path,filename,mode)) == NULL) {
        if (compile_dir && !abs_path(ipath->path) &&
            (f = open_path(compile_dir,ipath->path,filename,mode)))
          ipath->compdir_based = 1;
      }
      if (f != NULL) {
        if (ipath_used)
          *ipath_used = ipath;
        return f;
      }
    }
  }
  general_error(12,filename);
  return NULL;
}


/* create a new source text instance, which has cur_src as parent */
source *new_source(char *srcname,struct source_file *srcfile,
                   char *text,size_t size)
{
  static unsigned long id = 0;
  source *s = mymalloc(sizeof(source));
  size_t i;
  char *p;

  /* scan the source for strange characters */
  for (p=text,i=0; i<size; i++,p++) {
    if (*p == 0x1a) {
      /* EOF character - replace by newline and ignore rest of source */
      *p = '\n';
      size = i + 1;
      break;
    }
  }

  s->parent = cur_src;
  s->parent_line = cur_src ? cur_src->line : 0;
  s->srcfile = srcfile; /* NULL for macros and repetitions */
  s->name = mystrdup(srcname);
  s->text = text;
  s->size = size;
  s->defsrc = NULL;
  s->defline = 0;
  s->srcdebug = cur_src ? cur_src->srcdebug : 1;  /* source-level debugging */
  s->macro = NULL;
  s->repeat = 1;        /* read just once */
  s->irpname = NULL;
  s->cond_level = clev; /* remember level of conditional nesting */
  s->num_params = -1;   /* not a macro, no parameters */
  s->param[0] = emptystr;
  s->param_len[0] = 0;
  s->id = id++;	        /* every source has unique id - important for macros */
  s->srcptr = text;
  s->line = 0;
  s->bufsize = INITLINELEN;
  s->linebuf = mymalloc(INITLINELEN);
#ifdef CARGSYM
  s->cargexp = NULL;
#endif
#ifdef REPTNSYM
  /* -1 outside of a repetition block */
  s->reptn = cur_src ? cur_src->reptn : -1;
#endif
  return s;
}


/* quit parsing the current source instance, leave macros, repeat loops
   and restore the conditional assembly level */
void end_source(source *s)
{
  if(s){
    s->srcptr=s->text+s->size;
    s->repeat=1;
    clev=s->cond_level;
  }
}


source *include_source(char *inc_name)
{
  static int srcfileidx;
  char *filename;
  struct source_file **nptr = &first_source;
  struct source_file *srcfile;
  source *newsrc = NULL;
  FILE *f;

  filename = convert_path(inc_name);

  /* check whether this source file name was already included */
  while (srcfile = *nptr) {
    if (!filenamecmp(srcfile->name,filename)) {
      myfree(filename);
      nptr = NULL;  /* reuse existing source in memory */
      break;
    }
    nptr = &srcfile->next;
  }

  if (nptr != NULL) {
    /* allocate, locate and read a new source file */
    struct include_path *ipath;

    if (f = locate_file(filename,"r",&ipath)) {
      char *text;
      size_t size;

      for (text=NULL,size=0; ; size+=SRCREADINC) {
        size_t nchar;
        text = myrealloc(text,size+SRCREADINC);
        nchar = fread(text+size,1,SRCREADINC,f);
        if (nchar < SRCREADINC) {
          size += nchar;
          break;
        }
      }
      if (feof(f)) {
        if (size > 0) {
          text = myrealloc(text,size+2);
          *(text+size) = '\n';
          *(text+size+1) = '\0';
          size++;
        }
        else {
          myfree(text);
          text = "\n";
          size = 1;
        }
        srcfile = mymalloc(sizeof(struct source_file));
        srcfile->next = NULL;
        srcfile->name = filename;
        srcfile->incpath = ipath;
        srcfile->text = text;
        srcfile->size = size;
        srcfile->index = ++srcfileidx;
        *nptr = srcfile;
        cur_src = newsrc = new_source(filename,srcfile,text,size);
      }
      else
        general_error(29,filename);
      fclose(f);
    }
  }
  else {
    /* same source was already loaded before, source_file node exists */
    if (ignore_multinc)
      return NULL;  /* ignore multiple inclusion of this source completely */

    /* new source instance from existing source file */
    cur_src = newsrc = new_source(srcfile->name,srcfile,srcfile->text,
                                  srcfile->size);
  }
  return newsrc;
}


void include_binary_file(char *inname,long nbskip,unsigned long nbkeep)
/* locate a binary file and convert into a data atom */
{
  char *filename;
  FILE *f;

  filename = convert_path(inname);
  if (f = locate_file(filename,"rb",NULL)) {
    size_t size = filesize(f);

    if (size > 0) {
      if (nbskip>=0 && (size_t)nbskip<=size) {
        dblock *db = new_dblock();

        if (nbkeep > (unsigned long)(size - (size_t)nbskip) || nbkeep==0)
          db->size = size - (size_t)nbskip;
        else
          db->size = nbkeep;

        db->data = mymalloc(size);
        if (nbskip > 0)
          fseek(f,nbskip,SEEK_SET);

        if (fread(db->data,1,db->size,f) != db->size)
          general_error(29,filename);  /* read error */
        add_atom(0,new_data_atom(db,1));
      }
      else
        general_error(46);  /* bad file-offset argument */
    }
    fclose(f);
  }
  myfree(filename);
}


static struct include_path *new_ipath_node(char *pathname)
{
  struct include_path *new = mymalloc(sizeof(struct include_path));

  new->next = NULL;
  new->path = pathname;
  new->compdir_based = 0;
  return new;
}


static char *make_canonical_path(char *pathname)
{
  char *newpath = convert_path(pathname);

  pathname = append_path_delimiter(newpath);  /* append '/', when needed */
  myfree(newpath);
  return pathname;
}


/* add the main source include path, which is searched first */
void main_include_path(char *pathname)
{
  struct include_path *ipath;

  ipath = new_ipath_node(make_canonical_path(pathname));
  ipath->next = first_incpath;
  first_incpath = ipath;
}


struct include_path *new_include_path(char *pathname)
{
  struct include_path *ipath;

  /* check if path already exists, otherwise append new node */
  pathname = make_canonical_path(pathname);
  for (ipath=first_incpath; ipath; ipath=ipath->next) {
    if (!filenamecmp(pathname,ipath->path)) {
      myfree(pathname);
      return ipath;
    }
    if (ipath->next == NULL)
      return ipath->next = new_ipath_node(pathname);
  }
  return first_incpath = new_ipath_node(pathname);
}
