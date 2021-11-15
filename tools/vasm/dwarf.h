/* dwarf.h - DWARF debugging sections */
/* (c) in 2018 by Frank Wille */


struct dwarf_info {
  int version;
  char *producer;
  section *asec;
  void *range_length;
  section *lsec;
  void *line_length;
  section *rsec;
  int code_sections;
  atom *lowpc_atom,*highpc_atom;
  unsigned addr_len;
  unsigned min_inst_len;
  unsigned char default_is_stmt;
  signed char line_base;
  unsigned char line_range;
  unsigned char opcode_base;
  int max_pcadvance;
  int max_lnadvance_hipc;
  taddr address;
  int file,line,column,is_stmt,basic_block,end_sequence;
};

/* debug informations tags and attributes */
#define DW_TAG_compile_unit     0x11

#define DW_AT_name              0x03
#define DW_AT_stmt_list         0x10
#define DW_AT_low_pc            0x11
#define DW_AT_high_pc           0x12
#define DW_AT_language          0x13
#define DW_AT_comp_dir          0x1b
#define DW_AT_producer          0x25
#define DW_AT_ranges            0x55  /* DWARF3 */

/* debug abbreviation forms */
#define DW_FORM_addr            1
#define DW_FORM_data2           5
#define DW_FORM_data4           6
#define DW_FORM_string          8

/* DW_AT_language */
#define DW_LANG_C89             1
#define DW_LANG_C               2
#define DW_LANG_ASSEMBLER       0x8001 /* originally def. as MIPS assembler */

/* statement machine opcodes */
#define DW_LNS_copy             1
#define DW_LNS_advance_pc       2
#define DW_LNS_advance_line     3
#define DW_LNS_set_file         4
#define DW_LNS_const_add_pc     8

#define DW_LNE_end_sequence     1
#define DW_LNE_set_address      2


/* functions */
void dwarf_init(struct dwarf_info *,struct include_path *,struct source_file *);
void dwarf_finish(struct dwarf_info *);
void dwarf_end_sequence(struct dwarf_info *,section *);
void dwarf_line(struct dwarf_info *,section *,int,int);
