/* elf_reloc_x86_64.h ELF relocation types for x86 64-bit architecture */
/* (c) in 2011 by Frank Wille */

#define R_X86_64_NONE           0
#define R_X86_64_64             1
#define R_X86_64_PC32           2
#define R_X86_64_GOT32          3
#define R_X86_64_PLT32          4
#define R_X86_64_COPY           5
#define R_X86_64_GLOB_DAT       6
#define R_X86_64_JUMP_SLOT      7
#define R_X86_64_RELATIVE       8
#define R_X86_64_GOTPCREL       9
#define R_X86_64_32             10
#define R_X86_64_32S            11
#define R_X86_64_16             12
#define R_X86_64_PC16           13
#define R_X86_64_8              14
#define R_X86_64_PC8            15

/* TLS relocations */
#define R_X86_64_DTPMOD64       16
#define R_X86_64_DTPOFF64       17
#define R_X86_64_TPOFF64        18
#define R_X86_64_TLSGD          19
#define R_X86_64_TLSLD          20
#define R_X86_64_DTPOFF32       21
#define R_X86_64_GOTTPOFF       22
#define R_X86_64_TPOFF32        23


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
          if (size == 64)
            t = R_X86_64_64;
          else if (size == 32)
            t = R_X86_64_32;
          else if (size == 16)
            t = R_X86_64_16;
          else if (size == 8)
            t = R_X86_64_8;
        }
        break;

      case REL_PC:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_X86_64_PC32;
          else if (size == 16)
            t = R_X86_64_PC16;
          else if (size == 8)
            t = R_X86_64_PC8;
        }
        break;

      case REL_GOT:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_X86_64_GOT32;
        }
        break;

      case REL_GOTPC:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_X86_64_GOTPCREL;
        }
        break;

      case REL_PLT:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_X86_64_PLT32;
        }
        break;

    }
  }
