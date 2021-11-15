  "instruction not supported on selected architecture",FATAL|ERROR,
  "illegal addressing mode",ERROR,
  "invalid register list",ERROR,
  "missing ) in register indirect addressing mode",ERROR,
  "address register required",ERROR,
  "bad size extension",ERROR,                                        /* 05 */
  "displacement at bad position",WARNING,
  "base or index register expected",ERROR,
  "missing ] in memory indirect addressing mode",ERROR,
  "no extension allowed here",WARNING,
  "illegal scale factor",ERROR,                                      /* 10 */
  "can't scale PC register",WARNING,
  "index register expected",ERROR,
  "too many ] in memory indirect addressing mode",ERROR,
  "missing outer displacement",ERROR,
  "%c expected",ERROR,                                               /* 15 */
  "can't use PC register as index",ERROR,
  "double registers in list",WARNING,
  "data register required",ERROR,
  "illegal bitfield width/offset",ERROR,
  "constant integer expression required",ERROR,                      /* 20 */
  "value from -64 to 63 required for k-factor",ERROR,
  "need 32 bits to reference a program label",WARNING,
  "option expected",ERROR,
  "absolute value expected",ERROR,
  "operand value out of range: %ld (valid: %ld..%ld)",ERROR,         /* 25 */
  "label in operand required",ERROR,
  "using signed operand as unsigned: %ld (valid: %ld..%ld), "
    "%ld to fix",WARNING,
  "branch destination out of range",ERROR,
  "displacement out of range",ERROR,
  "absolute displacement expected",ERROR,                            /* 30 */
  "unknown option %c%c ignored",WARNING,
  "absolute short address out of range",ERROR,
  "deprecated instruction alias",WARNING,
  "illegal opcode extension",FATAL|ERROR,
  "extension for unsized instruction ignored",WARNING,               /* 35 */
  "immediate operand out of range",ERROR,
  "immediate operand has illegal type or size",ERROR,
  "data objects with %d bits size are not supported",ERROR,
  "data out of range",ERROR,
  "data has illegal type",ERROR,                                     /* 40 */
  "illegal combination of ColdFire addressing modes",ERROR,
  "FP register required",ERROR,
  "unknown cpu type",ERROR,
  "register expected",ERROR,
  "link.w changed to link.l",WARNING,                                /* 45 */
  "branch out of range changed to jmp",WARNING,
  "lea-displacement out of range, changed into move/add",WARNING,
  "translated (A%d) into (0,A%d) for movep",WARNING,
  "operand optimized: %s",MESSAGE,
  "operand translated: %s",MESSAGE,                                  /* 50 */
  "instruction optimized: %s",MESSAGE,
  "instruction translated: %s",MESSAGE,
  "branch optimized into: b<cc>.%c",MESSAGE,
  "branch translated into: b<cc>.%c",MESSAGE,
  "basereg A%d already in use",ERROR,                                /* 55 */
  "basereg A%d is already free",WARNING,
  "short-branch to following instruction turned into a nop",WARNING,
  "not a valid small data register",ERROR,
  "small data mode is not enabled",ERROR,
  "division by zero",WARNING,                                        /* 60 */
  "can't use B%d register as index",ERROR,
  "register list on both sides",ERROR,
  "\"%s\" directive was replaced by an instruction with the same name",NOLINE|WARNING,
  "Addr.reg. operand at level #0 causes F-line exception",WARNING,
  "Dr and Dq are identical, transforming DIVxL.L effectively into "
    "DIVx.L",WARNING,                                                /* 65 */
  "not a valid register list symbol",ERROR,
  "trailing garbage in operand",WARNING,
  "encoding absolute displacement directly",WARNING,
  "internal symbol %s has been modified",WARNING,
  "instruction too large for bank prefix",ERROR,                     /* 70 */
