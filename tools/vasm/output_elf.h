/* output_elf.h header file for ELF objects */
/* (c) in 2002-2016 by Frank Wille */

/* e_indent indexes */
#define EI_NIDENT  16
#define EI_MAG0    0
#define EI_MAG1    1
#define EI_MAG2    2
#define EI_MAG3    3
#define EI_CLASS   4
#define EI_DATA    5
#define EI_VERSION 6
#define EI_PAD     7

/* EI_CLASS */
#define ELFCLASSNONE 0
#define ELFCLASS32   1
#define ELFCLASS64   2

/* EI_DATA */
#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

/* e_type */
#define ET_NONE   0                 /* No file type */
#define ET_REL    1                 /* Relocatable file */
#define ET_EXEC   2                 /* Executable file */
#define ET_DYN    3                 /* Shared object file */
#define ET_CORE   4                 /* Core file */
#define ET_LOPROC 0xFF00            /* Processor-specific */
#define ET_HIPROC 0xFFFF            /* Processor-specific */

/* e_version */
#define EV_NONE    0
#define EV_CURRENT 1

/* e_machine */
#define EM_NONE           0
#define EM_M32            1
#define EM_SPARC          2
#define EM_386            3
#define EM_68K            4
#define EM_88K            5
#define EM_860            7
#define EM_MIPS           8
#define EM_MIPS_RS4_BE    10
#define EM_SPARC64        11
#define EM_PARISC         15
#define EM_PPC_OLD        17
#define EM_SPARC32PLUS    18
#define EM_PPC            20
#define EM_ARM            40
#define EM_X86_64         62
#define EM_JAGRISC        0x9004
#define EM_CYGNUS_POWERPC 0x9025
#define EM_ALPHA          0x9026

/* special sections indexes */
#define SHN_UNDEF 0
#define SHN_ABS 0xfff1
#define SHN_COMMON 0xfff2

/* sh_type */
#define SHT_NULL        0           /* Section header table entry unused */
#define SHT_PROGBITS    1           /* Program specific (private) data */
#define SHT_SYMTAB      2           /* Link editing symbol table */
#define SHT_STRTAB      3           /* A string table */
#define SHT_RELA        4           /* Relocation entries with addends */
#define SHT_HASH        5           /* A symbol hash table */
#define SHT_DYNAMIC     6           /* Information for dynamic linking */
#define SHT_NOTE        7           /* Information that marks file */
#define SHT_NOBITS      8           /* Section occupies no space in file */
#define SHT_REL         9           /* Relocation entries, no addends */
#define SHT_SHLIB       10          /* Reserved, unspecified semantics */
#define SHT_DYNSYM      11          /* Dynamic linking symbol table */
#define SHT_LOPROC      0x70000000  /* Processor-specific semantics, lo */
#define SHT_HIPROC      0x7FFFFFFF  /* Processor-specific semantics, hi */
#define SHT_LOUSER      0x80000000  /* Application-specific semantics */
#define SHT_HIUSER      0x8FFFFFFF  /* Application-specific semantics */

/* sh_flags */
#define SHF_WRITE     (1 << 0)      /* Writable data during execution */
#define SHF_ALLOC     (1 << 1)      /* Occupies memory during execution */
#define SHF_EXECINSTR (1 << 2)      /* Executable machine instructions */
#define SHF_MASKPROC  0xF0000000    /* Processor-specific semantics */

/* ST_BIND */
#define STB_LOCAL  0                /* Symbol not visible outside obj */
#define STB_GLOBAL 1                /* Symbol visible outside obj */
#define STB_WEAK   2                /* Like globals, lower precedence */
#define STB_LOPROC 13               /* Application-specific semantics */
#define STB_HIPROC 15               /* Application-specific semantics */

/* ST_TYPE */
#define STT_NOTYPE  0               /* Symbol type is unspecified */
#define STT_OBJECT  1               /* Symbol is a data object */
#define STT_FUNC    2               /* Symbol is a code object */
#define STT_SECTION 3               /* Symbol associated with a section */
#define STT_FILE    4               /* Symbol gives a file name */
#define STT_LOPROC  13              /* Application-specific semantics */
#define STT_HIPROC  15              /* Application-specific semantics */


