/* elf_reloc_jag.h ELF relocation types for Jaguar RISC */
/* (c) in 2015 by Frank Wille */

#define R_JAG_NONE 0      /* No reloc */
#define R_JAG_ABS32 1     /* Direct 32 bit */
#define R_JAG_ABS16 2     /* Direct 16 bit */
#define R_JAG_ABS8 3      /* Direct 8 bit */
#define R_JAG_REL32 4     /* PC relative 32 bit */
#define R_JAG_REL16 5     /* PC relative 16 bit */
#define R_JAG_REL8 6      /* PC relative 8 bit */
#define R_JAG_ABS5 7      /* Direct 5 bit */
#define R_JAG_REL5 8      /* PC relative 5 bit */
#define R_JAG_JR 9        /* PC relative branch (distance / 2), 5 bit */
#define R_JAG_ABS32SWP 10 /* 32 bit direct, halfwords swapped as in MOVEI */
#define R_JAG_REL32SWP 11 /* 32 bit PC rel., halfwords swapped as in MOVEI */

#define RFLD(p,s) (BIGENDIAN?(size==(s)&&(pos&15)==(p)):(size==(s)&&(pos&15)==(16-(p)-(s))))


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
            t = R_JAG_ABS32;
          else if (size == 16)
            t = R_JAG_ABS16;
          else if (size == 8)
            t = R_JAG_ABS8;
        }
        else if (RFLD(6,5) && mask==0x1f)
          t = R_JAG_ABS5;
        else if (RFLD(0,16) && (mask==0xffff || mask==0xffff0000)
                 && (rl2=(*rl)->next)!=NULL
                 && (*rl)->type==(*rl)->next->type) {
          nreloc *r2 = (nreloc *)rl2->reloc;
          if ((mask==0xffff0000 && r2->mask==0xffff && r2->size==16
              && r2->bitoffset==0) ||
              (mask==0xffff && r2->mask==0xffff0000 && r2->size==16
              && r2->bitoffset==16)) {
            t = R_JAG_ABS32SWP;
            *rl = (*rl)->next;
          }
        }
        break;

      case REL_PC:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_JAG_REL32;
          else if (size == 16)
            t = R_JAG_REL16;
          else if (size == 8)
            t = R_JAG_REL8;
        }
        else if (RFLD(6,5) && mask==0x1f)
          t = R_JAG_REL5;
        else if (RFLD(6,5) && mask==0x3e)
          t = R_JAG_JR;
        else if (RFLD(0,16) && (mask==0xffff || mask==0xffff0000)
                 && (rl2=(*rl)->next)!=NULL
                 && (*rl)->type==(*rl)->next->type) {
          nreloc *r2 = (nreloc *)rl2->reloc;
          if ((mask==0xffff0000 && r2->mask==0xffff && r2->size==16
              && r2->bitoffset==0) ||
              (mask==0xffff && r2->mask==0xffff0000 && r2->size==16
              && r2->bitoffset==16)) {
            t = R_JAG_REL32SWP;
            *rl = (*rl)->next;
          }
        }
        break;
    }
  }

#undef RFLD
