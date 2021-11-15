/* dwarf.c - DWARF debugging sections */
/* (c) in 2018 by Frank Wille */

#include "vasm.h"
#include "dwarf.h"
#include "osdep.h"

struct DWinclude {
  struct DWinclude *next;
  char *name;
};

struct DWfile {
  struct DWfile *next;
  char *name;
  int incidx;
};


static const unsigned char stdopclengths[] = {
  0,1,1,1,1,0,0,0,1
};

static unsigned char dw2_compile_unit_abbrev[] = {
  DW_TAG_compile_unit,
  0,  /* no children */
  DW_AT_producer,DW_FORM_string,
  DW_AT_language,DW_FORM_data2,
  DW_AT_name,DW_FORM_string,
  DW_AT_comp_dir,DW_FORM_string,
  DW_AT_stmt_list,DW_FORM_data4,
  DW_AT_low_pc,DW_FORM_addr,
  DW_AT_high_pc,DW_FORM_addr,
  0,0
};

static unsigned char dw3_compile_unit_abbrev[] = {
  DW_TAG_compile_unit,
  0,  /* no children */
  DW_AT_producer,DW_FORM_string,
  DW_AT_language,DW_FORM_data2,
  DW_AT_name,DW_FORM_string,
  DW_AT_comp_dir,DW_FORM_string,
  DW_AT_stmt_list,DW_FORM_data4,
  DW_AT_ranges,DW_FORM_data4,
  0,0
};

static struct DWinclude *first_dwinc;
static struct DWfile *first_dwfil;


static struct DWinclude *new_dwinc(char *name)
{
  struct DWinclude *new = mymalloc(sizeof(struct DWinclude));

  new->next = NULL;
  new->name = name;
  return new;
}


/* DWARF needs source file lists with the file name part only, but without
   any path in it. Include paths are put into another list. */
static void make_file_lists(struct source_file *first_source)
{
  struct source_file *srcnode;
  struct include_path *incnode;
  struct DWfile *newfil,*dwfil;
  struct DWinclude *dwinc;
  char pathbuf[MAXPATHLEN];
  char *filepart;
  int include_idx = 0;
  int file_idx = 0;
  int i;

  first_dwfil = NULL;
  first_dwinc = NULL;

  for (srcnode=first_source; srcnode; srcnode=srcnode->next) {
    newfil = mymalloc(sizeof(struct DWfile));
    newfil->next = NULL;
    pathbuf[0] = '\0';

    if ((incnode = srcnode->incpath) != NULL) {
      if (incnode->compdir_based) {
        if (compile_dir != NULL)
          strcpy(pathbuf,compile_dir);
        else
          ierror(0);
      }
      strcat(pathbuf,incnode->path);
    }        

    if ((filepart = get_filepart(srcnode->name)) != srcnode->name) {
      /* add path part from source file name */
      size_t len = filepart - srcnode->name;
      char *p = strrchr(pathbuf,'\0');

      memcpy(p,srcnode->name,len);
      *(p+len) = '\0';
    }

    if (pathbuf[0]) {
      char *newpath = remove_path_delimiter(pathbuf);

      /* check if this path already exists in the list, otherwise add it */
      if (first_dwinc) {
        for (i=1,dwinc=first_dwinc; ; i++,dwinc=dwinc->next) {
          if (!filenamecmp(newpath,dwinc->name)) {
            myfree(newpath);
            newfil->incidx = i;  /* use existing include index */
            break;
          }
          if (dwinc->next == NULL) {
            if (include_idx != i)
              ierror(0);
            dwinc->next = new_dwinc(newpath);
            newfil->incidx = ++include_idx;  /* new include index */
            break;
          }
        }
      }
      else {
        first_dwinc = new_dwinc(newpath);
        newfil->incidx = ++include_idx;  /* should be index 1 */
      }
    }
    else
      newfil->incidx = 0;  /* no path, file is in current work directory */

    /* append new file node */
    if (++file_idx != srcnode->index)
      ierror(0);
    newfil->name = filepart;
    if (dwfil = first_dwfil) {
      while (dwfil->next != NULL)
        dwfil = dwfil->next;
      dwfil->next = newfil;
    }
    else
      first_dwfil = newfil;
  }
}