/* 32-bit ELF */

struct Elf32_Ehdr {
  uint8_t	e_ident[EI_NIDENT]; /* ELF "magic number" */
  uint8_t	e_type[2];          /* Identifies object file type */
  uint8_t	e_machine[2];       /* Specifies required architecture */
  uint8_t	e_version[4];       /* Identifies object file version */
  uint8_t	e_entry[4];         /* Entry point virtual address */
  uint8_t	e_phoff[4];         /* Program header table file offset */
  uint8_t	e_shoff[4];         /* Section header table file offset */
  uint8_t	e_flags[4];         /* Processor-specific flags */
  uint8_t	e_ehsize[2];        /* ELF header size in bytes */
  uint8_t	e_phentsize[2];     /* Program header table entry size */
  uint8_t	e_phnum[2];         /* Program header table entry count */
  uint8_t	e_shentsize[2];     /* Section header table entry size */
  uint8_t	e_shnum[2];         /* Section header table entry count */
  uint8_t	e_shstrndx[2];      /* Section header string table index */
};

struct Elf32_Shdr {
  uint8_t	sh_name[4];         /* Section name, index in string tbl */
  uint8_t	sh_type[4];         /* Type of section */
  uint8_t	sh_flags[4];        /* Miscellaneous section attributes */
  uint8_t	sh_addr[4];         /* Section virtual addr at execution */
  uint8_t	sh_offset[4];       /* Section file offset */
  uint8_t	sh_size[4];         /* Size of section in bytes */
  uint8_t	sh_link[4];         /* Index of another section */
  uint8_t	sh_info[4];         /* Additional section information */
  uint8_t	sh_addralign[4];    /* Section alignment */
  uint8_t	sh_entsize[4];      /* Entry size if section holds table */
};

struct Elf32_Sym {
  uint8_t	st_name[4];         /* Symbol name, index in string tbl */
  uint8_t	st_value[4];        /* Value of the symbol */
  uint8_t	st_size[4];         /* Associated symbol size */
  uint8_t	st_info[1];         /* Type and binding attributes */
  uint8_t	st_other[1];        /* No defined meaning, 0 */
  uint8_t	st_shndx[2];        /* Associated section index */
};
/* st_info */
#define ELF32_ST_BIND(i) ((i)>>4)
#define ELF32_ST_TYPE(i) ((i)&0xf)
#define ELF32_ST_INFO(b,t) (((b)<<4)+((t)&0xf))

struct Elf32_Note {
  uint8_t	namesz[4];          /* Size of entry's owner string */
  uint8_t	descsz[4];          /* Size of the note descriptor */
  uint8_t	type[4];            /* Interpretation of the descriptor */
  char		name[1];            /* Start of the name+desc data */
};

struct Elf32_Rel {
  uint8_t	r_offset[4];        /* Location at which to apply the action */
  uint8_t	r_info[4];          /* index and type of relocation */
};

struct Elf32_Rela {
  uint8_t	r_offset[4];        /* Location at which to apply the action */
  uint8_t	r_info[4];          /* index and type of relocation */
  uint8_t	r_addend[4];        /* Constant addend used to compute value */
};

/* r_info */
#define ELF32_R_SYM(i) ((i)>>8)
#define ELF32_R_TYPE(i) ((uint8_t)(i))
#define ELF32_R_INFO(s,t) (((s)<<8)+(uint8_t)(t))


/* 64-bit ELF */

struct Elf64_Ehdr {
  uint8_t	e_ident[EI_NIDENT]; /* ELF "magic number" */
  uint8_t	e_type[2];          /* Identifies object file type */
  uint8_t	e_machine[2];       /* Specifies required architecture */
  uint8_t	e_version[4];       /* Identifies object file version */
  uint8_t	e_entry[8];         /* Entry point virtual address */
  uint8_t	e_phoff[8];         /* Program header table file offset */
  uint8_t	e_shoff[8];         /* Section header table file offset */
  uint8_t	e_flags[4];         /* Processor-specific flags */
  uint8_t	e_ehsize[2];        /* ELF header size in bytes */
  uint8_t	e_phentsize[2];     /* Program header table entry size */
  uint8_t	e_phnum[2];         /* Program header table entry count */
  uint8_t	e_shentsize[2];     /* Section header table entry size */
  uint8_t	e_shnum[2];         /* Section header table entry count */
  uint8_t	e_shstrndx[2];      /* Section header string table index */
};

