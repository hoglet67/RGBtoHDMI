/* name, operand 1, operand 2, operand 3, OpCode, Rn pos, CPU model */
"and",      {OP_GPR, OP_GPR,  OP_GPR  },  {0x80, 2, CPU_ALL},
"and",      {OP_GPR, OP_GPR,  OP_IMM  },  {0x80, 2, CPU_ALL},
"or",       {OP_GPR, OP_GPR,  OP_GPR  },  {0x81, 2, CPU_ALL},
"or",       {OP_GPR, OP_GPR,  OP_IMM  },  {0x81, 2, CPU_ALL},
"xor",      {OP_GPR, OP_GPR,  OP_GPR  },  {0x82, 2, CPU_ALL},
"xor",      {OP_GPR, OP_GPR,  OP_IMM  },  {0x82, 2, CPU_ALL},
"bitc",     {OP_GPR, OP_GPR,  OP_GPR  },  {0x83, 2, CPU_ALL},
"bitc",     {OP_GPR, OP_GPR,  OP_IMM  },  {0x83, 2, CPU_ALL},

"add",      {OP_GPR, OP_GPR,  OP_GPR  },  {0x84, 2, CPU_ALL},
"add",      {OP_GPR, OP_GPR,  OP_IMM  },  {0x84, 2, CPU_ALL},
"addc",     {OP_GPR, OP_GPR,  OP_GPR  },  {0x85, 2, CPU_ALL},
"addc",     {OP_GPR, OP_GPR,  OP_IMM  },  {0x85, 2, CPU_ALL},
"sub",      {OP_GPR, OP_GPR,  OP_GPR  },  {0x86, 2, CPU_ALL},
"sub",      {OP_GPR, OP_GPR,  OP_IMM  },  {0x86, 2, CPU_ALL},
"subb",     {OP_GPR, OP_GPR,  OP_GPR  },  {0x87, 2, CPU_ALL},
"subb",     {OP_GPR, OP_GPR,  OP_IMM  },  {0x87, 2, CPU_ALL},
"rsb",      {OP_GPR, OP_GPR,  OP_GPR  },  {0x88, 2, CPU_ALL},
"rsb",      {OP_GPR, OP_GPR,  OP_IMM  },  {0x88, 2, CPU_ALL},
"rsbb",     {OP_GPR, OP_GPR,  OP_GPR  },  {0x89, 2, CPU_ALL},
"rsbb",     {OP_GPR, OP_GPR,  OP_IMM  },  {0x89, 2, CPU_ALL},

"lls",      {OP_GPR, OP_GPR,  OP_GPR  },  {0x8A, 2, CPU_ALL},
"lls",      {OP_GPR, OP_GPR,  OP_IMM  },  {0x8A, 2, CPU_ALL},
"lrs",      {OP_GPR, OP_GPR,  OP_GPR  },  {0x8B, 2, CPU_ALL},
"lrs",      {OP_GPR, OP_GPR,  OP_IMM  },  {0x8B, 2, CPU_ALL},
"ars",      {OP_GPR, OP_GPR,  OP_GPR  },  {0x8C, 2, CPU_ALL},
"ars",      {OP_GPR, OP_GPR,  OP_IMM  },  {0x8C, 2, CPU_ALL},
"rotl",     {OP_GPR, OP_GPR,  OP_GPR  },  {0x8D, 2, CPU_ALL},
"rotl",     {OP_GPR, OP_GPR,  OP_IMM  },  {0x8D, 2, CPU_ALL},
"rotr",     {OP_GPR, OP_GPR,  OP_GPR  },  {0x8E, 2, CPU_ALL},
"rotr",     {OP_GPR, OP_GPR,  OP_IMM  },  {0x8E, 2, CPU_ALL},

"mul",      {OP_GPR, OP_GPR,  OP_GPR  },  {0x8F, 2, CPU_ALL},
"mul",      {OP_GPR, OP_GPR,  OP_IMM  },  {0x8F, 2, CPU_ALL},
"smul",     {OP_GPR, OP_GPR,  OP_GPR  },  {0x90, 2, CPU_ALL},
"smul",     {OP_GPR, OP_GPR,  OP_IMM  },  {0x90, 2, CPU_ALL},
"div",      {OP_GPR, OP_GPR,  OP_GPR  },  {0x91, 2, CPU_ALL},
"div",      {OP_GPR, OP_GPR,  OP_IMM  },  {0x91, 2, CPU_ALL},
"sdiv",     {OP_GPR, OP_GPR,  OP_GPR  },  {0x92, 2, CPU_ALL},
"sdiv",     {OP_GPR, OP_GPR,  OP_IMM  },  {0x92, 2, CPU_ALL},

