/* output_hunk.h header file for AmigaOS hunk format */
/* (c) in 2002-2019 by Frank Wille */

/* hunk-format relocs */
struct hunkreloc {
  struct node n;
  rlist *rl;
  uint32_t hunk_id;
  uint32_t hunk_index;
  uint32_t hunk_offset;
};

/* hunk-format external reference */
struct hunkxref {
  struct node n;
  char *name;
  uint32_t type;
  uint32_t size;
  uint32_t offset;
};

/* line debug hunk */
struct linedb_block {
  struct node n;
  char *filename;
  uint32_t offset;
  int entries;
  struct list lines;
};
struct linedb_entry {
  struct node n;
  uint32_t line;
  uint32_t offset;
};


/* additional symbol flags */
#define COMM_REFERENCED (RSRVD_O<<0)  /* common symbol was referenced */

/* additional section flags */
#define SEC_DELETED     (SECRSRVD<<1) /* this section can be deleted */


/* Amiga DOS Hunks */
#define HUNK_UNIT       999
#define HUNK_NAME       1000
#define HUNK_CODE       1001
#define HUNK_DATA       1002
#define HUNK_BSS        1003
#define HUNK_ABSRELOC32 1004
#define HUNK_RELRELOC16 1005
#define HUNK_RELRELOC8  1006
#define HUNK_EXT        1007
#define HUNK_SYMBOL     1008
#define HUNK_DEBUG      1009
#define HUNK_END        1010
#define HUNK_HEADER     1011
#define HUNK_DREL32     1015
#define HUNK_DREL16     1016
#define HUNK_DREL8      1017
#define HUNK_RELRELOC32 1021
#define HUNK_ABSRELOC16 1022

/* EHF extensions */
#define HUNK_PPC_CODE   1257
#define HUNK_RELRELOC26 1260

/* memory type */
#define HUNKB_CHIP      30
#define HUNKB_FAST      31
#define HUNKF_CHIP      (1L<<30)
#define HUNKF_FAST      (1L<<31)
#define HUNKF_MEMTYPE   (HUNKF_CHIP|HUNKF_FAST)

/* AmigaOS memory flags */
#define MEMF_PUBLIC     (1L<<0)
#define MEMF_CHIP       (1L<<1)
#define MEMF_FAST       (1L<<2)

/* hunk_ext sub-types */
#define EXT_SYMB 0
#define EXT_DEF	1
#define EXT_ABS	2
#define EXT_ABSREF32 129
#define EXT_ABSCOMMON 130
#define EXT_RELREF16 131
#define EXT_RELREF8 132
#define EXT_DEXT32 133
#define EXT_DEXT16 134
#define EXT_DEXT8 135
#define EXT_RELREF32 136
#define EXT_RELCOMMON 137
#define EXT_ABSREF16 138
#define EXT_ABSREF8 139

/* EHF extensions */
#define EXT_RELREF26 229