struct Elf64_Shdr {
  uint8_t	sh_name[4];         /* Section name, index in string tbl */
  uint8_t	sh_type[4];         /* Type of section */
  uint8_t	sh_flags[8];        /* Miscellaneous section attributes */
  uint8_t	sh_addr[8];         /* Section virtual addr at execution */
  uint8_t	sh_offset[8];       /* Section file offset */
  uint8_t	sh_size[8];         /* Size of section in bytes */
  uint8_t	sh_link[4];         /* Index of another section */
  uint8_t	sh_info[4];         /* Additional section information */
  uint8_t	sh_addralign[8];    /* Section alignment */
  uint8_t	sh_entsize[8];      /* Entry size if section holds table */
};

struct Elf64_Sym {
  uint8_t	st_name[4];         /* Symbol name, index in string tbl */
  uint8_t	st_info[1];         /* Type and binding attributes */
  uint8_t	st_other[1];        /* No defined meaning, 0 */
  uint8_t	st_shndx[2];        /* Associated section index */
  uint8_t	st_value[8];        /* Value of the symbol */
  uint8_t	st_size[8];         /* Associated symbol size */
};
#define ELF64_ST_BIND(i) ((i)>>4)
#define ELF64_ST_TYPE(i) ((i)&0xf)
#define ELF64_ST_INFO(b,t) (((b)<<4)+((t)&0xf))

struct Elf64_Note {
  uint8_t	namesz[4];          /* Size of entry's owner string */
  uint8_t	descsz[4];          /* Size of the note descriptor */
  uint8_t	type[4];            /* Interpretation of the descriptor */
  char		name[1];            /* Start of the name+desc data */
};

struct Elf64_Rel {
  uint8_t	r_offset[8];        /* Location at which to apply the action */
  uint8_t	r_info[8];          /* index and type of relocation */
};

struct Elf64_Rela {
  uint8_t	r_offset[8];        /* Location at which to apply the action */
  uint8_t	r_info[8];          /* index and type of relocation */
  uint8_t	r_addend[8];        /* Constant addend used to compute value */
};

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i)	((i) & 0xffffffff)
#define ELF64_R_INFO(sym,type) (((sym) << 32) + (type))


/* vasm-specific extensions */

typedef uint64_t elfull;

struct StrTabList {
  struct list l;
  unsigned index;
};

struct StrTabNode {
  struct node n;
  char *str;
};

struct Shdr32Node {
  struct node n;
  struct Elf32_Shdr s;
};

struct Symbol32Node {
  struct node n;
  char *name;
  unsigned idx;
  struct Elf32_Sym s;
};

struct Rela32Node {
  struct node n;
  struct Elf32_Rela r;
};

struct Rel32Node {
  struct node n;
  struct Elf32_Rel r;
};

struct Shdr64Node {
  struct node n;
  struct Elf64_Shdr s;
};

struct Symbol64Node {
  struct node n;
  char *name;
  unsigned idx;
  struct Elf64_Sym s;
};

struct Rela64Node {
  struct node n;
  struct Elf64_Rela r;
};

struct Rel64Node {
  struct node n;
  struct Elf64_Rel r;
};


#if defined(VASM_CPU_M68K)
#define RELA 1
#define ELFCPU EM_68K
#elif defined(VASM_CPU_PPC)
#define RELA 1
#define ELFCPU EM_PPC
#elif defined(VASM_CPU_ARM)
#define RELA 0
#define ELFCPU EM_ARM
#elif defined(VASM_CPU_X86)
#define RELA (bytespertaddr == 8)
#define ELFCPU (bytespertaddr == 8 ? EM_X86_64 : EM_386)
#elif defined(VASM_CPU_JAGRISC)
#define RELA 1
#define ELFCPU EM_JAGRISC
#else
#define ELFCPU 0
#endif

#define ELFSYMHTABSIZE 0x10000
