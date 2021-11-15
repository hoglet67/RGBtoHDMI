/* elf_reloc_arm.h ELF relocation types for ARM */
/* (c) in 2004,2016 by Frank Wille */

#define R_ARM_NONE 0
#define R_ARM_PC24 1
#define R_ARM_ABS32 2
#define R_ARM_REL32 3
#define R_ARM_PC13 4
#define R_ARM_ABS16 5
#define R_ARM_ABS12 6
#define R_ARM_THM_ABS5 7
#define R_ARM_ABS8 8
#define R_ARM_SBREL32 9
#define R_ARM_THM_PC22 10
#define R_ARM_THM_PC8 11
#define R_ARM_SWI24 13
#define R_ARM_THM_SWI8 14
#define R_ARM_ALU_PCREL_7_0 32
#define R_ARM_ALU_PCREL_15_8 33
#define R_ARM_ALU_PCREL_23_15 34
#define R_ARM_LDR_SBREL_11_0 35
#define R_ARM_LDR_SBREL_19_12 36
#define R_ARM_LDR_SBREL_27_20 37
#define R_ARM_RELABS32 38
#define R_ARM_ROSEGREL32 39
#define R_ARM_V4BX 40

#define AFLD(p,s) (BIGENDIAN?(size==(s)&&(pos&31)==(p)):(size==(s)&&(pos&31)==(32-(p)-(s))))
#define TFLD(p,s) (BIGENDIAN?(size==(s)&&(pos&15)==(p)):(size==(s)&&(pos&15)==(16-(p)-(s))))


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
            t = R_ARM_ABS32;
          else if (size == 16)
            t = R_ARM_ABS16;
          else if (size == 8)
            t = R_ARM_ABS8;
        }
        else if (AFLD(8,24) && mask==0xffffff)
          t = R_ARM_SWI24;
        else if (AFLD(20,12) && mask==0xfff)
          t = R_ARM_ABS12;
        else if (TFLD(8,8) && mask==0xff)
          t = R_ARM_THM_SWI8;
        else if (TFLD(5,5) && mask==0x1f)
          t = R_ARM_THM_ABS5;
        break;

      case REL_PC:
        if (size==32 && pos==0 && mask==~0)
          t = R_ARM_REL32;
        else if (AFLD(8,24) && mask==0x3fffffc)
          t = R_ARM_PC24;
        else if (AFLD(20,12) && mask==0x1fff)
          t = R_ARM_PC13;
        else if (AFLD(24,8) && mask==0xff)
          t = R_ARM_ALU_PCREL_7_0;
        else if (AFLD(24,8) && mask==0xff00)
          t = R_ARM_ALU_PCREL_15_8;
        else if (AFLD(24,8) && mask==0xff0000)
          t = R_ARM_ALU_PCREL_23_15;
        else if (TFLD(8,8) && mask==0x3fc)
          t = R_ARM_THM_PC8;
        else if (TFLD(5,11)) {
          if (rl2 = (*rl)->next) {
            nreloc *r2 = (nreloc *)rl2->reloc;
            if (rl2->type==(*rl)->type
                && r2->size==size && (r2->bitoffset&15)==(pos&15)) {
              if ((mask==0x7ff000 && r2->mask==0xffe) ||
                  (mask==0xffe && r2->mask==0x7ff000)) {
                t = R_ARM_THM_PC22;
                *rl = (*rl)->next;
              }
            }
          }
        }
        break;
    }
  }

#undef AFLD
#undef TFLD
