/* elf_reloc_68k.h ELF relocation types for M68k */
/* (c) in 2002 by Frank Wille */

#define R_68K_NONE 0      /* No reloc */
#define R_68K_32 1        /* Direct 32 bit  */
#define R_68K_16 2        /* Direct 16 bit  */
#define R_68K_8 3         /* Direct 8 bit  */
#define R_68K_PC32 4      /* PC relative 32 bit */
#define R_68K_PC16 5      /* PC relative 16 bit */
#define R_68K_PC8 6       /* PC relative 8 bit */
#define R_68K_GOT32 7     /* 32 bit PC relative GOT entry */
#define R_68K_GOT16 8     /* 16 bit PC relative GOT entry */
#define R_68K_GOT8 9      /* 8 bit PC relative GOT entry */
#define R_68K_GOT32O 10   /* 32 bit GOT offset */
#define R_68K_GOT16O 11   /* 16 bit GOT offset */
#define R_68K_GOT8O 12    /* 8 bit GOT offset */
#define R_68K_PLT32 13    /* 32 bit PC relative PLT address */
#define R_68K_PLT16 14    /* 16 bit PC relative PLT address */
#define R_68K_PLT8 15     /* 8 bit PC relative PLT address */
#define R_68K_PLT32O 16   /* 32 bit PLT offset */
#define R_68K_PLT16O 17   /* 16 bit PLT offset */
#define R_68K_PLT8O 18    /* 8 bit PLT offset */
#define R_68K_COPY 19     /* Copy symbol at runtime */
#define R_68K_GLOB_DAT 20 /* Create GOT entry */
#define R_68K_JMP_SLOT 21 /* Create PLT entry */
#define R_68K_RELATIVE 22 /* Adjust by program base */      


  if ((*rl)->type <= LAST_STANDARD_RELOC) {
    nreloc *r = (nreloc *)(*rl)->reloc;

    *refsym = r->sym;
    *addend = r->addend;
    pos = r->bitoffset;
    size = r->size;
    *roffset = r->byteoffset;
    mask = r->mask;

    switch ((*rl)->type) {

      case REL_ABS:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_68K_32;
          else if (size == 16)
            t = R_68K_16;
          else if (size == 8)
            t = R_68K_8;
        }
        break;

      case REL_PC:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_68K_PC32;
          else if (size == 16)
            t = R_68K_PC16;
          else if (size == 8)
            t = R_68K_PC8;
        }
        break;

      case REL_GOTOFF:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_68K_GOT32O;
          else if (size == 16)
            t = R_68K_GOT16O;
          else if (size == 8)
            t = R_68K_GOT8O;
        }
        break;

      case REL_GOT:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_68K_GOT32;
          else if (size == 16)
            t = R_68K_GOT16;
          else if (size == 8)
            t = R_68K_GOT8;
        }
        break;

      case REL_PLTOFF:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_68K_PLT32O;
          else if (size == 16)
            t = R_68K_PLT16O;
          else if (size == 8)
            t = R_68K_PLT8O;
        }
        break;

      case REL_PLT:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_68K_PLT32;
          else if (size == 16)
            t = R_68K_PLT16;
          else if (size == 8)
            t = R_68K_PLT8;
        }
        break;

    }
  }
