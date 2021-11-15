/* elf_reloc_386.h ELF relocation types for i386 */
/* (c) in 2005 by Frank Wille */

#define R_386_NONE      0
#define R_386_32        1
#define R_386_PC32      2
#define R_386_GOT32     3
#define R_386_PLT32     4
#define R_386_COPY      5
#define R_386_GLOB_DAT  6
#define R_386_JMP_SLOT  7
#define R_386_RELATIVE  8
#define R_386_GOTOFF    9
#define R_386_GOTPC     10
#define R_386_16        20
#define R_386_PC16      21
#define R_386_8         22
#define R_386_PC8       23


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
            t = R_386_32;
          else if (size == 16)
            t = R_386_16;
          else if (size == 8)
            t = R_386_8;
        }
        break;

      case REL_PC:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_386_PC32;
          else if (size == 16)
            t = R_386_PC16;
          else if (size == 8)
            t = R_386_PC8;
        }
        break;

      case REL_GOT:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_386_GOT32;
        }
        break;

      case REL_GOTPC:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_386_GOTPC;
        }
        break;

      case REL_PLT:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_386_PLT32;
        }
        break;

    }
  }