void dwarf_init(struct dwarf_info *dinfo,
                struct include_path *first_incpath,
                struct source_file *first_source)
{
  atom *a,*dinfoatom,*dabbratom,*drangatom,*dlineatom;
  void *lengthptr;
  symbol *tmpsym;
  section *dsec;
  struct DWinclude *dwinc;
  struct DWfile *dwfil;
  int i;

  if (dinfo->version < 2)
    ierror(0);

  /* init dwarf_info for DWARF2 and CPU architecture */
  dinfo->code_sections = 0;
  dinfo->addr_len = bytespertaddr;
  dinfo->min_inst_len = INST_ALIGN;
  dinfo->default_is_stmt = 1;
  dinfo->line_base = -8;
  dinfo->line_range = 16;
  dinfo->opcode_base = sizeof(stdopclengths) + 1;
  dinfo->max_pcadvance = (255 - dinfo->opcode_base) / dinfo->line_range;
  dinfo->max_lnadvance_hipc = 255 - dinfo->max_pcadvance * dinfo->line_range -
                              dinfo->opcode_base;

  dinfo->asec = dsec = new_section(".debug_aranges","r",1);

  /* compilation unit header */
  a = add_data_atom(dsec,4,1,0);        /* total range-table size */
  dinfo->range_length = a->content.db->data;
  add_data_atom(dsec,2,1,dinfo->version);
  dinfoatom = add_data_atom(dsec,4,1,0);/* 32-bit reloc to .debug_info */
  add_data_atom(dsec,1,1,dinfo->addr_len);
  add_data_atom(dsec,1,1,0);            /* segment-descriptor size - unused */

  dsec = new_section(".debug_info","r",1);

  /* make 32-bit reloc for info-start label */
  tmpsym = new_tmplabel(dsec);
  add_extnreloc(&dinfoatom->content.db->relocs,tmpsym,tmpsym->pc,
                REL_ABS,0,32,0);

  /* compilation unit header */
  a = add_data_atom(dsec,4,1,0);        /* total compilation unit size */
  lengthptr = a->content.db->data;
  add_data_atom(dsec,2,1,dinfo->version);
  dabbratom = add_data_atom(dsec,4,1,0);/* 32-bit reloc to .debug_abbrev */
  add_data_atom(dsec,1,1,dinfo->addr_len);

  /* first DIE, using abbrev. no. 1 */
  add_leb128_atom(dsec,1);

  /* DW_TAG_compile_unit */
  add_bytes_atom(dsec,dinfo->producer,strlen(dinfo->producer));
  add_data_atom(dsec,1,1,' ');
  add_string_atom(dsec,cpuname);        /* vasm version and cpu-name */
  add_data_atom(dsec,2,1,DW_LANG_ASSEMBLER);
  add_string_atom(dsec,getdebugname());
  add_string_atom(dsec,get_workdir());

  dlineatom = add_data_atom(dsec,4,1,0);/* 32-bit reloc to .debug_line */

  if (dinfo->version < 3) {
    /* DWARF2:
       low_pc/high_pc does not work for multiple sections, so the workaround
       is to set low_pc=0 and high_pc=~0 to cover the whole address space. */
    dinfo->lowpc_atom = add_data_atom(dsec,dinfo->addr_len,1,0);
    dinfo->highpc_atom = add_data_atom(dsec,dinfo->addr_len,1,~0);
  }
  else {
    /* address ranges defined in .debug_ranges since DWARF3 */
    drangatom = add_data_atom(dsec,4,1,0);/* 32-bit reloc to .debug_ranges */
    dinfo->lowpc_atom = dinfo->highpc_atom = NULL;
  }

