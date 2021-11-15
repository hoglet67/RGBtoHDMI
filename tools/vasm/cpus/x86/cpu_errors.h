  "instruction not supported on selected architecture",ERROR,
  "trailing garbage in operand",WARNING,
  "same type of prefix used twice",WARNING,
  "immediate operand illegal with absolute jump",ERROR,
  "base register expected",ERROR,
  "scale factor without index register",ERROR,                  /* 5 */
  "missing ')' in baseindex addressing mode",ERROR,
  "redundant %s prefix ignored",WARNING,
  "unknown register specified",ERROR,
  "using register %%%s instead of %%%s due to '%c' suffix",WARNING,
  "%%%s not allowed with '%c' suffix",ERROR,                    /* 10 */
  "illegal suffix '%c'",ERROR,
  "instruction has no suffix and no register operands - size is unknown",ERROR,
  "UNUSED",ERROR,
  "memory operand expected",ERROR,
  "you cannot pop %%%s",ERROR,                                  /* 15 */
  "translating to %s %%%s,%%%s",WARNING,
  "translating to %s %%%s",WARNING,
  "absolute scale factor required",ERROR,
  "illegal scale factor (valid: 1,2,4,8)",ERROR,
  "data objects with %d bits size are not supported",ERROR,     /* 20 */
  "need at least %d bits for a relocatable symbol",ERROR,
  "pc-relative jump destination out of range (%lld)",ERROR,
  "instruction doesn't support these operand sizes",ERROR,
  "cannot determine immediate operand size without a suffix",ERROR,
  "displacement doesn't fit into %d bits",ERROR,                /* 25 */