/* P2 */

"mov",      {OP_GPR, OP_GPR },            {0x40, 1, CPU_ALL},
"mov",      {OP_GPR, OP_IMM },            {0x40, 1, CPU_ALL},
"swp",      {OP_GPR, OP_GPR },            {0x41, 1, CPU_ALL},
"not",      {OP_GPR, OP_GPR },            {0x42, 1, CPU_ALL},
"not",      {OP_GPR, OP_IMM },            {0x42, 1, CPU_ALL},

"sigxb",    {OP_GPR, OP_GPR },            {0x43, 1, CPU_ALL},
"sigxb",    {OP_GPR, OP_IMM },            {0x43, 1, CPU_ALL},
"sigxw",    {OP_GPR, OP_GPR },            {0x44, 1, CPU_ALL},
"sigxw",    {OP_GPR, OP_IMM },            {0x44, 1, CPU_ALL},

"jmp",      {OP_GPR, OP_GPR },            {0x4B, 1, CPU_ALL},
"jmp",      {OP_GPR, OP_IMM },            {0x4B, 1, CPU_ALL},
"call",     {OP_GPR, OP_GPR },            {0x4C, 1, CPU_ALL},
"call",     {OP_GPR, OP_IMM },            {0x4C, 1, CPU_ALL},

/* Branch */
"ifeq",     {OP_GPR, OP_GPR },            {0x70, 1, CPU_ALL},
"ifeq",     {OP_GPR, OP_IMM },            {0x70, 1, CPU_ALL},
"ifneq",    {OP_GPR, OP_GPR },            {0x71, 1, CPU_ALL},
"ifneq",    {OP_GPR, OP_IMM },            {0x71, 1, CPU_ALL},

"ifl",      {OP_GPR, OP_GPR },            {0x72, 1, CPU_ALL},
"ifl",      {OP_GPR, OP_IMM },            {0x72, 1, CPU_ALL},
"ifsl",     {OP_GPR, OP_GPR },            {0x73, 1, CPU_ALL},
"ifsl",     {OP_GPR, OP_IMM },            {0x73, 1, CPU_ALL},
"ifle",     {OP_GPR, OP_GPR },            {0x74, 1, CPU_ALL},
"ifle",     {OP_GPR, OP_IMM },            {0x74, 1, CPU_ALL},
"ifsle",    {OP_GPR, OP_GPR },            {0x75, 1, CPU_ALL},
"ifsle",    {OP_GPR, OP_IMM },            {0x75, 1, CPU_ALL},

"ifg",      {OP_GPR, OP_GPR },            {0x76, 1, CPU_ALL},
"ifg",      {OP_GPR, OP_IMM },            {0x76, 1, CPU_ALL},
"ifsg",     {OP_GPR, OP_GPR },            {0x77, 1, CPU_ALL},
"ifsg",     {OP_GPR, OP_IMM },            {0x77, 1, CPU_ALL},
"ifge",     {OP_GPR, OP_GPR },            {0x78, 1, CPU_ALL},
"ifge",     {OP_GPR, OP_IMM },            {0x78, 1, CPU_ALL},
"ifsge",    {OP_GPR, OP_GPR },            {0x79, 1, CPU_ALL},
"ifsge",    {OP_GPR, OP_IMM },            {0x79, 1, CPU_ALL},

"ifbits",   {OP_GPR, OP_GPR },            {0x7A, 1, CPU_ALL},
"ifbits",   {OP_GPR, OP_IMM },            {0x7A, 1, CPU_ALL},
"ifclear",  {OP_GPR, OP_GPR },            {0x7B, 1, CPU_ALL},
"ifclear",  {OP_GPR, OP_IMM },            {0x7B, 1, CPU_ALL},