  add_leb128_atom(dsec,0);              /* no more DIEs */
  setval(BIGENDIAN,lengthptr,4,dsec->pc-4);

  dsec = new_section(".debug_abbrev","r",1);

  /* make 32-bit reloc for abbreviations-start label */
  tmpsym = new_tmplabel(dsec);
  add_extnreloc(&dabbratom->content.db->relocs,tmpsym,tmpsym->pc,
                REL_ABS,0,32,0);

  add_leb128_atom(dsec,1);              /* abbrev. no. 1 */
  if (dinfo->version < 3)
    add_bytes_atom(dsec,dw2_compile_unit_abbrev,
                   sizeof(dw2_compile_unit_abbrev));
  else
    add_bytes_atom(dsec,dw3_compile_unit_abbrev,
                   sizeof(dw3_compile_unit_abbrev));
  add_leb128_atom(dsec,0);              /* end of abbreviations */

  if (dinfo->version >= 3) {
    dinfo->rsec = dsec = new_section(".debug_ranges","r",2*dinfo->addr_len);

    /* make 32-bit reloc for ranges-start label */
    tmpsym = new_tmplabel(dsec);
    add_extnreloc(&drangatom->content.db->relocs,tmpsym,tmpsym->pc,
                  REL_ABS,0,32,0);

    /* @@@ highest and lowest possible address in first entry? */
    add_data_atom(dsec,dinfo->addr_len,dinfo->addr_len,-1);
    add_data_atom(dsec,dinfo->addr_len,dinfo->addr_len,0);
  }
  else
    dinfo->rsec = NULL;

  dinfo->lsec = dsec = new_section(".debug_line","r",1);

  /* make 32-bit reloc for linedebug-start label */
  tmpsym = new_tmplabel(dsec);
  add_extnreloc(&dlineatom->content.db->relocs,tmpsym,tmpsym->pc,
                REL_ABS,0,32,0);

  /* make .debug_line prologue */
  a = add_data_atom(dsec,4,1,0);        /* line debug size for comp.unit */
  dinfo->line_length = a->content.db->data;
  add_data_atom(dsec,2,1,dinfo->version);
  a = add_data_atom(dsec,4,1,0);        /* byte-offset to statement program */
  lengthptr = a->content.db->data;
  add_data_atom(dsec,1,1,dinfo->min_inst_len);
  add_data_atom(dsec,1,1,dinfo->default_is_stmt);
  add_data_atom(dsec,1,1,dinfo->line_base);
  add_data_atom(dsec,1,1,dinfo->line_range);
  add_data_atom(dsec,1,1,dinfo->opcode_base);

  /* define standard opcode lengths */
  for (i=0; i<sizeof(stdopclengths); i++)
    add_data_atom(dsec,1,1,stdopclengths[i]);

  /* make lists of DWARF directories and files */
  make_file_lists(first_source);

  /* list of include directories, CWD-entry (index 0) is not written */
  for (dwinc=first_dwinc; dwinc; dwinc=dwinc->next)
    add_string_atom(dsec,dwinc->name);
  add_data_atom(dsec,1,1,0);  /* list is terminated by a 0-byte */

  /* list of file names, with directory-index, last-modif.-time and size */
  for (dwfil=first_dwfil; dwfil; dwfil=dwfil->next) {
    add_string_atom(dsec,dwfil->name);
    add_leb128_atom(dsec,dwfil->incidx);
    add_leb128_atom(dsec,0);  /* time */
    add_leb128_atom(dsec,0);  /* size */
  }
  add_data_atom(dsec,1,1,0);  /* list is terminated by a 0-byte */

  /* start of statement program */
  setval(BIGENDIAN,lengthptr,4,dsec->pc-10);
  dinfo->end_sequence = 1;  /* we have to start a new sequence */
}


