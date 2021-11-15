/* output_tos.h header file for Atari TOS format */
/* (c) in 2009,2012,2020 by Frank Wille */


/* TOS program header */
typedef struct
{
  uint8_t ph_branch[2];  /* branch to start of program (0x601a) */
  uint8_t ph_tlen[4];    /* .text length */
  uint8_t ph_dlen[4];    /* .data length */
  uint8_t ph_blen[4];    /* .bss length */
  uint8_t ph_slen[4];    /* length of symbol table */
  uint8_t ph_magic[4];
  uint8_t ph_flags[4];   /* Atari special flags */
  uint8_t ph_abs[2];     /* has to be 0, otherwise no relocation takes place */
} PH;


/* DRI symbol table */
#define DRI_NAMELEN 8

struct DRIsym
{
  char name[DRI_NAMELEN];
  uint8_t type[2];
  uint8_t value[4];
};

#define STYP_UNDEF 0
#define STYP_BSS 0x0100
#define STYP_TEXT 0x0200
#define STYP_DATA 0x0400
#define STYP_EXTERNAL 0x0800
#define STYP_REGISTER 0x1000
#define STYP_GLOBAL 0x2000
#define STYP_EQUATED 0x4000
#define STYP_DEFINED 0x8000
#define STYP_LONGNAME 0x0048
#define STYP_TFILE 0x0280
#define STYP_TFARC 0x02c0