/* P1 instructions */
"xchgb",    {OP_GPR },                    {0x20, 0, CPU_ALL},
"xchgb",    {OP_IMM },                    {0x20, 0, CPU_ALL},
"xchgw",    {OP_GPR },                    {0x21, 0, CPU_ALL},
"xchgw",    {OP_IMM },                    {0x21, 0, CPU_ALL},

"getpc",    {OP_GPR },                    {0x22, 0, CPU_ALL},

"pop",      {OP_GPR },                    {0x23, 0, CPU_ALL},
"push",     {OP_GPR },                    {0x24, 0, CPU_ALL},
"push",     {OP_IMM },                    {0x24, 0, CPU_ALL},

"jmp",      {OP_GPR },                    {0x25, 0, CPU_ALL},
"jmp",      {OP_IMM },                    {0x25, 0, CPU_ALL},
"call",     {OP_GPR },                    {0x26, 0, CPU_ALL},
"call",     {OP_IMM },                    {0x26, 0, CPU_ALL},

"rjmp",     {OP_GPR },                    {0x27, 0, CPU_ALL},
"rjmp",     {OP_IMM },                    {0x27, 0, CPU_ALL},
"rcall",    {OP_GPR },                    {0x28, 0, CPU_ALL},
"rcall",    {OP_IMM },                    {0x28, 0, CPU_ALL},

"int",      {OP_GPR },                    {0x29, 0, CPU_ALL},
"int",      {OP_IMM },                    {0x29, 0, CPU_ALL},

/* NP instructions */
"sleep",    { },                          {0x00, 0, CPU_ALL},
"ret",      { },                          {0x01, 0, CPU_ALL},
"rfi",      { },                          {0x02, 0, CPU_ALL},

  /* Load / Store*/
/*          Register    Were to read  */
"load",     {OP_GPR, OP_GPR,  OP_GPR  },  {0x93, 2, CPU_ALL},
"load",     {OP_GPR, OP_GPR,  OP_IMM  },  {0x93, 2, CPU_ALL},
"load",     {OP_GPR, OP_GPR   },          {0x45, 1, CPU_ALL},
"load",     {OP_GPR, OP_IMM   },          {0x45, 1, CPU_ALL},
"loadw",    {OP_GPR, OP_GPR,  OP_GPR  },  {0x94, 2, CPU_ALL},
"loadw",    {OP_GPR, OP_GPR,  OP_IMM  },  {0x94, 2, CPU_ALL},
"loadw",    {OP_GPR, OP_GPR   },          {0x46, 1, CPU_ALL},
"loadw",    {OP_GPR, OP_IMM   },          {0x46, 1, CPU_ALL},
"loadb",    {OP_GPR, OP_GPR,  OP_GPR  },  {0x95, 2, CPU_ALL},
"loadb",    {OP_GPR, OP_GPR,  OP_IMM  },  {0x95, 2, CPU_ALL},
"loadb",    {OP_GPR, OP_GPR   },          {0x47, 1, CPU_ALL},
"loadb",    {OP_GPR, OP_IMM   },          {0x47, 1, CPU_ALL},
/*           Were to write     data to write  */
"store",    {OP_GPR, OP_GPR,  OP_GPR  },  {0x96, 1, CPU_ALL},
"store",    {OP_GPR, OP_IMM,  OP_GPR  },  {0x96, 1, CPU_ALL},
"store",    {OP_GPR, OP_GPR   },          {0x48, 0, CPU_ALL},
"store",    {OP_IMM, OP_GPR   },          {0x48, 0, CPU_ALL},
"storew",   {OP_GPR, OP_GPR,  OP_GPR  },  {0x97, 1, CPU_ALL},
"storew",   {OP_GPR, OP_IMM,  OP_GPR  },  {0x97, 1, CPU_ALL},
"storew",   {OP_GPR, OP_GPR   },          {0x49, 0, CPU_ALL},
"storew",   {OP_IMM, OP_GPR   },          {0x49, 0, CPU_ALL},
"storeb",   {OP_GPR, OP_GPR,  OP_GPR  },  {0x98, 1, CPU_ALL},
"storeb",   {OP_GPR, OP_IMM,  OP_GPR  },  {0x98, 1, CPU_ALL},
"storeb",   {OP_GPR, OP_GPR   },          {0x4A, 0, CPU_ALL},
"storeb",   {OP_IMM, OP_GPR   },          {0x4A, 0, CPU_ALL},

/* TODO Others */