void dwarf_finish(struct dwarf_info *dinfo)
{
  /* close .debug_aranges table with two NULL-entries and set its size */
  add_data_atom(dinfo->asec,dinfo->addr_len,dinfo->addr_len,0);
  add_data_atom(dinfo->asec,dinfo->addr_len,dinfo->addr_len,0);
  setval(BIGENDIAN,dinfo->range_length,4,dinfo->asec->pc-4);

  if (dinfo->rsec) {
    /* close .debug_ranges table with two NULL-entries */
    add_data_atom(dinfo->rsec,dinfo->addr_len,dinfo->addr_len,0);
    add_data_atom(dinfo->rsec,dinfo->addr_len,dinfo->addr_len,0);
  }

  /* set .debug_line compilation unit size */
  setval(BIGENDIAN,dinfo->line_length,4,dinfo->lsec->pc-4);
}


static void dwarf_set_address(struct dwarf_info *dinfo,symbol *sym)
{
  static unsigned char opcode[3] = { 0,0,DW_LNE_set_address };
  atom *a;

  /* extended opcode to set address for current cpu including relocation */
  opcode[1] = dinfo->addr_len + 1;
  add_bytes_atom(dinfo->lsec,opcode,3);
  a = add_data_atom(dinfo->lsec,dinfo->addr_len,1,sym->pc);
  add_extnreloc(&a->content.db->relocs,sym,sym->pc,REL_ABS,
                0,dinfo->addr_len<<3,0);
}


static void set_atom_label(atom *a,size_t addrlen,symbol *sym)
{
  setval(BIGENDIAN,a->content.db->data,addrlen,sym->pc);
  add_extnreloc(&a->content.db->relocs,sym,sym->pc,REL_ABS,0,addrlen<<3,0);
}


static void set_atom_val(atom *a,size_t len,taddr val)
{
  setval(BIGENDIAN,a->content.db->data,len,val);
  a->content.db->relocs = NULL;  /* clear relocs */
}


void dwarf_end_sequence(struct dwarf_info *dinfo,section *sec)
{
  if (!dinfo->end_sequence) {
    static unsigned char opcode[3] = { 0,1,DW_LNE_end_sequence };
    symbol *sym = new_tmplabel(sec);  /* label at end of section */
    atom *a;

    dwarf_set_address(dinfo,sym);
    add_bytes_atom(dinfo->lsec,opcode,3);
    dinfo->end_sequence = 1;

    /* enter section size for this sequence into the address-range table */
    add_data_atom(dinfo->asec,dinfo->addr_len,dinfo->addr_len,sec->pc);

    if (dinfo->highpc_atom) {
      if (dinfo->code_sections > 1)
        /* low_pc=0, high_pc=~0 for multiple sections as workaround */
        set_atom_val(dinfo->highpc_atom,dinfo->addr_len,~0);
      else
        /* we can set high_pc, when there is just a single code section */
        set_atom_label(dinfo->highpc_atom,dinfo->addr_len,sym);
    }

    if (dinfo->rsec) {
      /* enter end-of-section label reference into the ranges table */
      a = add_data_atom(dinfo->rsec,dinfo->addr_len,dinfo->addr_len,sym->pc);
      add_extnreloc(&a->content.db->relocs,sym,sym->pc,REL_ABS,
                    0,dinfo->addr_len<<3,0);
    }
  }
}


