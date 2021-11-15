/* output_xfile.h header file for the Sharp X68000 Xfile format */
/* (c) in 2018,2020 by Frank Wille */


/* Xfile program header, big endian */
typedef struct
{
  uint8_t x_id[2];        /* 'H','U' - xfile identification */
  uint8_t x_rsrvd1[2];    /* unused - always zero */
  uint8_t x_baseaddr[4];  /* linker's base address */
  uint8_t x_execaddr[4];  /* start address on base address */
  uint8_t x_textsz[4];    /* .text size in bytes */
  uint8_t x_datasz[4];    /* .data size in bytes */
  uint8_t x_heapsz[4];    /* .bss and .stack size in bytes */
  uint8_t x_relocsz[4];   /* relocation table size in bytes */
  uint8_t x_syminfsz[4];  /* symbol info size in bytes */
  uint8_t x_scdlinsz[4];  /* SCD line info size */
  uint8_t x_scdsymsz[4];  /* SCD symbols size */
  uint8_t x_scdstrsz[4];  /* SCD strings size */
  uint8_t x_rsrvd2[20];   /* unused - always zero */
} XFILE;


/* Xfile symbol table */
typedef struct
{
  uint16_t type;
  uint32_t value;
  char name[1];           /* null-terminated symbol name, padded to even */
} Xsym;

#define XSYM_ABS	0x0200
#define XSYM_TEXT       0x0201
#define XSYM_DATA       0x0202
#define XSYM_BSS        0x0203
#define XSYM_STACK      0x0204
