/* elf_reloc_ppc.h ELF relocation types for PowerPC */
/* (c) in 2002-2008,2016 by Frank Wille */

#define R_PPC_NONE 0
#define R_PPC_ADDR32 1
#define R_PPC_ADDR24 2
#define R_PPC_ADDR16 3
#define R_PPC_ADDR16_LO 4
#define R_PPC_ADDR16_HI 5
#define R_PPC_ADDR16_HA 6
#define R_PPC_ADDR14 7
#define R_PPC_ADDR14_BRTAKEN 8
#define R_PPC_ADDR14_BRNTAKEN 9
#define R_PPC_REL24 10
#define R_PPC_REL14 11
#define R_PPC_REL14_BRTAKEN 12
#define R_PPC_REL14_BRNTAKEN 13
#define R_PPC_GOT16 14
#define R_PPC_GOT16_LO 15
#define R_PPC_GOT16_HI 16
#define R_PPC_GOT16_HA 17
#define R_PPC_PLTREL24 18
#define R_PPC_COPY 19
#define R_PPC_GLOB_DAT 20
#define R_PPC_JMP_SLOT 21
#define R_PPC_RELATIVE 22
#define R_PPC_LOCAL24PC 23
#define R_PPC_UADDR32 24
#define R_PPC_UADDR16 25
#define R_PPC_REL32 26
#define R_PPC_PLT32 27
#define R_PPC_PLTREL32 28
#define R_PPC_PLT16_LO 29
#define R_PPC_PLT16_HI 30
#define R_PPC_PLT16_HA 31
#define R_PPC_SDAREL16 32
#define R_PPC_SECTOFF 33
#define R_PPC_SECTOFF_LO 34
#define R_PPC_SECTOFF_HI 35
#define R_PPC_SECTOFF_HA 36
#define R_PPC_ADDR30 37
#define R_PPC_EMB_NADDR32 101
#define R_PPC_EMB_NADDR16 102
#define R_PPC_EMB_NADDR16_LO 103
#define R_PPC_EMB_NADDR16_HI 104
#define R_PPC_EMB_NADDR16_HA 105
#define R_PPC_EMB_SDAI16 106
#define R_PPC_EMB_SDA2I16 107
#define R_PPC_EMB_SDA2REL 108
#define R_PPC_EMB_SDA21 109
#define R_PPC_EMB_MRKREF 110
#define R_PPC_EMB_RELSEC16 111
#define R_PPC_EMB_RELST_LO 112
#define R_PPC_EMB_RELST_HI 113
#define R_PPC_EMB_RELST_HA 114
#define R_PPC_EMB_BIT_FLD 115
#define R_PPC_EMB_RELSDA 116
#define R_PPC_MORPHOS_DREL 200
#define R_PPC_MORPHOS_DREL_LO 201
#define R_PPC_MORPHOS_DREL_HI 202
#define R_PPC_MORPHOS_DREL_HA 203
#define R_PPC_AMIGAOS_BREL 210
#define R_PPC_AMIGAOS_BREL_LO 211
#define R_PPC_AMIGAOS_BREL_HI 212
#define R_PPC_AMIGAOS_BREL_HA 213
#define R_PPC_GNU_VTINHERIT 253
#define R_PPC_GNU_VTENTRY 254
#define R_PPC_TOC16 255


  if ((*rl)->type <= LAST_PPC_RELOC) {
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
            t = R_PPC_ADDR32;
          else if (size == 16)
            t = R_PPC_ADDR16;
        }
        else if (size==30 && pos==0 && mask==~3)
          t = R_PPC_ADDR30;
        else if (size==24 && pos==6 && mask==0x3fffffc)
          t = R_PPC_ADDR24;
        else if (size==14 && pos==0 && mask==0xfffc)
          t = R_PPC_ADDR14;
        else if (size==16 && pos==0) {
          if (mask == 0x0000ffff)
            t = R_PPC_ADDR16_LO;
          else if (mask == 0xffff0000) {
            if (rl2 = (*rl)->next) {
              nreloc *r2 = (nreloc *)rl2->reloc;
              if (rl2->type==(*rl)->type && r2->byteoffset==*roffset &&
                  r2->bitoffset==pos && r2->size==size && r2->mask==0x8000) {
                t = R_PPC_ADDR16_HA;
                *rl = (*rl)->next;
              }
            }
            else
              t = R_PPC_ADDR16_HI;
          }
        }
        break;

      case REL_PC:
        if (size==32 && pos==0 && mask==~0)
          t = R_PPC_REL32;
        else if (size==24 && pos==6 && mask==0x3fffffc)
          t = R_PPC_REL24;
        else if (size==14 && pos==0 && mask==0xfffc)
          t = R_PPC_REL14;
        break;

      case REL_GOT:
        if (size==16 && pos==0 && mask==~0)
          t = R_PPC_GOT16;
        else if (size==16 && pos==0) {
          if (mask == 0x0000ffff)
            t = R_PPC_GOT16_LO;
          else if (mask == 0xffff0000) {
            if (rl2 = (*rl)->next) {
              nreloc *r2 = (nreloc *)rl2->reloc;
              if (rl2->type==(*rl)->type && r2->byteoffset==*roffset &&
                  r2->bitoffset==pos && r2->size==size && r2->mask==0x8000) {
                t = R_PPC_GOT16_HA;
                *rl = (*rl)->next;
              }
            }
            else
              t = R_PPC_GOT16_HI;
          }
        }
        break;

      case REL_PLT:
        if (size==32 && pos==0 && mask==~0)
          t = R_PPC_PLT32;
        else if (size==16 && pos==0) {
          if (mask == 0x0000ffff)
            t = R_PPC_PLT16_LO;
          else if (mask == 0xffff0000) {
            if (rl2 = (*rl)->next) {
              nreloc *r2 = (nreloc *)rl2->reloc;
              if (rl2->type==(*rl)->type && r2->byteoffset==*roffset &&
                  r2->bitoffset==pos && r2->size==size && r2->mask==0x8000) {
                t = R_PPC_PLT16_HA;
                *rl = (*rl)->next;
              }
            }
            else
              t = R_PPC_PLT16_HI;
          }
        }
        break;

      case REL_PLTPC:
        if (size==32 && pos==0 && mask==~0)
          t = R_PPC_PLTREL32;
        else if (size==24 && pos==6 && mask==0x3fffffc)
          t = R_PPC_PLTREL24;
        break;

      case REL_SD:
        if (size==16 && pos==0 && mask==~0)
          t = R_PPC_SDAREL16;
        break;

      case REL_LOCALPC:
        if (size==24 && pos==6 && mask==0x3fffffc)
          t = R_PPC_LOCAL24PC;
        break;

      case REL_UABS:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_PPC_UADDR32;
          else if (size == 16)
            t = R_PPC_UADDR16;
        }
        break;

      case REL_SECOFF:
        if (size==16 && pos==0 && mask==~0)
          t = R_PPC_SECTOFF;
        else if (size==16 && pos==0) {
          if (mask == 0x0000ffff)
            t = R_PPC_SECTOFF_LO;
          else if (mask == 0xffff0000) {
            if (rl2 = (*rl)->next) {
              nreloc *r2 = (nreloc *)rl2->reloc;
              if (rl2->type==(*rl)->type && r2->byteoffset==*roffset &&
                  r2->bitoffset==pos && r2->size==size && r2->mask==0x8000) {
                t = R_PPC_SECTOFF_HA;
                *rl = (*rl)->next;
              }
            }
            else
              t = R_PPC_SECTOFF_HI;
          }
        }
        break;

      case REL_PPCEABI_SDA2:
        if (size==16 && pos==0 && mask==~0)
          t = R_PPC_EMB_SDA2REL;
        break;

      case REL_PPCEABI_SDA21:
        if (size==16 && pos==0 && mask==~0) {
          t = R_PPC_EMB_SDA21;
          *roffset -= 2;  /* sda21 starts at beginning of instr. word! */
        }
        break;

      case REL_PPCEABI_SDAI16:
        if (size==16 && pos==0 && mask==~0)
          t = R_PPC_EMB_SDAI16;
        break;

      case REL_PPCEABI_SDA2I16:
        if (size==16 && pos==0 && mask==~0)
          t = R_PPC_EMB_SDA2I16;
        break;

      case REL_MORPHOS_DREL:
        if (size==16 && pos==0 && mask==~0)
          t = R_PPC_MORPHOS_DREL;
        else if (size==16 && pos==0) {
          if (mask == 0x0000ffff)
            t = R_PPC_MORPHOS_DREL_LO;
          else if (mask == 0xffff0000) {
            if (rl2 = (*rl)->next) {
              nreloc *r2 = (nreloc *)rl2->reloc;
              if (rl2->type==(*rl)->type && r2->byteoffset==*roffset &&
                  r2->bitoffset==pos && r2->size==size && r2->mask==0x8000) {
                t = R_PPC_MORPHOS_DREL_HA;
                *rl = (*rl)->next;
              }
            }
            else
              t = R_PPC_MORPHOS_DREL_HI;
          }
        }
        break;

      case REL_AMIGAOS_BREL:
        if (size==16 && pos==0 && mask==~0)
          t = R_PPC_AMIGAOS_BREL;
        else if (size==16 && pos==0) {
          if (mask == 0x0000ffff)
            t = R_PPC_AMIGAOS_BREL_LO;
          else if (mask == 0xffff0000) {
            if (rl2 = (*rl)->next) {
              nreloc *r2 = (nreloc *)rl2->reloc;
              if (rl2->type==(*rl)->type && r2->byteoffset==*roffset &&
                  r2->bitoffset==pos && r2->size==size && r2->mask==0x8000) {
                t = R_PPC_AMIGAOS_BREL_HA;
                *rl = (*rl)->next;
              }
            }
            else
              t = R_PPC_AMIGAOS_BREL_HI;
          }
        }
        break;
    }
  }