void dwarf_line(struct dwarf_info *dinfo,section *sec,int file,int line)
{
  if (file==0 || line==0)
    ierror(0);

  if (dinfo->end_sequence) {
    symbol *sym;
    atom *a;

    /* start a new sequence, reset state machine */
    dinfo->code_sections++;
    dinfo->file = 1;
    dinfo->line = 1;
    dinfo->column = 0;
    dinfo->is_stmt = dinfo->default_is_stmt;
    dinfo->basic_block = 0;
    dinfo->end_sequence = 0;

    /* set label at start of the instruction's section */
    sym = new_tmplabel(sec);
    sym->pc = 0;

    /* make new entry into the address-range table: .debug_aranges */
    a = new_space_atom(number_expr(0),1,NULL);
    a->align = 2 * dinfo->addr_len;
    add_atom(dinfo->asec,a);  /* align to double address-length */
    a = add_data_atom(dinfo->asec,dinfo->addr_len,dinfo->addr_len,0);
    add_extnreloc(&a->content.db->relocs,sym,sym->pc,REL_ABS,
                  0,dinfo->addr_len<<3,0);

    if (dinfo->lowpc_atom) {
      if (dinfo->code_sections > 1)
        /* low_pc=0, high_pc=~0 for multiple sections as workaround */
        set_atom_val(dinfo->lowpc_atom,dinfo->addr_len,0);
      else
        /* we can set low_pc, when there is just a single code section */
        set_atom_label(dinfo->lowpc_atom,dinfo->addr_len,sym);
    }

    if (dinfo->rsec) {
      /* make new entry into the ranges table: .debug_ranges */
      a = add_data_atom(dinfo->rsec,dinfo->addr_len,dinfo->addr_len,0);
      add_extnreloc(&a->content.db->relocs,sym,sym->pc,REL_ABS,
                    0,dinfo->addr_len<<3,0);
    }

    /* set relocatable address of first instruction, then advance line, etc.*/
    dinfo->address = sec->pc;
    dwarf_set_address(dinfo,new_tmplabel(sec));

    if (file != dinfo->file) {
      add_data_atom(dinfo->lsec,1,1,DW_LNS_set_file);
      add_leb128_atom(dinfo->lsec,file);
      dinfo->file = file;
    }
    if (line != dinfo->line) {
      add_data_atom(dinfo->lsec,1,1,DW_LNS_advance_line);
      add_leb128_atom(dinfo->lsec,line-dinfo->line);
      dinfo->line = line;
    }
    add_data_atom(dinfo->lsec,1,1,DW_LNS_copy);
  }
  else {
    int lineoffs = line - dinfo->line;
    int instoffs = (sec->pc - dinfo->address) / dinfo->min_inst_len;

    if (file != dinfo->file) {
      add_data_atom(dinfo->lsec,1,1,DW_LNS_set_file);
      add_leb128_atom(dinfo->lsec,file);
      dinfo->file = file;
    }

    if (instoffs > dinfo->max_pcadvance) {
      if (instoffs - dinfo->max_pcadvance <= dinfo->max_pcadvance) {
        /* const_add_pc for up to twice the maximum special opcode advance */
        add_data_atom(dinfo->lsec,1,1,DW_LNS_const_add_pc);
        instoffs -= dinfo->max_pcadvance;
      }
      else {
        /* advance address by standard opcode */
        add_data_atom(dinfo->lsec,1,1,DW_LNS_advance_pc);
        add_leb128_atom(dinfo->lsec,instoffs);
        instoffs = 0;
      }
    }

    if (lineoffs < dinfo->line_base ||
        lineoffs >= dinfo->line_base + dinfo->line_range ||
        (instoffs == dinfo->max_pcadvance &&
         lineoffs > dinfo->line_base + dinfo->max_lnadvance_hipc)) {
      /* we have to advance line by standard opcode */
      add_data_atom(dinfo->lsec,1,1,DW_LNS_advance_line);
      add_sleb128_atom(dinfo->lsec,lineoffs);
      lineoffs = 0;
    }

    /* construct special opcode for simultaneous inst./pc-advancement */
    add_data_atom(dinfo->lsec,1,1,
                  dinfo->opcode_base +
                  instoffs*dinfo->line_range +
                  (lineoffs-dinfo->line_base));
    /* update line/address */
    dinfo->address = sec->pc;
    dinfo->line = line;
  }
}
