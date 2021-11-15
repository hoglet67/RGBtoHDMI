/*
 * Copyright (c) 2007-2009 Dominic Morris <dom /at/ suborbital.org.uk>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* z80 backend for vasm */

/* 8085 instructions support by gbm 06'21 */

#include "vasm.h"



/*
 * TODO: Undocumented ld a,rlc(ix+d) style unstructions
 * TODO: Finish off rcm4000 opcodes
 *
 * Note: All z80 opcodes work as expected
 */
mnemonic mnemonics[] = {
    "aci",  { OP_ABS },                                         { TYPE_NONE,  0xce, CPU_80OS, 0 },
   
    "adc",  { OP_HL|OP_INDEX|OP_RALT, OP_ARITH16 },             { TYPE_ARITH16, 0xed4a, CPU_ZILOG|CPU_RABBIT, F_ALTD },
    "adc",  { OP_REG8|OP_INDEX },                               { TYPE_ARITH8,  0x88, CPU_ALL|CPU_80OS, F_ALL, 0, 0, RCM_EMU_INCREMENT },
    "adc",  { OP_REG8|OP_INDEX },                               { TYPE_ARITH8,  0x7f88, CPU_RCM4000, F_ALL },
    "adc",  { OP_A|OP_RALT, OP_REG8 | OP_INDEX },               { TYPE_ARITH8,  0x88, CPU_ALL, F_ALTD, 0, 0, RCM_EMU_INCREMENT },
    "adc",  { OP_A|OP_RALT, OP_REG8 | OP_INDEX },               { TYPE_ARITH8,  0x7f88, CPU_RCM4000, F_ALTD, },
    "adc",  { OP_ABS },                                         { TYPE_NONE,  0xce, CPU_ALL, F_ALTD },
    "adc",  { OP_A|OP_RALT, OP_ABS },                           { TYPE_NONE,  0xce, CPU_ALL, F_ALTD },

    "add",  { OP_REG8 },                                        { TYPE_ARITH8,  0x80, CPU_ALL|CPU_80OS, F_ALTD, 0, 0, RCM_EMU_INCREMENT },
    "add",  { OP_REG8 },                                        { TYPE_ARITH8,  0x7f80, CPU_RCM4000, F_ALTD },
    "add",  { OP_HL|OP_RALT, OP_JK },                           { TYPE_NONE, 0x65, CPU_RCM4000, F_ALTD },

    "add",  { OP_HL|OP_INDEX|OP_RALT, OP_INDEX|OP_ARITH16},     { TYPE_ARITH16, 0x09, CPU_ALL, F_ALTD },

    "add",  { OP_A|OP_RALT, OP_REG8 },                          { TYPE_ARITH8,  0x80, CPU_ALL, F_ALL, 0, 0, RCM_EMU_INCREMENT },
    "add",  { OP_A|OP_RALT, OP_REG8 },                          { TYPE_ARITH8,  0x7f80,  CPU_RCM4000, F_ALL }, /* add a,r for RCM4000 */
    "add",  { OP_A|OP_RALT, OP_ABS },                           { TYPE_NONE,  0xc6, CPU_ALL, F_ALTD },
    "add",  { OP_SP, OP_ABS },                                  { TYPE_NONE, 0x27, CPU_RABBIT|CPU_GB80, 0, 0, 0, 0, RCM_EMU_INCREMENT }, /* add sp,xx */
    "add",  { OP_SP, OP_ABS },                                  { TYPE_NONE, 0xe8, CPU_GB80, 0 }, /* add sp,xx */
    "add",  { OP_JKHL|OP_RALT, OP_BCDE },                        { TYPE_NONE, 0xedc6, CPU_RCM4000, F_ALTD },
    "add",  { OP_ABS },                                         { TYPE_NONE,  0xc6, CPU_ALL, F_ALTD },

    "ana",  { OP_REG8 },                                        { TYPE_ARITH8, 0xa0, CPU_80OS  }, /* and a,r  */
    
    "and",  { OP_HL|OP_INDEX|OP_RALT, OP_DE },                  { TYPE_NONE, 0xDC, CPU_RABBIT, F_ALTD }, /* and hl/ix/iy,de */
    "and",  { OP_JKHL|OP_RALT, OP_BCDE },                       { TYPE_NONE, 0xede6, CPU_RCM4000, F_ALTD },
    "and",  { OP_REG8|OP_INDEX },                               { TYPE_ARITH8, 0xa0, CPU_ALL, F_ALL, 0, 0, RCM_EMU_INCREMENT },
    "and",  { OP_REG8|OP_INDEX },                               { TYPE_ARITH8, 0x7fa0, CPU_RCM4000, F_ALL,  }, /* and a,r  */
    "and",  { OP_A|OP_RALT, OP_REG8|OP_INDEX },                 { TYPE_ARITH8, 0xa0, CPU_ALL, F_ALTD, 0, 0, RCM_EMU_INCREMENT },
    "and",  { OP_A|OP_RALT, OP_REG8|OP_INDEX },                 { TYPE_ARITH8, 0x7fa0, CPU_RCM4000, F_ALTD },
    "and",  { OP_A|OP_RALT, OP_ABS },                           { TYPE_NONE, 0xE6, CPU_ALL, F_ALTD },
    "and",  { OP_ABS },                                         { TYPE_NONE, 0xE6, CPU_ALL, F_ALTD },
    
    "adi",  { OP_ABS },                                         { TYPE_NONE, 0xc6, CPU_80OS, 0 },
    
    "ani",  { OP_ABS },                                         { TYPE_NONE, 0xe6, CPU_80OS, 0 },

    "arhl", { OP_NONE },                                        { TYPE_NONE, 0x10, CPU_80OS, 0 },	/* gbm 8085 */


    "bit",  { OP_NUMBER, OP_REG8|OP_INDEX|OP_RALT|OP_RALTHL},   { TYPE_BIT, 0xCB40, CPU_NOT8080, F_ALL },
    "bool", { OP_HL|OP_INDEX|OP_RALT},                          { TYPE_NONE, 0xCC, CPU_RABBIT, F_ALTD }, /* bool hl,ix,iy */


    "call", { OP_FLAGS, OP_ABS16 },                             { TYPE_FLAGS, 0xc4, CPU_ALL },  /* We use emulation on RCM */
    "call", { OP_HL|OP_INDIR|OP_INDEX },                        { TYPE_NONE, 0xEA, CPU_RCM4000, 0 }, /* call (hl/ix/iy) */
    "call", { OP_ABS16 },                                       { TYPE_NONE, 0xcd, CPU_ALL|CPU_80OS, 0 },

    "cbm",  { OP_ABS },                                         { TYPE_NONE, 0xed00, CPU_RCM4000, F_IO }, /* cbm x */

    "cc",   { OP_ABS16 },                                       { TYPE_NONE, 0xdc, CPU_80OS, 0 },

    "ccf",  { OP_NONE, },                                       { TYPE_NONE, 0x3f, CPU_ALL, F_ALTD },
 
    "clr",  { OP_HL|OP_RALT },                                  { TYPE_NONE, 0xBF, CPU_RCM4000, F_ALTD },
    
    "cm",   { OP_ABS16 },                                       { TYPE_NONE, 0xfc, CPU_80OS, 0 },
    
    "cma",  { OP_NONE, },                                       { TYPE_NONE, 0x2f, CPU_80OS, 0 },
    
    "cmc",  { OP_NONE, },                                       { TYPE_NONE, 0x3f, CPU_80OS, 0 },

    "cnc",   { OP_ABS16 },                                      { TYPE_NONE, 0xd4, CPU_80OS, 0 },
    "cnz",   { OP_ABS16 },                                      { TYPE_NONE, 0xc4, CPU_80OS, 0 },
    
    "copy", { OP_NONE },                                        { TYPE_NONE, 0xed80, CPU_RCM4000, 0 },
    "copyr",{ OP_NONE },                                        { TYPE_NONE, 0xed88, CPU_RCM4000, 0 },
    
    "cmp", { OP_REG8 | OP_INDEX },                             { TYPE_ARITH8, 0xb8, CPU_80OS, 0 },
   
    "cp",   { OP_REG8 | OP_INDEX },                            { TYPE_ARITH8, 0xb8, CPU_ALL, F_ALL, 0, 0, RCM_EMU_INCREMENT },
    "cp",   { OP_REG8 | OP_INDEX },                             { TYPE_ARITH8, 0x7fb8, CPU_RCM4000,F_ALL },
    "cp",   { OP_A|OP_RALT, OP_REG8 | OP_INDEX },               { TYPE_ARITH8, 0xb8, CPU_ALL, F_ALL, 0, 0, RCM_EMU_INCREMENT },
    "cp",   { OP_A|OP_RALT, OP_REG8 | OP_INDEX },               { TYPE_ARITH8, 0x7fb8, CPU_RCM4000, F_ALL },
    "cp",   { OP_A|OP_RALT, OP_ABS },                           { TYPE_NONE, 0xfe, CPU_ALL, F_ALL },
    "cp",   { OP_JKHL|OP_RALT, OP_BCDE },                       { TYPE_NONE, 0xed58, CPU_RCM4000, F_ALTD },
    "cp",   { OP_HL|OP_RALT, OP_DE },                           { TYPE_NONE, 0xED48, CPU_RCM4000, F_ALTD }, /* cp hl,de */
    "cp",   { OP_HL|OP_RALT, OP_ABS },                          { TYPE_NONE, 0x48, CPU_RCM4000, F_ALTD }, /* cp hl,de */

    "cp",   { OP_ABS },                                          { TYPE_NONE, 0xfe, CPU_ALL|CPU_80OS, F_80OS_CHECK},
    
    "cpd",  { OP_NONE, },                                       { TYPE_NONE, 0xeda9, CPU_RABBIT|CPU_ZILOG, 0, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, 0 },
    "cpdr", { OP_NONE, },                                       { TYPE_NONE, 0xedb9, CPU_RABBIT|CPU_ZILOG, 0, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, 0 },
    
    "cpe",   { OP_ABS16 },                                      { TYPE_NONE, 0xec, CPU_80OS, 0 },
    
    "cpi",  { OP_NONE, },                                       { TYPE_NONE, 0xeda1, CPU_RABBIT|CPU_ZILOG, 0, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, 0 },
    "cpi",  { OP_ABS },                                         { TYPE_NONE, 0xfe, CPU_80OS, 0 },
    "cpir", { OP_NONE, },                                       { TYPE_NONE, 0xedb1, CPU_RABBIT|CPU_ZILOG, 0, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, 0 },
    "cpl",  { OP_NONE, },                                       { TYPE_NONE, 0x2f,   CPU_ALL, F_ALTD },
    
    "cpo",   { OP_ABS16 },                                      { TYPE_NONE, 0xe4, CPU_80OS, 0 },
    
    "cz",   { OP_ABS16 },                                       { TYPE_NONE, 0xcc, CPU_80OS, 0 },
    
    "daa",  { OP_NONE, },                                       { TYPE_NONE, 0x27,   CPU_ZILOG|CPU_8080|CPU_GB80|CPU_80OS, 0 },
    
    "dad",  { OP_RP },                                          { TYPE_RP, 0x09, CPU_80OS, 0 },
    
    "dcr",  { OP_REG8 },                                        { TYPE_MISC8, 0x05,  CPU_80OS, 0 },
    
    "dcx",  { OP_RP },                                          { TYPE_RP, 0x0b,     CPU_80OS, 0 },
    
    "dec",  { OP_REG8 | OP_INDEX|OP_RALT },                     { TYPE_MISC8, 0x05,  CPU_ALL, F_ALL },
    "dec",  { OP_ARITH16 | OP_INDEX|OP_RALT },                  { TYPE_ARITH16, 0x0b,   CPU_ALL, F_ALTD },
    "di",   { OP_NONE, },                                       { TYPE_NONE, 0xf3,   CPU_ZILOG|CPU_8080|CPU_80OS|CPU_GB80, F_ALTD },

    "djnz", { OP_ABS },                                         { TYPE_RELJUMP, 0x10, CPU_NOTGB80, F_ALTD },
    "dsub", { OP_NONE },                                        { TYPE_NONE, 0x08, CPU_80OS, 0 },   /* gbm 8085 */
    "dwjnz",{ OP_ABS },                                         { TYPE_RELJUMP, 0xed10, CPU_RCM4000, F_ALTD },


    "ei",   { OP_NONE, },                                       { TYPE_NONE, 0xfb,   CPU_ZILOG|CPU_8080|CPU_80OS, CPU_GB80, 0 },
    "ex",   { OP_AF, OP_AF | OP_ALT },                          { TYPE_NONE, 0x08, CPU_ZILOG|CPU_RABBIT, F_ALTDW }, /* ex af,af' */
    "ex",   { OP_BC, OP_HL },                                   { TYPE_NONE, 0xb3, CPU_RCM4000, 0 },
    "ex",   { OP_BC, OP_HL|OP_ALT },                            { TYPE_NONE, 0x76b3, CPU_RCM4000, 0 },
    "ex",   { OP_BC|OP_ALT, OP_HL },                            { TYPE_NONE, 0xed74, CPU_RCM4000, 0 },
    "ex",   { OP_BC|OP_ALT, OP_HL|OP_ALT },                     { TYPE_NONE, 0x76ed74, CPU_RCM4000, 0 },


    "ex",   { OP_DE, OP_HL|OP_RALT },                           { TYPE_NONE, 0xEB, CPU_NOTGB80, F_ALTD }, /* ex de,hl */
    "ex",   { OP_DE, OP_HL|OP_ALT },                            { TYPE_NONE, 0x76Eb, CPU_RABBIT, F_ALTDW }, /* ex de,hl' (is this right?) */
    "ex",   { OP_DE|OP_ALT, OP_HL|OP_RALT },                    { TYPE_NONE, 0xe3, CPU_RABBIT, F_ALTD }, /* ex de',hl */
    "ex",   { OP_DE|OP_ALT, OP_HL|OP_ALT },                     { TYPE_NONE, 0x76e3, CPU_RABBIT, F_ALTDW }, /* ex de',hl */

    "ex",   { OP_JKHL, OP_BCDE },                               { TYPE_NONE, 0xb4, CPU_RCM4000, 0 }, /* ex jkhl,bcde */
    "ex",   { OP_JK|OP_ALT, OP_HL },                            { TYPE_NONE, 0xed7c, CPU_RCM4000, 0 }, /* ex jk',hl */
    "ex",   { OP_JK, OP_HL },                                   { TYPE_NONE, 0xb9, CPU_RCM4000, F_ALTD }, /* ex jk,hl */



    "ex",   { OP_SP|OP_INDIR, OP_HL|OP_INDEX|OP_RALT},          { TYPE_NONE, 0xe3, CPU_NOTGB80, F_ALTD, RCM_EMU_INCREMENT, RCM_EMU_INCREMENT, RCM_EMU_INCREMENT  },
    "ex",   { OP_SP|OP_INDIR, OP_HL|OP_INDEX|OP_RALT},          { TYPE_NONE, 0xed54, CPU_RABBIT, F_ALTD}, /* Must be after standard ex for rabbit compat */

    "exp",  { OP_NONE },                                        { TYPE_NONE, 0xedd9, CPU_RCM4000, 0 },

    "exx",  { OP_NONE, },                                       { TYPE_NONE, 0xd9,   CPU_ZILOG|CPU_RABBIT, F_ALTDW },

    "flag", { OP_FLAGS, OP_HL },                                { TYPE_FLAGS, 0xedc4, CPU_RCM4000, 0 }, /* flag cc,hl */
    "flag", { OP_FLAGS_RCM, OP_HL },                            { TYPE_FLAGS, 0xeda4, CPU_RCM4000, 0 }, /* flag cc,hl */
    
    "fsyscall",{ OP_NONE },                                     { TYPE_NONE, 0xed55, CPU_RCM4000, 0 },

    "halt", { OP_NONE, },                                       { TYPE_NONE, 0x76,   CPU_ZILOG|CPU_8080|CPU_GB80, 0},
    "hlt",  { OP_NONE, },                                       { TYPE_NONE, 0x76,   CPU_80OS, 0},

    "ibox", { OP_A|OP_RALT },                                   { TYPE_NONE, 0xed12, CPU_RCM4000, F_ALTD },
    "idet", { OP_NONE },                                        { TYPE_NONE, 0x5b, CPU_RCM3000|CPU_RCM4000, 0 }, 

    "im",   { OP_NUMBER },                                      { TYPE_IM,   0xED46, CPU_ZILOG, 0 },

    "in",   { OP_REG8, OP_PORT },                               { TYPE_MISC8, 0xed40, CPU_ZILOG, 0 }, /* in r,(c) */
    "in",   { OP_A, OP_ADDR8 },                                 { TYPE_NONE, 0xDB, CPU_8080|CPU_ZILOG, 0 },  /* in a,(xx) */
    "in",   { OP_ABS },                                         { TYPE_NONE, 0xDB, CPU_80OS, 0 },  /* in xx */
    "in",   { OP_PORT },                                        { TYPE_NONE, 0xed70, CPU_ZILOG, 0 }, /* in (c) */ 
    "in",   { OP_F, OP_PORT },                                  { TYPE_NONE, 0xed70, CPU_ZILOG, 0 }, /* in f,(c) */

    "in0",  { OP_REG8, OP_ADDR8 },                              { TYPE_MISC8, 0xed00, CPU_Z180|CPU_EZ80, 0 }, /* in0 r,(xx) */

    "inc",  { OP_REG8 | OP_INDEX },                             { TYPE_MISC8, 0x04,   CPU_ALL, F_ALTD },
    "inc",  { OP_ARITH16 | OP_INDEX },                          { TYPE_ARITH16, 0x03,   CPU_ALL, F_ALTD },
    "ind",  { OP_NONE, },                                       { TYPE_NONE, 0xedaa, CPU_ZILOG , 0},
    "indr", { OP_NONE, },                                       { TYPE_NONE, 0xedba, CPU_ZILOG, 0 },
    "ini",  { OP_NONE, },                                       { TYPE_NONE, 0xeda2, CPU_ZILOG, 0 },
    "inir", { OP_NONE, },                                       { TYPE_NONE, 0xedb2, CPU_ZILOG, 0 },
    
    "inr",  { OP_REG8 },                                        { TYPE_MISC8, 0x04,   CPU_80OS, 0 },
   
    "inx",  { OP_RP },                                          { TYPE_RP, 0x03,   CPU_80OS, 0 },

    "ipres",{ OP_NONE },                                        { TYPE_NONE, 0xed54, CPU_RABBIT, 0 },
    "ipset",{ OP_NUMBER },                                      { TYPE_IPSET, 0xed46, CPU_RABBIT, 0 },
    
    "jc",    { OP_ABS16 },                                       { TYPE_NONE, 0xda, CPU_80OS, 0 },
    
    "jm",    { OP_ABS16 },                                       { TYPE_NONE, 0xfa, CPU_80OS, 0 },

    "jmp",   { OP_ABS16 },                                       { TYPE_NONE, 0xc3, CPU_80OS, 0 },
    
    "jnc",   { OP_ABS16 },                                       { TYPE_NONE, 0xd2, CPU_80OS, 0 },
    "jnui",  { OP_ABS16 },                                       { TYPE_NONE, 0xdd, CPU_80OS, 0 },	/* gbm 8085 */
    "jnz",   { OP_ABS16 },                                       { TYPE_NONE, 0xc2, CPU_80OS, 0 },
    "jp",    { OP_ABS16 },                                       { TYPE_NONE, 0xf2, CPU_80OS, 0 },
    "jpe",   { OP_ABS16 },                                       { TYPE_NONE, 0xea, CPU_80OS, 0 },
    "jpo",   { OP_ABS16 },                                       { TYPE_NONE, 0xe2, CPU_80OS, 0 },

    "jr",   { OP_FLAGS, OP_ABS },                               { TYPE_RELJUMP, 0x20, CPU_ALL, 0 },
    "jr",   { OP_FLAGS_RCM, OP_ABS },                           { TYPE_RELJUMP, 0xA0, CPU_RCM4000, 0 },
    "jr",   { OP_ABS },                                         { TYPE_RELJUMP, 0x18, CPU_ALL, 0 },

    "jre",  { OP_FLAGS_RCM, OP_ABS16 },                         { TYPE_RELJUMP, 0xeda3, CPU_RCM4000, 0 },
    "jre",  { OP_FLAGS, OP_ABS16 },                             { TYPE_RELJUMP, 0xedc3, CPU_RCM4000, 0 },
    "jre",  { OP_ABS16 },                                       { TYPE_RELJUMP, 0x98, CPU_RCM4000, 0 },

    "jp",   { OP_FLAGS, OP_ABS16 },                             { TYPE_FLAGS, 0xc2, CPU_ALL, 0 },
    "jp",   { OP_FLAGS_RCM, OP_ABS16 },                         { TYPE_FLAGS, 0xA2, CPU_RCM4000, 0 },
    "jp",   { OP_HL|OP_INDIR|OP_INDEX },                        { TYPE_NONE, 0xE9, CPU_ALL, 0 },
    "jp",   { OP_HL|OP_INDEX },                                 { TYPE_NONE, 0xE9, CPU_ALL, 0 },
    "jp",   { OP_ABS16 },                                       { TYPE_NONE, 0xc3, CPU_ALL|CPU_80OS, F_80OS_CHECK },
    
    "jui",  { OP_ABS16 },                                       { TYPE_NONE, 0xfd, CPU_80OS, 0 },	/* gbm 8085 */
    "jz",   { OP_ABS16 },                                       { TYPE_NONE, 0xca, CPU_80OS, 0 },

    "lcall",{ OP_ABS24 },                                       { TYPE_NONE, 0xcf, CPU_RABBIT, 0 },
    "ljp",  { OP_ABS24 },                                       { TYPE_NONE, 0xc7, CPU_RABBIT, 0 },


    "ld",   { OP_BC|OP_INDIR, OP_A },                           { TYPE_NONE, 0x02, CPU_ALL, F_IO }, /* ld (bc),a */
    "ld",   { OP_DE|OP_INDIR, OP_A },                           { TYPE_NONE, 0x12, CPU_ALL, F_IO }, /* ld (de),a */

    "ld",   { OP_REG8|OP_INDEX|OP_RALT, OP_REG8|OP_INDEX},      { TYPE_LD8, 0x40, CPU_ALL, F_ALL|F_ALTDWHL, 0, 0, RCM_EMU_INCREMENT, 0 }, /* ld r8,r8 */
    "ld",   { OP_REG8|OP_INDEX|OP_RALT, OP_REG8|OP_INDEX},      { TYPE_LD8, 0x7f40, CPU_RCM4000, F_ALL|F_ALTDWHL }, /* ld r8,r8 (only those without a or (hl) */

    "ld",   { OP_STD16BIT|OP_ALT, OP_BC },                      { TYPE_ARITH16, 0xed49, CPU_RABBIT, 0 }, /* ld [bc|de|hl]',bc */
    "ld",   { OP_STD16BIT|OP_ALT, OP_DE },                      { TYPE_ARITH16, 0xed41, CPU_RABBIT, 0 }, /* ld [bc|de|hl]',de */
    
    "ld",   { OP_HL|OP_RALT, OP_BC },                           { TYPE_NONE, 0x81, CPU_RCM4000, F_ALTD }, /* ld hl,bc */
    "ld",   { OP_BC|OP_RALT, OP_HL },                           { TYPE_NONE, 0x91, CPU_RCM4000, F_ALTD }, /* ld bc,hl */
    "ld",   { OP_HL|OP_RALT, OP_DE },                           { TYPE_NONE, 0xA1, CPU_RCM4000, F_ALTD }, /* ld hl,de */
    "ld",   { OP_DE|OP_RALT, OP_HL },                           { TYPE_NONE, 0xB1, CPU_RCM4000, F_ALTD }, /* ld de,hl */

    "ld",   { OP_HL|OP_INDEX|OP_RALT,OP_SP|OP_OFFSET|OP_INDIR}, { TYPE_NONE, 0xc4, CPU_RABBIT, F_ALTD }, /* ld hl,(sp+dd) */
    "ld",   { OP_SP|OP_OFFSET|OP_INDIR, OP_HL|OP_INDEX},        { TYPE_NONE, 0xd4, CPU_RABBIT, 0 }, /* ld (sp+dd),hl */

    "ld",   { OP_HL|OP_RALT, OP_IX },                           { TYPE_NONE, 0xdd7c, CPU_RABBIT, F_ALTD }, /* ld hl,ix */
    "ld",   { OP_HL|OP_RALT, OP_IY },                           { TYPE_NONE, 0xfd7c, CPU_RABBIT, F_ALTD }, /* ld hl,iy */
    "ld",   { OP_IX, OP_HL },                                   { TYPE_NONE, 0xdd7d, CPU_RABBIT, 0 }, /* ld ix,hl */
    "ld",   { OP_IY, OP_HL },                                   { TYPE_NONE, 0xfd7d, CPU_RABBIT, 0 }, /* ld iy,hl */
    "ld",   { OP_HL|OP_RALT, OP_IX|OP_OFFSET|OP_INDIR },        { TYPE_NOPREFIX, 0xe4, CPU_RABBIT, F_ALTD },  /* ld hl,(ix+dd) */
    "ld",   { OP_HL|OP_RALT, OP_IY|OP_OFFSET|OP_INDIR },        { TYPE_NOPREFIX, 0xfde4, CPU_RABBIT, F_ALTD },  /* ld hl,(iy+dd) */
    "ld",   { OP_HL|OP_RALT, OP_HL|OP_OFFSET|OP_INDIR },        { TYPE_NOPREFIX, 0xdde4, CPU_RABBIT, F_ALTD },  /* ld hl,(hl+dd) */
    "ld",   { OP_IX|OP_OFFSET|OP_INDIR, OP_HL },                { TYPE_NOPREFIX, 0xf4, CPU_RABBIT, F_IO },  /* ld (ix+dd),hl */
    "ld",   { OP_IY|OP_OFFSET|OP_INDIR, OP_HL },                { TYPE_NOPREFIX, 0xfdf4, CPU_RABBIT, F_IO },  /* ld (iy+dd),hl */
    "ld",   { OP_HL|OP_OFFSET|OP_INDIR, OP_HL },                { TYPE_NONE, 0xddf4, CPU_RABBIT, F_IO },  /* ld (hl+dd),hl */



    "ld",   { OP_HL|OP_INDEX|OP_RALT, OP_ADDR },                { TYPE_NONE, 0x2A, CPU_NOTGB80, F_ALL }, /* ld hl,(xx)  */
    "ld",   { OP_ARITH16|OP_INDEX|OP_RALT, OP_ADDR },           { TYPE_ARITH16, 0xED4B, CPU_ZILOG|CPU_RABBIT, F_ALL }, /* ld bc,de,sp,(xx) */

    "ld",   { OP_A|OP_RALT, OP_IDX32|OP_INDIR|OP_OFFSHL },      { TYPE_IDX32, 0x8b, CPU_RCM4000, F_ALTD }, /* ld a,(ps+hl) */
    "ld",   { OP_A|OP_RALT, OP_IDX32|OP_OFFSET|OP_INDIR },      { TYPE_IDX32, 0x8d, CPU_RCM4000, F_ALTD }, /* ld a,(ps+d) */
    "ld",   { OP_IDX32|OP_OFFSHL|OP_INDIR,OP_A },               { TYPE_IDX32, 0x8c, CPU_RCM4000, 0 }, /* ld (pd+hl),a */
    "ld",   { OP_IDX32|OP_OFFSET|OP_INDIR,OP_A },               { TYPE_IDX32, 0x8e, CPU_RCM4000, 0 }, /* ld (pd+d),a */
    "ld",   { OP_HL|OP_RALT, OP_IDX32|OP_OFFSBC|OP_INDIR },     { TYPE_IDX32, 0xed06, CPU_RCM4000, F_ALTD }, /* ld hl,(ps+bc) */
    "ld",   { OP_IDX32|OP_OFFSBC|OP_INDIR, OP_HL },             { TYPE_IDX32, 0xed07, CPU_RCM4000, 0 }, /* ld (pd+bc),hl */
    "ld",   { OP_HL|OP_RALT, OP_SP|OP_OFFSHL|OP_INDIR },        { TYPE_IDX32, 0xedfe, CPU_RCM4000, F_ALTD }, /* ld hl,(sp+hl) */
    "ld",   { OP_HL|OP_RALT, OP_IDX32|OP_OFFSET|OP_INDIR },     { TYPE_IDX32, 0x85, CPU_RCM4000, F_ALTD }, /* ld hl,(ps+d) */
    "ld",   { OP_IDX32|OP_OFFSET|OP_INDIR, OP_HL },             { TYPE_IDX32, 0x86, CPU_RCM4000, 0 }, /* ld (pd+d),hl */
    "ld",   { OP_IDX32, OP_SP|OP_OFFSET|OP_INDIR },             { TYPE_IDX32, 0xed04, CPU_RCM4000, 0 }, /* ld pd,(sp+n) */
    "ld",   { OP_SP|OP_OFFSET|OP_INDIR, OP_IDX32 },             { TYPE_NONE, 0xed05, CPU_RCM4000, 0 }, /* ld (sp+dd),pw */
   

    "ld",   { OP_BCDE|OP_RALT, OP_HL|OP_INDIR },                { TYPE_NONE, 0xdd1a, CPU_RCM4000, F_ALTD },  /* ld bcde,(hl) */
    "ld",   { OP_HL|OP_INDIR, OP_BCDE },                        { TYPE_NONE, 0xdd1b, CPU_RCM4000, 0 }, /* ld (hl),bcde */
    "ld",   { OP_BCDE|OP_RALT, OP_ADDR },                       { TYPE_NONE, 0x93, CPU_RCM4000, F_ALTD },  /* ld bcde,(xx) */
    "ld",   { OP_ADDR, OP_BCDE },                               { TYPE_NONE, 0x83, CPU_RCM4000, 0 }, /* ld (xx),bcde */
    "ld",   { OP_BCDE|OP_RALT, OP_ABS },                        { TYPE_NONE, 0xa3, CPU_RCM4000, F_ALTD },  /* ld bcde,dd */
    "ld",   { OP_BCDE|OP_RALT, OP_SP|OP_OFFSHL|OP_INDIR },      { TYPE_NONE,  0xddfe, CPU_RCM4000, F_ALTD }, /* ld bcde,(sp+hl) */
    "ld",   { OP_BCDE|OP_RALT, OP_SP|OP_OFFSET|OP_INDIR },      { TYPE_NONE, 0xddee, CPU_RCM4000, F_ALTD },  /* ld bcde,(sp+dd) */
    "ld",   { OP_BCDE|OP_RALT, OP_IX|OP_OFFSET|OP_INDIR },      { TYPE_NONE, 0xddce, CPU_RCM4000, F_ALTD },  /* ld bcde,(ix+dd) */
    "ld",   { OP_BCDE|OP_RALT, OP_IY|OP_OFFSET|OP_INDIR },      { TYPE_NONE, 0xddde, CPU_RCM4000, F_ALTD },  /* ld bcde,(iy+dd) */
    "ld",   { OP_SP|OP_OFFSET|OP_INDIR, OP_BCDE },              { TYPE_NONE, 0xddef, CPU_RCM4000, 0 },  /* ld (sp+dd),bcde */
    "ld",   { OP_BCDE|OP_RALT, OP_IDX32|OP_OFFSHL|OP_INDIR },   { TYPE_IDX32, 0xdd0c, CPU_RCM4000, F_ALTD }, /* ld bcde,(ps+hl) */
    "ld",   { OP_BCDE|OP_RALT, OP_IDX32|OP_OFFSET|OP_INDIR },   { TYPE_IDX32, 0xdd0e, CPU_RCM4000, F_ALTD }, /* ld bcde,(ps+d) */
    "ld",   { OP_BCDE|OP_RALT, OP_IDX32 },                      { TYPE_IDX32, 0xddcd, CPU_RCM4000, F_ALTD }, /* ld bcde,ps */
    "ld",   { OP_IDX32|OP_OFFSHL|OP_INDIR, OP_BCDE },           { TYPE_IDX32, 0xdd0d, CPU_RCM4000, 0 }, /* ld (pd+hl),bcde */
    "ld",   { OP_IDX32|OP_OFFSET|OP_INDIR, OP_BCDE },           { TYPE_IDX32, 0xdd0f, CPU_RCM4000, 0 }, /* ld (pd+d),bcde */
    "ld",   { OP_IDX32, OP_BCDE },                              { TYPE_IDX32, 0xdd8d, CPU_RCM4000, 0 }, /* ld pd,bcde */



   

    "ld",   { OP_JKHL|OP_RALT, OP_HL|OP_INDIR },                { TYPE_NONE, 0xfd1a, CPU_RCM4000, F_ALTD },  /* ld jkhl,(hl) */
    "ld",   { OP_HL|OP_INDIR, OP_JKHL },                        { TYPE_NONE, 0xfd1b, CPU_RCM4000, 0 }, /* ld (hl),jkhl */
    "ld",   { OP_JKHL|OP_RALT, OP_ADDR },                       { TYPE_NONE, 0x94, CPU_RCM4000, F_ALTD },  /* ld jkhl,(xx) */
    "ld",   { OP_ADDR, OP_JKHL },                               { TYPE_NONE, 0x84, CPU_RCM4000, 0 }, /* ld (xx),jkhl */
    "ld",   { OP_JKHL|OP_RALT, OP_ABS },                        { TYPE_NONE, 0xa4, CPU_RCM4000, F_ALTD },  /* ld jkhl,dd */
    "ld",   { OP_JKHL|OP_RALT, OP_SP|OP_OFFSHL|OP_INDIR },      { TYPE_NONE,  0xddfe, CPU_RCM4000, F_ALTD }, /* ld jkhl,(sp+hl) */
    "ld",   { OP_JKHL|OP_RALT, OP_SP|OP_OFFSET|OP_INDIR },      { TYPE_NONE, 0xfdee, CPU_RCM4000, F_ALTD },  /* ld jkhl,(sp+dd) */
    "ld",   { OP_JKHL|OP_RALT, OP_IX|OP_OFFSET|OP_INDIR },      { TYPE_NONE, 0xfdce, CPU_RCM4000, F_ALTD },  /* ld jkhl,(ix+dd) */
    "ld",   { OP_JKHL|OP_RALT, OP_IY|OP_OFFSET|OP_INDIR },      { TYPE_NONE, 0xfdde, CPU_RCM4000, F_ALTD },  /* ld jkhl,(iy+dd) */
    "ld",   { OP_SP|OP_OFFSET|OP_INDIR, OP_JKHL },              { TYPE_NONE, 0xfdef, CPU_RCM4000, 0 },  /* ld (sp+dd),jkhl */
    "ld",   { OP_JKHL|OP_RALT, OP_IDX32|OP_OFFSHL|OP_INDIR },   { TYPE_IDX32, 0xfd0c, CPU_RCM4000, F_ALTD }, /* ld jkhl,(ps+hl) */
    "ld",   { OP_JKHL|OP_RALT, OP_IDX32|OP_OFFSET|OP_INDIR },   { TYPE_IDX32, 0xfd0e, CPU_RCM4000, F_ALTD }, /* ld jkhl,(ps+d) */
    "ld",   { OP_JKHL|OP_RALT, OP_IDX32 },                      { TYPE_IDX32, 0xfdcd, CPU_RCM4000, F_ALTD }, /* ld jkhl,ps */
    "ld",   { OP_IDX32|OP_OFFSHL|OP_INDIR, OP_JKHL },           { TYPE_IDX32, 0xfd0d, CPU_RCM4000, 0 }, /* ld (pd+hl),jkhl */
    "ld",   { OP_IDX32|OP_OFFSET|OP_INDIR, OP_JKHL },           { TYPE_IDX32, 0xfd0f, CPU_RCM4000, 0 }, /* ld (pd+d),jkhl */
    "ld",   { OP_IDX32, OP_JKHL },                              { TYPE_IDX32, 0xfd8d, CPU_RCM4000, 0 }, /* ld pd,bcde */


    "ld",   { OP_IDX32, OP_IDX32|OP_INDIR|OP_OFFSHL },          { TYPE_IDX32R, 0x6d0a, CPU_RCM4000, 0 }, /* ld pd,(ps+hl) */
    "ld",   { OP_IDX32, OP_IDX32|OP_INDIR|OP_OFFSET },          { TYPE_IDX32R, 0x6d08, CPU_RCM4000, 0 }, /* ld pd,(ps+dd) */
    "ld",   { OP_IDX32, OP_IDX32|OP_OFFSHL },                   { TYPE_IDX32R, 0x6d0e, CPU_RCM4000, 0 }, /* ld pd,ps+hl */
    "ld",   { OP_IDX32, OP_IDX32|OP_OFFSDE },                   { TYPE_IDX32R, 0x6d06, CPU_RCM4000, 0 }, /* ld pd,ps+de */
    "ld",   { OP_IDX32, OP_IDX32|OP_OFFSIX },                   { TYPE_IDX32R, 0x6d04, CPU_RCM4000, 0 }, /* ld pd,ps+ix */
    "ld",   { OP_IDX32, OP_IDX32|OP_OFFSIY },                   { TYPE_IDX32R, 0x6d05, CPU_RCM4000, 0 }, /* ld pd,ps+iy */
    "ld",   { OP_IDX32, OP_IDX32 },                             { TYPE_IDX32R, 0x6d07, CPU_RCM4000, 0 }, /* ld pd,ps */
    "ld",   { OP_IDX32, OP_IDX32|OP_OFFSET },                   { TYPE_IDX32R, 0x6d0c, CPU_RCM4000, 0 }, /* ld pd,ps+dd */


    "ld",   { OP_BC|OP_RALT, OP_IDX32|OP_INDIR|OP_OFFSHL },     { TYPE_IDX32, 0x6d02, CPU_RCM4000, F_ALTD }, /* ld bc,(ps+hl) */
    "ld",   { OP_BC|OP_RALT, OP_IDX32|OP_INDIR|OP_OFFSET },     { TYPE_IDX32, 0x6d00, CPU_RCM4000, F_ALTD }, /* ld bc,(ps+dd) */
    "ld",   { OP_IDX32|OP_INDIR|OP_OFFSHL, OP_BC },             { TYPE_IDX32, 0x6d03, CPU_RCM4000, 0 }, /* ld (pd+hl),bc */
    "ld",   { OP_IDX32|OP_INDIR|OP_OFFSET, OP_BC },             { TYPE_IDX32, 0x6d01, CPU_RCM4000, 0 }, /* ld (pd+dd),bc */
    "ld",   { OP_DE|OP_RALT, OP_IDX32|OP_INDIR|OP_OFFSHL },     { TYPE_IDX32, 0x6d42, CPU_RCM4000, F_ALTD }, /* ld de,(ps+hl) */
    "ld",   { OP_DE|OP_RALT, OP_IDX32|OP_INDIR|OP_OFFSET },     { TYPE_IDX32, 0x6d40, CPU_RCM4000, F_ALTD }, /* ld de,(ps+dd) */
    "ld",   { OP_IDX32|OP_INDIR|OP_OFFSHL, OP_DE },             { TYPE_IDX32, 0x6d43, CPU_RCM4000, 0 }, /* ld (pd+hl),de */
    "ld",   { OP_IDX32|OP_INDIR|OP_OFFSET, OP_DE },             { TYPE_IDX32, 0x6d41, CPU_RCM4000, 0 }, /* ld (pd+dd),de */
    "ld",   { OP_IX, OP_IDX32|OP_INDIR|OP_OFFSHL },             { TYPE_IDX32, 0x6d82, CPU_RCM4000, 0 }, /* ld ix,(ps+hl) */
    "ld",   { OP_IX, OP_IDX32|OP_INDIR|OP_OFFSET },             { TYPE_IDX32, 0x6d80, CPU_RCM4000, 0 }, /* ld ix,(ps+dd) */
    "ld",   { OP_IDX32|OP_INDIR|OP_OFFSHL, OP_IX },             { TYPE_IDX32, 0x6d83, CPU_RCM4000, 0 }, /* ld (pd+hl),ix */
    "ld",   { OP_IDX32|OP_INDIR|OP_OFFSET, OP_IX },             { TYPE_IDX32, 0x6d81, CPU_RCM4000, 0 }, /* ld (pd+dd),ix */
    "ld",   { OP_IY, OP_IDX32|OP_INDIR|OP_OFFSHL },             { TYPE_IDX32, 0x6dc2, CPU_RCM4000, 0 }, /* ld iy,(ps+hl) */
    "ld",   { OP_IY, OP_IDX32|OP_INDIR|OP_OFFSET },             { TYPE_IDX32, 0x6dc0, CPU_RCM4000, 0 }, /* ld iy,(ps+dd) */
    "ld",   { OP_IDX32|OP_INDIR|OP_OFFSHL, OP_IY },             { TYPE_IDX32, 0x6dc3, CPU_RCM4000, 0 }, /* ld (pd+hl),iy */
    "ld",   { OP_IDX32|OP_INDIR|OP_OFFSET, OP_IY },             { TYPE_IDX32, 0x6dc1, CPU_RCM4000, 0 }, /* ld (pd+dd),iy */
    "ld",   { OP_IDX32|OP_INDIR|OP_OFFSHL, OP_IDX32 },          { TYPE_IDX32R, 0x6d0b, CPU_RCM4000, 0 }, /* ld (pd+hl),ps */
    "ld",   { OP_IDX32|OP_INDIR|OP_OFFSET, OP_IDX32 },          { TYPE_IDX32R, 0x6d09, CPU_RCM4000, 0 }, /* ld (pd+dd),ps */


    "ld",   { OP_IDX32, OP_ABS32 },                             { TYPE_IDX32, 0xed0c, CPU_RCM4000, 0 }, /* ld pd,klmnn */



    "ld",   { OP_XPC, OP_A },                                   { TYPE_NONE, 0xed67, CPU_RABBIT, 0 }, /* ld xpc,a */
    "ld",   { OP_A, OP_XPC },                                   { TYPE_NONE, 0xed77, CPU_RABBIT, 0 }, /* ld a,xpc */
    "ld",   { OP_HTR, OP_A },                                   { TYPE_NONE, 0xed40, CPU_RCM4000, 0 }, /* ld htr,a */
    "ld",   { OP_A, OP_HTR },                                   { TYPE_NONE, 0xed50, CPU_RCM4000, 0 }, /* ld a,htr */

    "ld",   { OP_JK|OP_RALT, OP_ABS16 },                        { TYPE_NONE, 0xa9, CPU_RCM4000, F_ALTD },  /* ld jk,xx */
    "ld",   { OP_ADDR, OP_JK  },                                { TYPE_NONE, 0x89, CPU_RCM4000, F_IO }, /* ld (xx),jk */
    "ld",   { OP_JK|OP_RALT, OP_ADDR },                         { TYPE_NONE, 0x99, CPU_RCM4000, F_ALL },  /* ld jk,(xx) */


    "ld",   { OP_A|OP_RALT, OP_BC|OP_INDIR},                    { TYPE_NONE, 0x0a, CPU_ALL, F_ALL }, /* ld a,(bc) */
    "ld",   { OP_A|OP_RALT, OP_DE|OP_INDIR},                    { TYPE_NONE, 0x1a, CPU_ALL, F_ALL }, /* ld a,(de) */
    
    "ld",   { OP_A|OP_RALT, OP_ADDR },                          { TYPE_NONE, 0x3a, CPU_ALL, F_ALL, 0, 0, 0, RCM_EMU_INCREMENT }, /* ld a,(xx) */
    "ld",   { OP_A, OP_ADDR },                                  { TYPE_NONE, 0xfa, CPU_GB80, F_ALL }, /* ld a,(xx) */

    "ld",   { OP_A|OP_RALT, OP_I },                             { TYPE_NONE, 0xed57, CPU_ZILOG|CPU_RABBIT, F_ALTD }, /* ld a,i */
    "ld",   { OP_A|OP_RALT, OP_R },                             { TYPE_NONE, 0xed5f, CPU_ZILOG|CPU_RABBIT, F_ALTD }, /* ld a,r */

    "ld",   { OP_REG8| OP_INDEX|OP_RALT, OP_ABS },              { TYPE_MISC8, 0x06, CPU_ALL, F_ALL|F_ALTDWHL },  /* ld r8, nn */
    "ld",   { OP_SP, OP_HL|OP_INDEX},                           { TYPE_NONE, 0xf9, CPU_ALL, 0 }, /* ld sp,hl */
    "ld",   { OP_ARITH16| OP_INDEX|OP_RALT, OP_ABS16 },         { TYPE_ARITH16, 0x01, CPU_ALL, F_ALTD }, /* ld hl,bc,de,xx */
    "ld",   { OP_R, OP_A },                                     { TYPE_NONE, 0xed4f, CPU_ZILOG|CPU_RABBIT, F_ALTD }, /* ld r,a */
    "ld",   { OP_I, OP_A },                                     { TYPE_NONE, 0xed47, CPU_ZILOG|CPU_RABBIT, F_ALTD }, /* ld i,a */

    "ld",   { OP_PORT, OP_A },                                  { TYPE_NONE, 0xe2, CPU_GB80, 0 }, /* ld (c),a (ff00 + c) */


    "ld",   { OP_ADDR, OP_SP },                                 { TYPE_NONE, 0xED73, CPU_ZILOG|CPU_RABBIT|CPU_GB80, F_IO, 0, 0, 0, RCM_EMU_INCREMENT }, /* ld (xx),sp */
    "ld",   { OP_ADDR, OP_SP },                                 { TYPE_NONE, 0x08, CPU_GB80, 0 }, /* ld (xx), sp */
    "ld",   { OP_ADDR, OP_HL| OP_INDEX },                       { TYPE_NONE, 0x22, CPU_NOTGB80, F_IO }, /* ld (xx),hl */
    "ld",   { OP_ADDR, OP_ARITH16 },                            { TYPE_ARITH16, 0xED43, CPU_ZILOG|CPU_RABBIT, F_IO }, /* ld (xx),bc,de,sp */
    "ld",   { OP_ADDR, OP_A },                                  { TYPE_NONE, 0x32, CPU_ALL, F_IO, 0, 0, 0, RCM_EMU_INCREMENT }, /* ld (xx),a */
    "ld",   { OP_ADDR, OP_A },                                  { TYPE_NONE, 0xea, CPU_GB80, 0 }, /* ld (xx),a (GB) */   

    "lda",  { OP_ABS16 },                                       { TYPE_NONE, 0x3a, CPU_80OS, 0 },

    "ldax", { OP_B },                                           { TYPE_NONE, 0x0a, CPU_80OS, 0 },
    "ldax", { OP_D },                                           { TYPE_NONE, 0x1a, CPU_80OS, 0 },

    "ldd",  { OP_NONE, },                                       { TYPE_NONE, 0xeda8, CPU_ZILOG|CPU_RABBIT, F_IO },
    "ldd",  { OP_A, OP_HL|OP_INDIR },                           { TYPE_NONE, 0x3a, CPU_GB80, 0 },
    "ldd",  { OP_HL|OP_INDIR,OP_A },                            { TYPE_NONE, 0x32, CPU_GB80, 0 },
    "lddr", { OP_NONE, },                                       { TYPE_NONE, 0xedb8, CPU_ZILOG|CPU_RABBIT, F_IO },
    "lddsr",{ OP_NONE },                                        { TYPE_NONE, 0xed98, CPU_RCM3000 | CPU_RCM4000, F_IO },

    "ldf", { OP_ADDR24, OP_A },                                 { TYPE_NONE, 0x8a, CPU_RCM4000, 0 }, /* ldf (lmn),a */
    "ldf", { OP_ADDR24, OP_BC },                                { TYPE_NONE, 0xed0b, CPU_RCM4000, 0 }, /* ldf (lmn),bc */
    "ldf", { OP_ADDR24, OP_DE },                                { TYPE_NONE, 0xed1b, CPU_RCM4000, 0 }, /* ldf (lmn),de */
    "ldf", { OP_ADDR24, OP_IX },                                { TYPE_NONE, 0xed2b, CPU_RCM4000, 0 }, /* ldf (lmn),ix */
    "ldf", { OP_ADDR24, OP_IY },                                { TYPE_NONE, 0xed3b, CPU_RCM4000, 0 }, /* ldf (lmn),iy */
    "ldf", { OP_ADDR24, OP_HL },                                { TYPE_NONE, 0x82, CPU_RCM4000, 0 }, /* ldf (lmn),hl */
    "ldf", { OP_ADDR24, OP_BCDE },                              { TYPE_NONE, 0xdd0b, CPU_RCM4000, 0 }, /* ldf (lmn),bcde */
    "ldf", { OP_ADDR24, OP_JKHL },                              { TYPE_NONE, 0xfd0b, CPU_RCM4000, 0 }, /* ldf (lmn),jkhl */
    "ldf", { OP_A|OP_RALT, OP_ADDR24 },                         { TYPE_NONE, 0x9a, CPU_RCM4000, F_ALTD }, /* ldf a,(lmn) */
    "ldf", { OP_BC|OP_RALT, OP_ADDR24 },                        { TYPE_NONE, 0xed0a, CPU_RCM4000, F_ALTD }, /* ldf bc,(lmn) */
    "ldf", { OP_DE|OP_RALT, OP_ADDR24 },                        { TYPE_NONE, 0xed1a, CPU_RCM4000, F_ALTD }, /* ldf de,(lmn) */
    "ldf", { OP_IX, OP_ADDR24 },                                { TYPE_NONE, 0xed2a, CPU_RCM4000, 0 }, /* ldf ix,(lmn) */
    "ldf", { OP_IY, OP_ADDR24 },                                { TYPE_NONE, 0xed3a, CPU_RCM4000, 0 }, /* ldf iy,(lmn) */
    "ldf", { OP_HL|OP_RALT, OP_ADDR24 },                        { TYPE_NONE, 0x92, CPU_RCM4000, F_ALTD }, /* ldf hl,(lmn) */
    "ldf", { OP_BCDE|OP_RALT, OP_ADDR24 },                      { TYPE_NONE, 0xdd0a, CPU_RCM4000, 0 }, /* ldf bcde,(lmn) */
    "ldf", { OP_JKHL|OP_RALT, OP_ADDR24 },                      { TYPE_NONE, 0xfd0a, CPU_RCM4000, F_ALTD }, /* ldf jkhl,(lmn) */

    "ldh",  { OP_ADDR8, OP_A },                                 { TYPE_NONE, 0xe0, CPU_GB80, 0 }, /* ld (xx),a (GB - for ff00 page) */
    "ldh",  { OP_A, OP_ADDR8 },                                 { TYPE_NONE, 0xf0, CPU_GB80, 0 }, /* ld a,(xx) */
    "ldhi", { OP_ABS },                                         { TYPE_NONE, 0x28, CPU_80OS, 0 }, /* gbm 8085 */
    "ldhl", { OP_SP, OP_ABS },                                  { TYPE_NONE, 0xf8, CPU_GB80, 0 },

    "ldi",  { OP_NONE, },                                       { TYPE_NONE, 0xeda0, CPU_ZILOG|CPU_RABBIT, F_IO },
    "ldi",  { OP_A, OP_HL|OP_INDIR },                           { TYPE_NONE, 0x2a, CPU_GB80, 0 },
    "ldi",  { OP_HL|OP_INDIR,OP_A },                            { TYPE_NONE, 0x22, CPU_GB80, 0 },
    "ldir", { OP_NONE, },                                       { TYPE_NONE, 0xedb0, CPU_ZILOG|CPU_RABBIT, F_IO },
    "ldisr",{ OP_NONE },                                        { TYPE_NONE, 0xed90, CPU_RCM3000 | CPU_RCM4000, F_IO },

    "ldl",  { OP_IDX32|OP_RALT, OP_DE },                        { TYPE_IDX32, 0xdd8f, CPU_RCM4000, F_ALTD },
    "ldl",  { OP_IDX32|OP_RALT, OP_HL },                        { TYPE_IDX32, 0xfd8f, CPU_RCM4000, F_ALTD },
    "ldl",  { OP_IDX32|OP_RALT, OP_IX },                        { TYPE_IDX32, 0xdd8c, CPU_RCM4000, F_ALTD },
    "ldl",  { OP_IDX32|OP_RALT, OP_IY },                        { TYPE_IDX32, 0xfd8c, CPU_RCM4000, F_ALTD },
    "ldl",  { OP_IDX32|OP_RALT, OP_ABS16 },                     { TYPE_IDX32, 0xed0d, CPU_RCM4000, F_ALTD },
    "ldl",  { OP_IDX32|OP_RALT, OP_SP|OP_OFFSET|OP_INDIR },     { TYPE_IDX32, 0xed03, CPU_RCM4000, F_ALTD },


    "ldp",  { OP_HL|OP_INDEX|OP_INDIR, OP_HL },                 { TYPE_EDPREF, 0xed64, CPU_RABBIT, 0 }, /* ldp (hl),hl */
    "ldp",  { OP_ADDR, OP_HL|OP_INDEX },                        { TYPE_EDPREF, 0xed65, CPU_RABBIT, 0 }, /* ldp (xx),hl */
    "ldp",  { OP_HL, OP_HL|OP_INDEX|OP_INDIR },                 { TYPE_EDPREF, 0xed6c, CPU_RABBIT, 0 }, /* ldp hl,(hl) */
    "ldp",  { OP_HL|OP_INDEX, OP_ADDR },                        { TYPE_EDPREF, 0xed6d, CPU_RABBIT, 0 }, /* ldp hl,(xx) */

    "ldsi", { OP_ABS },                                         { TYPE_NONE, 0x38, CPU_80OS, 0 },	/* gbm 8085 */
    "lhld", { OP_ABS16 },                                       { TYPE_NONE, 0x2a, CPU_80OS, 0 },
    "lhlx", { OP_NONE },                                        { TYPE_NONE, 0xed, CPU_80OS, 0 },	/* gbm 8085 */
    
    "llret",{ OP_NONE },                                        { TYPE_NONE, 0xed8b, CPU_RCM4000, 0 },
    "lret", { OP_NONE },                                        { TYPE_NONE, 0xed45, CPU_RABBIT, 0 },

    "lsddr",{ OP_NONE },                                        { TYPE_NONE, 0xedd8, CPU_RCM3000 | CPU_RCM4000, F_IO },
    "lsidr",{ OP_NONE },                                        { TYPE_NONE, 0xedd0, CPU_RCM3000 | CPU_RCM4000, F_IO },
    "lsir", { OP_NONE },                                        { TYPE_NONE, 0xedf0, CPU_RCM3000 | CPU_RCM4000, F_IO },
    "lsdr", { OP_NONE },                                        { TYPE_NONE, 0xedf8, CPU_RCM3000 | CPU_RCM4000, F_IO },

    "lxi",   { OP_RP, OP_ABS16 },                               { TYPE_RP, 0x01, CPU_80OS, 0 }, /* lxi h,b,d,sp,xx */

    "mov",   {OP_REG8, OP_REG8},                                 { TYPE_LD8, 0x40, CPU_80OS, 0 },

    "mul",  { OP_NONE },                                        { TYPE_NONE, 0xf7, CPU_RABBIT, 0 }, /* mul (hl:bc=bc*de) */

    "mult", { OP_ARITH16 },                                     { TYPE_ARITH16, 0xed4c, CPU_Z180|CPU_EZ80,0 }, /* mult bc/de/hl/sp */
    "mulu", { OP_NONE },                                        { TYPE_NONE, 0xa7, CPU_RCM4000, 0 }, /* mulu (hl:bc=bc*de) */
    
    "mvi",  { OP_REG8, OP_ABS },                                { TYPE_MISC8, 0x06, CPU_80OS, 0 },  /* mvi r8, nn */
    
    "neg",  { OP_NONE, },                                       { TYPE_NONE, 0xed44, CPU_ZILOG|CPU_RABBIT, F_ALTD },
    "neg",  { OP_HL|OP_RALT, },                                 { TYPE_NONE, 0x44, CPU_RCM4000, F_ALTD },
    "neg",  { OP_JKHL },                                        { TYPE_NONE, 0xfd4d, CPU_RCM4000, 0 },
    "neg",  { OP_BCDE },                                        { TYPE_NONE, 0xdd4d, CPU_RCM4000, 0 },

    "nop",  { OP_NONE, },                                       { TYPE_NONE, 0x00, CPU_ALL|CPU_80OS, 0 },

    "or",   { OP_REG8 | OP_INDEX },                             { TYPE_ARITH8, 0xb0, CPU_ALL, F_ALL,0, 0, RCM_EMU_INCREMENT },
    "or",   { OP_REG8 | OP_INDEX },                             { TYPE_ARITH8, 0x7fb0, CPU_RCM4000, F_ALL },
    "or",   { OP_A|OP_RALT, OP_REG8 | OP_INDEX },               { TYPE_ARITH8, 0xb0, CPU_ALL, F_ALL,0, 0, RCM_EMU_INCREMENT },
    "or",   { OP_A|OP_RALT, OP_REG8 | OP_INDEX },               { TYPE_ARITH8, 0x7fb0, CPU_RCM4000, F_ALL },
    "or",   { OP_A|OP_RALT, OP_ABS },                           { TYPE_NONE,  0xf6, CPU_ALL, F_ALTD },
    "or",   { OP_ABS },                                         { TYPE_NONE,  0xf6, CPU_ALL, F_ALTD },
    "or",   { OP_HL|OP_INDEX|OP_RALT, OP_DE },                  { TYPE_NONE, 0xEC, CPU_RABBIT, F_ALTD }, /* or hl/ix/iy,de */
    "or",   { OP_JKHL|OP_RALT, OP_BCDE },                       { TYPE_NONE, 0xedf6, CPU_RCM4000, F_ALTD },
    
    "ora",  { OP_REG8 },                                        { TYPE_ARITH8, 0xb0, CPU_80OS, 0 },
    
    "ori",   { OP_ABS },                                        { TYPE_NONE,  0xf6, CPU_80OS, 0 },

    "otdm", { OP_NONE },                                        { TYPE_NONE, 0xed8b, CPU_Z180|CPU_EZ80, 0 },
    "otdmr",{ OP_NONE },                                        { TYPE_NONE, 0xed9b, CPU_Z180|CPU_EZ80, 0 },
    "otdr", { OP_NONE, },                                       { TYPE_NONE, 0xedbb, CPU_ZILOG, 0 },

    "otim", { OP_NONE },                                        { TYPE_NONE, 0xed83, CPU_Z180, 0 },
    "otimr",{ OP_NONE },                                        { TYPE_NONE, 0xed93, CPU_Z180, 0 },

    "otir", { OP_NONE, },                                       { TYPE_NONE, 0xedb3, CPU_ZILOG, 0 },

    "out",  { OP_PORT, OP_REG8},                                { TYPE_MISC8, 0xed41, CPU_ZILOG, 0 },  
    "out",  { OP_PORT       },                                  { TYPE_NONE, 0xed71, CPU_ZILOG, 0 }, /* Undoc out(c) */ 
    "out",  { OP_PORT, OP_F },                                  { TYPE_NONE, 0xed71, CPU_ZILOG, 0 }, /* Undoc out (c),f */
    "out",  { OP_PORT, OP_NUMBER },                             { TYPE_OUT_C_0, 0xed71, CPU_ZILOG, 0 }, /* Undoc out (c),0 */
    "out",  { OP_ADDR8, OP_A },                                 { TYPE_NONE, 0xD3, CPU_8080|CPU_ZILOG, 0 },   /* out (xx),a */
    "out",  { OP_ABS },                                         { TYPE_NONE, 0xd3, CPU_80OS, 0 },   /* out xx */
    "out0", { OP_ADDR8, OP_REG8 },                              { TYPE_MISC8, 0xed01, CPU_Z180|CPU_EZ80, 0 }, /* out0 (xx),r */



    "outd", { OP_NONE, },                                       { TYPE_NONE, 0xedab, CPU_ZILOG, 0 },
    "outi", { OP_NONE, },                                       { TYPE_NONE, 0xeda3, CPU_ZILOG, 0 },

    "pchl", { OP_NONE },                                        { TYPE_NONE, 0xE9, CPU_80OS, 0 },
    
    "pop",  { OP_PUSHABLE|OP_INDEX|OP_RALT },                   { TYPE_ARITH16, 0xc1, CPU_ALL, F_ALTD },
    "pop",  { OP_RP_PUSHABLE },                                 { TYPE_RP, 0xc1, CPU_80OS, 0 },
    "pop",  { OP_IP },                                          { TYPE_NONE, 0xed7e, CPU_RABBIT, 0 } ,
    "pop",  { OP_BCDE|OP_RALT },                                { TYPE_NONE, 0xddf1, CPU_RCM4000, F_ALTD },
    "pop",  { OP_JKHL|OP_RALT },                                { TYPE_NONE, 0xfdf1, CPU_RCM4000, F_ALTD },
    "pop",  { OP_SU },                                          { TYPE_NONE, 0xed6f, CPU_RCM4000, 0 }, /* pop su */
    "pop",  { OP_IDX32 },                                       { TYPE_IDX32, 0xedc1, CPU_RCM4000 , 0 },

    "push", { OP_PUSHABLE|OP_INDEX|OP_RALT },                   { TYPE_ARITH16, 0xc5, CPU_ALL, F_ALTD },
    "push", { OP_RP_PUSHABLE },                                 { TYPE_RP, 0xc5, CPU_80OS, 0 },
    "push", { OP_IP },                                          { TYPE_NONE, 0xed76, CPU_RABBIT, 0 } ,
    "push", { OP_BCDE|OP_RALT },                                { TYPE_NONE, 0xddf5, CPU_RCM4000, F_ALTD },
    "push", { OP_JKHL|OP_RALT },                                { TYPE_NONE, 0xfdf5, CPU_RCM4000, F_ALTD },
    "push", { OP_IDX32 },                                       { TYPE_IDX32, 0xedc5, CPU_RCM4000 , 0 },
    "push", { OP_SU },                                          { TYPE_NONE, 0xed66, CPU_RCM4000, 0 }, /* push su */
    "push", { OP_ABS16 },                                       { TYPE_NONE, 0xeda5, CPU_RCM4000, 0 },  /* push xx */
    
    "ral",  { OP_NONE },                                        { TYPE_NONE, 0x17, CPU_80OS, 0 },
    "rar",  { OP_NONE, },                                       { TYPE_NONE, 0x1f, CPU_80OS, 0 },
    
    "rc",   { OP_NONE },                                        { TYPE_NONE, 0xd8,  CPU_80OS, 0 },

    "rdel",  { OP_NONE },                                       { TYPE_NONE, 0x18, CPU_80OS, 0 },   /* gbm 8085 */
   
    "rdmode",{ OP_NONE },                                       { TYPE_NONE, 0xed7f, CPU_RCM3000|CPU_RCM4000, 0 },

    "res",  { OP_NUMBER, OP_REG8|OP_INDEX|OP_RALT},             { TYPE_BIT, 0xCB80, CPU_NOT8080, F_ALL|F_ALTDWHL },

    "ret",  { OP_NONE, },                                       { TYPE_NONE, 0xc9,   CPU_ALL|CPU_80OS, 0 },
    "ret",  { OP_FLAGS },                                       { TYPE_FLAGS, 0xc0, CPU_ALL, 0 },
    "retn", { OP_NONE, },                                       { TYPE_NONE, 0xed45, CPU_ZILOG, 0 },
    "reti", { OP_NONE, },                                       { TYPE_NONE, 0xed4d, CPU_ZILOG|CPU_RABBIT|CPU_GB80, 0, 0, 0, 0, RCM_EMU_INCREMENT },
    "reti", { OP_NONE, },                                       { TYPE_NONE, 0xd9,   CPU_GB80, 0 },

    "rim",  { OP_NONE },                                        { TYPE_NONE, 0x20, CPU_80OS, 0 },   /* gbm 8085 */

    "rl",   { OP_REG8|OP_RALT },                                { TYPE_ARITH8, 0xcb10, CPU_NOT8080, F_ALL },
    "rl",   { OP_BC|OP_RALT },                                  { TYPE_NONE, 0x62, CPU_RCM4000, F_ALTD },
    "rl",   { OP_DE|OP_RALT },                                  { TYPE_NONE, 0xf3, CPU_RABBIT, F_ALTD },
    "rl",   { OP_HL|OP_RALT },                                  { TYPE_NONE, 0x42, CPU_RCM4000, F_ALTD },
    "rla",  { OP_NONE, },                                       { TYPE_NONE, 0x17, CPU_ALL, F_ALTD },

    "rlb",  { OP_A, OP_BCDE },                                  { TYPE_NONE, 0xdd6f, CPU_RCM4000, 0 },
    "rlb",  { OP_A, OP_JKHL },                                  { TYPE_NONE, 0xfd6f, CPU_RCM4000, 0 },

    "rlc",  { OP_REG8|OP_RALT },                                { TYPE_ARITH8, 0xcb00, CPU_NOT8080, F_ALTD },
    "rlc",  { OP_DE|OP_RALT },                                  { TYPE_NONE, 0x50, CPU_RCM4000, F_ALTD },
    "rlc",  { OP_BC|OP_RALT },                                  { TYPE_NONE, 0x60, CPU_RCM4000, F_ALTD },
    "rlc",  { OP_NONE, },                                       { TYPE_NONE, 0x07, CPU_80OS, 0 },
    "rlca", { OP_NONE, },                                       { TYPE_NONE, 0x07,   CPU_ALL, F_ALTD },

    "rld",  { OP_NONE, },                                       { TYPE_NONE, 0xed6f, CPU_ZILOG|CPU_RABBIT, 0, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, 0 },

    "rnc",  { OP_NONE },                                        { TYPE_NONE, 0xd0,  CPU_80OS, 0 },
    "rnz",  { OP_NONE },                                        { TYPE_NONE, 0xc0,  CPU_80OS, 0 },
    
    "rm",   { OP_NONE },                                        { TYPE_NONE, 0xf8,  CPU_80OS, 0 },
    "rp",   { OP_NONE },                                        { TYPE_NONE, 0xf0,  CPU_80OS, 0 },
    "rpe",  { OP_NONE },                                        { TYPE_NONE, 0xe8,  CPU_80OS, 0 },
    "rpo",  { OP_NONE },                                        { TYPE_NONE, 0xe0,  CPU_80OS, 0 },
    
    "rr",   { OP_REG8|OP_RALT },                                { TYPE_ARITH8, 0xcb18, CPU_NOT8080, F_ALL },
    "rr",   { OP_DE|OP_RALT },                                  { TYPE_NONE, 0xfb, CPU_RABBIT, F_ALTD },
    "rr",   { OP_BC|OP_RALT },                                  { TYPE_NONE, 0x63, CPU_RCM4000, F_ALTD },
    "rr",   { OP_HL|OP_INDEX|OP_RALT},                          { TYPE_NONE, 0xfc, CPU_RABBIT, F_ALTD },

    "rra",  { OP_NONE, },                                       { TYPE_NONE, 0x1f, CPU_ALL, F_ALTD },

    "rrb",  { OP_A, OP_BCDE },                                  { TYPE_NONE, 0xdd7f, CPU_RCM4000, 0 },
    "rrb",  { OP_A, OP_JKHL },                                  { TYPE_NONE, 0xfd7f, CPU_RCM4000, 0 },


    "rrc",  { OP_REG8|OP_RALT, },                               { TYPE_ARITH8, 0xcb08, CPU_NOT8080, F_ALTD },
    "rrc",  { OP_BC|OP_RALT, },                                 { TYPE_NONE, 0x61, CPU_RCM4000, F_ALTD },
    "rrc",  { OP_DE|OP_RALT, },                                 { TYPE_NONE, 0x51, CPU_RCM4000, F_ALTD },
    "rrc",  { OP_NONE, },                                       { TYPE_NONE, 0x0f, CPU_80OS, 0 },
    "rrca", { OP_NONE, },                                       { TYPE_NONE, 0x0f, CPU_ALL, F_ALTD },

    "rrd",  { OP_NONE, },                                       { TYPE_NONE, 0xed67, CPU_ZILOG|CPU_RABBIT, 0, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, RCM_EMU_LIBRARY, 0 },
    "rst",  { OP_NUMBER },                                      { TYPE_RST,  0xc7, CPU_ALL|CPU_80OS, 0 },  /* RCM doesn't support 0, 8, 30 */

    "rstv", { OP_NONE },                                        { TYPE_NONE, 0xcb, CPU_80OS, 0 },	/* gbm 8085 */

    "rz",  { OP_NONE, },                                        { TYPE_NONE, 0xc8,   CPU_80OS, 0 },
    
    "sbb",  { OP_REG8 },                                        { TYPE_ARITH8, 0x98, CPU_80OS },
    
    "sbc",  { OP_REG8 | OP_INDEX, },                            { TYPE_ARITH8, 0x98, CPU_ALL, F_ALL, 0, 0, RCM_EMU_INCREMENT },
    "sbc",  { OP_REG8 | OP_INDEX, },                            { TYPE_ARITH8, 0x7f98, CPU_RCM4000, F_ALL },
    "sbc",  { OP_A|OP_RALT, OP_REG8 | OP_INDEX },               { TYPE_ARITH8, 0x98, CPU_ALL, F_ALTD, 0, 0, RCM_EMU_INCREMENT },
    "sbc",  { OP_A|OP_RALT, OP_REG8 | OP_INDEX },               { TYPE_ARITH8, 0x7f98, CPU_RCM4000, F_ALTD },
    "sbc",  { OP_A|OP_RALT, OP_ABS },                           { TYPE_NONE, 0xde, CPU_ALL , F_ALTD},
    "sbc",  { OP_ABS },                                         { TYPE_NONE, 0xde, CPU_ALL, F_ALTD },
    "sbc",  { OP_HL|OP_RALT, OP_ARITH16 },                      { TYPE_ARITH16, 0xed42, CPU_ZILOG|CPU_RABBIT, F_ALTD },
    
    "sbi",  { OP_ABS },                                         { TYPE_NONE, 0xde, CPU_80OS, 0 },

    "sbox", { OP_A },                                           { TYPE_NONE, 0xed02, CPU_RCM4000, F_ALTD },


    "scf",  { OP_NONE, },                                       { TYPE_NONE, 0x37, CPU_ALL, F_ALTD },


    "set",  { OP_NUMBER, OP_REG8|OP_INDEX|OP_RALT},             { TYPE_BIT, 0xCBC0, CPU_NOT8080, F_ALL|F_ALTDWHL },

    "setsysp", { OP_ABS16 },                                    { TYPE_NONE, 0xedb1, CPU_RCM4000, 0 },
    "setusr",  { OP_NONE },                                     { TYPE_NONE, 0xed6f, CPU_RCM3000|CPU_RCM4000 },
    "setusrp", { OP_ABS16 },                                    { TYPE_NONE, 0xedbf, CPU_RCM4000, 0 },
    
    "shld",  { OP_ABS16 },                                      { TYPE_NONE, 0x22, CPU_80OS, 0 }, 
    "shlx",  { OP_NONE },                                       { TYPE_NONE, 0xd9, CPU_80OS, 0 },   /* gbm 8085 */

    "sim",  { OP_NONE },                                        { TYPE_NONE, 0x30, CPU_80OS, 0 },   /* gbm 8085 */

    "sla",  { OP_REG8 },                                        { TYPE_ARITH8, 0xcb20, CPU_NOT8080, F_ALTD },
    "sll",  { OP_REG8, },                                       { TYPE_ARITH8, 0xcb30, CPU_Z80, 0 }, /* Undoc */

    "slp",  { OP_NONE },                                        { TYPE_NONE, 0xed76, CPU_Z180|CPU_EZ80 }, 

    "sphl",  { OP_NONE, },                                      { TYPE_NONE, 0xf9, CPU_80OS, 0 },
    
    "sra",  { OP_REG8|OP_RALT, },                               { TYPE_ARITH8, 0xcb28, CPU_NOT8080, F_ALTD },


    "srl",  { OP_REG8|OP_RALT, },                               { TYPE_ARITH8, 0xcb38, CPU_ZILOG|CPU_RABBIT|CPU_GB80, F_ALTD },

    "sta",  { OP_ABS16 },                                       { TYPE_NONE, 0x32, CPU_80OS, 0 },
    
    "stax", { OP_B },                                           { TYPE_NONE, 0x02, CPU_80OS, 0 },
    "stax", { OP_D },                                           { TYPE_NONE, 0x12, CPU_80OS, 0 },
    
    "stc",  { OP_NONE, },                                       { TYPE_NONE, 0x37, CPU_80OS, 0 },
    
    "stop", { OP_NONE },                                        { TYPE_NONE, 0x10, CPU_GB80, 0 },

    "sub",  { OP_HL|OP_RALT, OP_DE },                           { TYPE_NONE, 0x55, CPU_RCM4000, F_ALTD },
    "sub",  { OP_HL|OP_RALT, OP_JK },                           { TYPE_NONE, 0x45, CPU_RCM4000, F_ALTD },
    "sub",  { OP_JKHL|OP_RALT, OP_BCDE },                       { TYPE_NONE, 0xedd6, CPU_RCM4000, F_ALTD},


    "sub",  { OP_REG8|OP_INDEX },                               { TYPE_ARITH8, 0x90, CPU_ALL|CPU_80OS, F_ALTD, 0, 0, RCM_EMU_INCREMENT },
    "sub",  { OP_REG8|OP_INDEX },                               { TYPE_ARITH8, 0x7f90, CPU_RCM4000, F_ALTD },
    "sub",  { OP_A|OP_RALT, OP_REG8|OP_INDEX },                 { TYPE_ARITH8, 0x90, CPU_ALL,F_ALTD,0,0,RCM_EMU_INCREMENT },
    "sub",  { OP_A|OP_RALT, OP_REG8|OP_INDEX },                 { TYPE_ARITH8, 0x7f90, CPU_RCM4000,F_ALTD },
    "sub",  { OP_A|OP_RALT, OP_ABS },                           { TYPE_NONE, 0xd6, CPU_ALL, F_ALTD },
    "sub",  { OP_ABS },                                         { TYPE_NONE, 0xd6, CPU_ALL, F_ALTD },

    "sui",  { OP_ABS },                                         { TYPE_NONE, 0xd6, CPU_80OS, 0 },
    
    "sures",   { OP_NONE },                                     { TYPE_NONE, 0xed7d, CPU_RCM3000|CPU_RCM4000, 0 },

    "swap",  { OP_REG8 },                                       { TYPE_ARITH8, 0xcb30, CPU_GB80, 0 },

    "syscall", { OP_NONE },                                     { TYPE_NONE, 0xed65, CPU_RCM3000|CPU_RCM4000, 0 },
    "sysret",  { OP_NONE },                                     { TYPE_NONE, 0xed83, CPU_RCM4000, 0 },


    "test", { OP_HL|OP_INDEX },                                 { TYPE_NONE, 0x4c, CPU_RCM4000, F_ALTD },
    "test", { OP_BC },                                          { TYPE_NONE, 0xed4c, CPU_RCM4000, F_ALTD },
    "test", { OP_BCDE },                                        { TYPE_NONE, 0xdd5c, CPU_RCM4000, F_ALTD },
    "test", { OP_JKHL },                                        { TYPE_NONE, 0xfd5c, CPU_RCM4000, F_ALTD },

    "tst",  { OP_REG8 },                                        { TYPE_MISC8, 0xed04, CPU_Z180|CPU_EZ80, 0 },  /* Z180 TST r */
    "tst",  { OP_A, OP_REG8 },                                  { TYPE_MISC8, 0xed04, CPU_Z180|CPU_EZ80, 0 },  /* Z180 TST a,r */
    "tst",  { OP_A, OP_ABS },                                   { TYPE_NONE, 0xed64, CPU_Z180|CPU_EZ80, 0 },  /* Z180 TST a,xx */
    "tst",  { OP_ABS },                                         { TYPE_NONE, 0xed64, CPU_Z180|CPU_EZ80, 0 },  /* Z180 TST xx */
    "tstio",{ OP_ABS },                                         { TYPE_NONE, 0xed74, CPU_Z180|CPU_EZ80, 0 },  /* Z180 TSTIO xx */

    "uma",  { OP_NONE },                                        { TYPE_NONE, 0xedc0, CPU_RCM3000|CPU_RCM4000, 0 },
    "ums",  { OP_NONE },                                        { TYPE_NONE, 0xedc8, CPU_RCM3000|CPU_RCM4000, 0 },

    "xchg",   { OP_NONE },                                      { TYPE_NONE, 0xEB, CPU_80OS, 0 },
    
    "xor",  { OP_JKHL|OP_RALT, OP_BCDE },                       { TYPE_NONE, 0xedee, CPU_RCM4000, F_ALTD },
    "xor",  { OP_REG8|OP_INDEX },                               { TYPE_ARITH8, 0xa8, CPU_ALL, F_ALTD, 0, 0, RCM_EMU_INCREMENT },
    "xor",  { OP_REG8|OP_INDEX },                               { TYPE_ARITH8, 0x7fa8, CPU_RCM4000, F_ALTD },
    "xor",  { OP_A|OP_RALT, OP_REG8|OP_INDEX },                 { TYPE_ARITH8, 0xa8, CPU_ALL, F_ALTD, 0, 0, RCM_EMU_INCREMENT },
    "xor",  { OP_A|OP_RALT, OP_REG8|OP_INDEX },                 { TYPE_ARITH8, 0x7fa8, CPU_RCM4000, F_ALTD },
    "xor",  { OP_A|OP_RALT, OP_ABS },                           { TYPE_NONE, 0xEE, CPU_ALL, F_ALTD },
    "xor",  { OP_ABS },                                         { TYPE_NONE, 0xEE, CPU_ALL, F_ALTD },
    "xor",  { OP_HL|OP_RALT, OP_DE },                           { TYPE_NONE, 0x54, CPU_RCM4000, F_ALTD }, /* xor hl,de */
    
    "xra",  { OP_REG8 },                                        { TYPE_ARITH8, 0xa8, CPU_80OS, 0 },
    
    "xri",  { OP_ABS },                                         { TYPE_NONE, 0xee, CPU_80OS, 0 },
    
    "xthl",  { OP_NONE, },                                      { TYPE_NONE, 0xe3, CPU_80OS, 0 },
};

int mnemonic_cnt=sizeof(mnemonics)/sizeof(mnemonics[0]);

char *cpu_copyright="vasm 8080/gbz80/z80/z180/rcmX000 cpu backend 0.4 (c) 2007,2009 Dominic Morris";
char *cpuname = "z80";
int bitsperbyte = 8;
int bytespertaddr = 2;

/* Configuration options */
static int z80asm_compat = 0;
static int cpu_type = CPU_Z80;
static int swapixiy = 0;
static int rcmemu = 0;

/* Variables set by special parsing */
static int altd_enabled = 0;
static int ioi_enabled = 0;
static int ioe_enabled = 0;
static int modifier;  /* set by find_base() */


struct {
    char         *name;
    int           op;
    int           reg;
    int           cpu;
} registers[] = {
    { "bc'",  OP_BC|OP_PUSHABLE|OP_STD16BIT|OP_ALT, REG_BC | REG_ALT, CPU_RABBIT },
    { "de'",  OP_DE|OP_PUSHABLE|OP_STD16BIT|OP_ALT, REG_DE | REG_ALT, CPU_RABBIT },
    { "hl'",  OP_HL|OP_PUSHABLE|OP_STD16BIT|OP_ALT, REG_HL | REG_ALT, CPU_RABBIT },
    { "af'",  OP_AF|OP_PUSHABLE|OP_ALT, REG_AF | REG_ALT, CPU_RABBIT },

    { "bc",   OP_BC|OP_STD16BIT|OP_ARITH16|OP_PUSHABLE, REG_BC, CPU_ALL },
    { "hl",   OP_HL|OP_STD16BIT|OP_ARITH16|OP_PUSHABLE, REG_HL, CPU_ALL },
    { "de",   OP_DE|OP_STD16BIT|OP_ARITH16|OP_PUSHABLE, REG_DE, CPU_ALL },
    { "sp",   OP_SP|OP_ARITH16|OP_RP, REG_SP, CPU_ALL },
    { "af",   OP_AF|OP_PUSHABLE, REG_AF, CPU_ALL },

    { "ixl",  OP_REG8|OP_INDEX, REG_L | REG_IX, CPU_ZILOG },
    { "ixh",  OP_REG8|OP_INDEX, REG_H | REG_IX, CPU_ZILOG },
    { "iyl",  OP_REG8|OP_INDEX, REG_L | REG_IY, CPU_ZILOG },
    { "iyh",  OP_REG8|OP_INDEX, REG_H | REG_IY, CPU_ZILOG },

    { "ix",   OP_HL|OP_ARITH16|OP_PUSHABLE|OP_INDEX, REG_HL | REG_IX, CPU_NOT8080 },
    { "iy",   OP_HL|OP_ARITH16|OP_PUSHABLE|OP_INDEX, REG_HL | REG_IY, CPU_NOT8080 },
    { "ip",   OP_IP, REG_IP, CPU_RABBIT }, /* RCM */
    { "a'",   OP_A|OP_ALT,  REG_A|REG_ALT, CPU_RABBIT },
    { "b'",   OP_REG8|OP_ALT,  REG_B|REG_ALT, CPU_RABBIT },
    { "c'",   OP_REG8|OP_ALT,  REG_C|REG_ALT, CPU_RABBIT },
    { "d'",   OP_REG8|OP_ALT,  REG_D|REG_ALT, CPU_RABBIT },
    { "e'",   OP_REG8|OP_ALT,  REG_E|REG_ALT, CPU_RABBIT },
    { "h'",   OP_REG8|OP_ALT,  REG_H|REG_ALT, CPU_RABBIT },
    { "l'",   OP_REG8|OP_ALT,  REG_L|REG_ALT, CPU_RABBIT },
    { "psw",  OP_RP_PUSHABLE, REG_A, CPU_80OS },
    { "a",    OP_A,  REG_A, CPU_ALL },
    { "b",    OP_REG8|OP_B|OP_RP|OP_RP_PUSHABLE,  REG_B, CPU_ALL|CPU_80OS },
    { "c",    OP_REG8,  REG_C, CPU_ALL },
    { "d",    OP_REG8|OP_D|OP_RP|OP_RP_PUSHABLE,  REG_D, CPU_ALL|CPU_80OS },
    { "e",    OP_REG8,  REG_E, CPU_ALL },
    { "h",    OP_REG8|OP_RP|OP_RP_PUSHABLE,  REG_H, CPU_ALL },
    { "l",    OP_REG8,  REG_L, CPU_ALL },
    { "m",    OP_REG8,  REG_M, CPU_8080 },
    { "r",    OP_R, REG_R, CPU_ZILOG },
    { "eir",  OP_R, REG_R, CPU_RABBIT },  /* RCM alias */
    { "i",    OP_I, REG_I, CPU_ZILOG },
    { "iir",  OP_I, REG_I, CPU_RABBIT },  /* RCM alias */
    { "f",    OP_F, REG_F, CPU_Z80 },
    { "xpc",  OP_XPC, REG_XPC, CPU_RABBIT }, /* RCM */

    /* RCM4000 extra registers + pw, px, py, pz */
    { "pw'",   OP_IDX32|OP_ALT, REG_PW|REG_ALT, CPU_RCM4000 }, 
    { "px'",   OP_IDX32|OP_ALT, REG_PX|REG_ALT, CPU_RCM4000 }, 
    { "py'",   OP_IDX32|OP_ALT, REG_PY|REG_ALT, CPU_RCM4000 }, 
    { "pz'",   OP_IDX32|OP_ALT, REG_PZ|REG_ALT, CPU_RCM4000 }, 
    { "pw",   OP_IDX32, REG_PW, CPU_RCM4000 }, 
    { "px",   OP_IDX32, REG_PX, CPU_RCM4000 }, 
    { "py",   OP_IDX32, REG_PY, CPU_RCM4000 }, 
    { "pz",   OP_IDX32, REG_PZ, CPU_RCM4000 }, 
    { "su",   OP_SU, REG_SU, CPU_RCM4000 },
    { "htr",  OP_HTR, REG_HTR, CPU_RCM4000 },  
    { "jkhl'", OP_JKHL|OP_ALT, REG_JKHL|REG_ALT, CPU_RCM4000 },
    { "jkhl", OP_JKHL, REG_JKHL, CPU_RCM4000 },
    { "bcde'", OP_BCDE|OP_ALT, REG_BCDE|REG_ALT, CPU_RCM4000 },
    { "bcde", OP_BCDE, REG_BCDE, CPU_RCM4000 },
    { "jk'",   OP_JK|OP_ALT,   REG_JK|REG_ALT,   CPU_RCM4000 },
    { "jk",   OP_JK,   REG_JK,   CPU_RCM4000 },
};

struct {
    char          *name;
    int            op;
    int            flags;
    int            cpu;
} flags[] = {
    { "z",   OP_FLAGS, FLAGS_Z,  CPU_ALL },
    { "nz",  OP_FLAGS, FLAGS_NZ, CPU_ALL },
    { "c",   OP_FLAGS, FLAGS_C,  CPU_ALL },
    { "nc",  OP_FLAGS, FLAGS_NC, CPU_ALL },
    { "pe",  OP_FLAGS, FLAGS_PE, CPU_ALL },  /* Not for jr, emu for RCM */
    { "po",  OP_FLAGS, FLAGS_PO, CPU_ALL },  /* Not for jr, emu for RCM */
    { "p",   OP_FLAGS, FLAGS_P,  CPU_ALL },   /* Not for jr, emu for RCM */
    { "m",   OP_FLAGS, FLAGS_M,  CPU_ALL },   /* Not for jr, emu for RCM */
    { "lz",  OP_FLAGS, FLAGS_PO, CPU_RABBIT },  /* RCM alias */
    { "lo",  OP_FLAGS, FLAGS_PE, CPU_RABBIT },  /* RCM alias */
    { "gtu", OP_FLAGS_RCM, FLAGS_GTU, CPU_RCM4000 }, /* RCM4000 */
    { "gt",  OP_FLAGS_RCM, FLAGS_GT,  CPU_RCM4000 },  /* RCM4000 */
    { "lt",  OP_FLAGS_RCM, FLAGS_LT,  CPU_RCM4000 },  /* RCM4000 */
    { "v",   OP_FLAGS_RCM, FLAGS_V,   CPU_RCM4000 },   /* RCM4000 */
};




int ext_unary_eval(int type,taddr val,taddr *result,int cnst)
{
    switch (type) {
    case LOBYTE: 
        *result = cnst ? (val & 0xff) : val;
        return 1;
    case HIBYTE:
        *result = cnst ? ((val >> 8) & 0xff) : val;
        return 1;
    default:   
        break;   
    }
    return 0;  /* unknown type */
}

int ext_find_base(symbol **base,expr *p,section *sec,taddr pc)
{
    if ( p->type==DIV || p->type ==MOD) {
        if (p->right->type == NUM && p->right->c.val == 256 ) {
           p->type = p->type == DIV ? HIBYTE : LOBYTE;
        }
    } else if ( p->type == BAND && p->right->type == NUM && p->right->c.val == 255 ) {
        p->type = LOBYTE;
    }
    if (p->type==LOBYTE || p->type==HIBYTE) {
        modifier = p->type;
        return find_base(p->left,base,sec,pc);
    }
    return BASE_ILLEGAL;
}
                                                

static int get_flags_info(char **text, int len, int *flags_ptr, int *op)
{
    int     i;

    for ( i = 0; i < sizeof(flags) / sizeof(flags[0]); i++ ) {
        if ( len != strlen(flags[i].name) ) {
            continue;
        }
        if ( strnicmp(*text, flags[i].name,len) == 0 ) {            
#if 0
            if ( (cpu_type & flags[i].cpu) == 0 ) {
                continue;
            }
#endif
            *text += len;
            *op= flags[i].op;
            *flags_ptr = flags[i].flags;
            return 0;
        }
    }
    return -1;
}



static int get_register_info(char **text, int len, int *reg, int *op)
{
    int     i;

    for ( i = 0; i < sizeof(registers) / sizeof(registers[0]); i++ ) {
        if ( len != strlen(registers[i].name) ) {
            continue;
        }
        if ( strnicmp(*text, registers[i].name,len) == 0 ) {
#if 0
            if ( (cpu_type & registers[i].cpu) == 0 ) {
                continue;
            }
#endif
            *text += len;
            *reg = registers[i].reg;
            *op = registers[i].op;
            return 0;
        }
    }
    return -1;
}


#define BASIC_TYPE(x) ((x) & OP_MASK)
#define CHECK_INDEXVALID(ot, r) ( ( (ot) & OP_INDEX ) == OP_INDEX || ( ( ((r) & (REG_IX|REG_IY)) == 0 ) ) )
#define ONLY_MODIFIER(x, nmod) ((((x) & ~OP_MASK) | (nmod)) == (nmod))
#define PERMITTED(c, ot, b ) ( ( (ot) & (b) ) || ( ((ot) & (b)) | ( (c) & (b) ) ) == 0 )
#define WANT(c, ot, b ) ( ( (ot) & (b) ) && ( ((c) & (b)) ) ||  ( (ot) & (b) ) == 0 && ( ((c) & (b)) ) == 0 )

/* Check whether an alt override is permitted - this is for allowing the first register 
 * operand of rabbit instructions to specify that they want to use altd 
 *
 * NB: OP_ALT and OP_RALT are mutually exclusive 
 */
static int alt_override_permitted(int want, int got, int gotreg)
{
    if ( ( cpu_type & CPU_RABBIT ) ) {
        if ( ( want & OP_RALT|OP_ALT ) == 0 && ( got & OP_ALT ) ) {
            /* Not allowed an override or not accepting an alt by default */
            return 0;
        }
        if ( ( want & OP_RALT ) && ( got & OP_ALT) ) {
            if ( (gotreg & REG_PLAIN) == REG_HLREF && ( want & OP_RALTHL ) == 0 ) {
                /* We're not allowed (hl') apart from for bit */
                return 0;
            }
            /* We wanted an override */
            return 1;
        }
    }

    if ( ( want & OP_ALT ) && ( got & OP_ALT) == 0 ) {
        /* We wanted an alt but didn't get one */
        return 0;
    }

    if ( ( got & OP_ALT ) && ( want & OP_ALT ) == 0 ) {
        /* Not expected an alt by default */
        return 0;
    }
    return 1; /* It is permitted */
}

int parse_operand(char *p, int len, operand *op, int optype)
{
    char   *start = p;
    int     opt = 0;
    int     regopt = 0;
    int     second_reg = -1;
    int     ignore;

    op->value = NULL;
    op->reg = 0;
    op->type = 0;
    op->flags = 0;
    op->bit = 0;

    p = skip(p);
    /* Here I disable the possiblity to use parentheses around constants 
     * when addressing is not possible. This old behavior created a lot of
     * inexisting instructions and bugs.
     */
    if ( *p == '(' && optype != OP_DATA && check_indir(p,start+len) ) {
        int   llen;
        char *end;
        p++;

        p = skip(p);

        /* Search for the first nonmatching character */
        if ( (end = strpbrk(p, ") +-") ) != NULL ) {
            llen = end - p;
        } else {
            llen = len - (p - start);
        }

        /* Search for register */
        if ( get_register_info(&p, llen, &op->reg, &opt) == 0 ) {
            char  save;
            char *save_ptr;
            int   get_expr = 1;
            opt |= OP_INDIR;
            p = skip(p);
            if ( *p != ')' ) {
                if ( *p == '+' ) {
                    p++;
                    /* RCM4000 - check for a register here */
                    if ( (end = strpbrk(p+1, ") \t+-") ) != NULL ) {
                        llen = end - p;
                    } else {
                        llen = len - (p - start);
                    }
                    if ( get_register_info(&p, llen, &second_reg, &ignore) == 0 ) {
                        get_expr = 0;
                    } else {
                        p--;
                    }
                }

                if ( get_expr ) {
                    /* We may well have an expression, check for it */
                    save_ptr = p-1;
                    save = *save_ptr;
                    *save_ptr = '(';  /* Create a bracket */
                    if ( ( op->value = parse_expr(&p) ) != NULL ) {
                        opt |= OP_OFFSET;
                    } else {
                        *save_ptr = save;
                        return PO_NOMATCH;
                    }
                    *save_ptr = save;
                } 
            }


        } else if ( ( op->value = parse_expr(&p) ) != NULL ) {
            opt = OP_ABS16|OP_INDIR;
        } else {
            return PO_NOMATCH;
        }
    } else if ( (optype == OP_FLAGS || optype == OP_FLAGS_RCM) && 
                get_flags_info(&p, len, &op->flags, &opt ) == 0 ) {
        /* It's a flag (flag c matches register c */
    } else if ( get_register_info(&p, len, &op->reg, &opt) == 0 ) {
        /* It's a register */
    } else {
        int   llen;
        char *end;

        /* We might have a reg+reg setup */
        /* Search for the first nonmatching character */
        if ( (end = strpbrk(p, " \t+-") ) != NULL ) {
            llen = end - p;
        } else {
            llen = len - (p - start);
        }
        if ( get_register_info(&p, llen, &op->reg, &opt) == 0 ) {
            p = skip(p);
            if ( *p == '+' ) {
                p++;
                /* RCM4000 - check for a register here */
                if ( (end = strpbrk(p+1, ") \t+-") ) != NULL ) {
                    llen = end - p;
                } else {
                    llen = len - (p - start);
                }
                if ( get_register_info(&p, llen, &second_reg, &ignore)  ) {
                    if ( ( op->value = parse_expr(&p) ) != NULL ) {
                        opt |= OP_OFFSET;
                    } else {
                        return PO_NOMATCH;
                    }
                } 
            }
        } else {
            if ( z80asm_compat && *p == '#' ) {
                p++;  /* Constant identifier - compatibility with z80asm */
            }
            if ( ( op->value = parse_expr(&p) ) != NULL ) {
                opt = OP_ABS16;
            } else {
                /* It's rubbish and we don't understand it at all */
                return PO_NOMATCH;
            }
        }
    }

    opt |= regopt; /* Setup what we've parsed */


    if ( optype & OP_ARITH16 ) {

        if ( (opt & OP_ARITH16) == 0 || (opt & OP_INDIR) != 0 ||
             !PERMITTED(opt, optype, OP_INDEX) || !alt_override_permitted(optype,opt,op->reg) ) {
            goto nomatch;
        }
    } else if ( optype & OP_PUSHABLE ) {
        if ( (opt & OP_PUSHABLE) == 0 ||
             !PERMITTED(opt, optype, OP_INDEX) || !alt_override_permitted(optype,opt,op->reg) ) {
            goto nomatch;
        }
    } else if ( optype & OP_STD16BIT ) {
        if ( (opt & OP_STD16BIT) == 0 ||
             !PERMITTED(opt, optype, OP_INDEX) || !alt_override_permitted(optype,opt,op->reg) ) {
            goto nomatch;
        }
    } else if ( optype & OP_RP ) {
        if ( (opt & OP_RP) == 0)
            goto nomatch;
    } else if ( optype & OP_RP_PUSHABLE ) {
        if ( (opt & OP_RP_PUSHABLE) == 0)
            goto nomatch;
    } else  if (optype & OP_B) {
        if ( (opt & OP_B) == 0)
            goto nomatch;
    } else  if (optype & OP_D) {
        if ( (opt & OP_D) == 0)
            goto nomatch;
    } else {
        switch ( BASIC_TYPE(optype) ) {   
        case OP_A:
        case OP_B:
        case OP_D:
        case OP_AF:
        case OP_BC:
        case OP_DE:
        case OP_HL:
        case OP_SP:
        case OP_JKHL:
        case OP_BCDE:
        case OP_JK:
        case OP_I:
        case OP_R:
        case OP_F:
        case OP_IP:
        case OP_XPC:
        case OP_IDX32:
        case OP_SU:
        case OP_HTR:
            /* Basic type and indirection and index and alt register designation must match */
            if ( BASIC_TYPE(opt) != BASIC_TYPE(optype) || !PERMITTED(opt, optype, (OP_INDEX)) || 
                 !WANT(opt,optype,OP_INDIR) || !alt_override_permitted(optype,opt,op->reg) ) {
                goto nomatch;
            }
            if ( ( optype & OP_OFFSA ) ) {
                if ( second_reg != REG_A ) {
                    goto nomatch;
                }
            } else if ( ( optype & OP_OFFSHL ) ) {
                if ( second_reg != REG_HL ) {
                    goto nomatch;
                }
            } else if ( ( optype & OP_OFFSBC ) ) {
                if ( second_reg != REG_BC ) {
                    goto nomatch;
                }
            } else if ( ( optype & OP_OFFSDE ) ) {
                if ( second_reg != REG_DE ) {
                    goto nomatch;
                }
            } else if ( ( optype & OP_OFFSIX ) ) {
                if ( second_reg != (REG_HL|REG_IX) ) {
                    goto nomatch;
                }
            } else if ( ( optype & OP_OFFSIY ) ) {
                if ( second_reg != (REG_HL|REG_IY) ) {
                    goto nomatch;
                }
            } else if ( (optype & OP_OFFSET)  ) {
                if ( op->value == NULL ) {
                    char *expr_s = "0";
                    op->value = parse_expr(&expr_s);
                    opt |= OP_OFFSET;
                }
            } else if ( (( optype & OP_OFFSET ) == 0 && op->value) || second_reg != -1 ) {
                goto nomatch;
            }
            break;
        case OP_REG8:
            /* Normalise (hl) or (ix+d) to the correct register type */
            if ( ( opt & OP_INDIR ) && (op->reg & REG_PLAIN) == REG_HL ) {
                opt = ( opt & (OP_OFFSET|OP_INDEX|OP_ALT) ) | OP_REG8;
                op->reg = ( op->reg & (REG_IX|REG_IY|REG_ALT) ) | REG_HLREF;
                if ( op->reg & ( REG_IX|REG_IY) ) {
                    op->reg |= REG_INDEX;
                    /* If it's an index, set an expression on it if there's not one already*/
                    if ( op->value == NULL ) {
                        char *expr_s = "0";
                        op->value = parse_expr(&expr_s);
                        opt |= OP_OFFSET;
                    }
                } else if ( op->value ) {
                    goto nomatch;
                }
            } else if ( op->value ) {
                goto nomatch;
            }
            /* If it's OP_A then we're ok */
            if ( opt == OP_A ) {
                opt = OP_REG8;
            }
            /* Final check - check indices */
            if ( !CHECK_INDEXVALID(optype, opt) || BASIC_TYPE(opt) != BASIC_TYPE(optype) || !alt_override_permitted(optype,opt,op->reg) || second_reg != -1  ) {
                goto nomatch;
            }
            break;
        case OP_ADDR8:
            if ( opt != (OP_ABS16|OP_INDIR) || op->value == NULL ) {
                goto nomatch;
            }
            opt = OP_ABS;
            break;
        case OP_ADDR:
            if ( opt != (OP_ABS16|OP_INDIR) || op->value == NULL ) {
                goto nomatch;
            }
            break;
        case OP_ADDR24:
            if ( opt != (OP_ABS16|OP_INDIR) || op->value == NULL ) {
                goto nomatch;
            }
            opt = OP_ABS24;
            break;
        case OP_ADDR32:
            if ( opt != (OP_ABS32|OP_INDIR) || op->value == NULL ) {
                goto nomatch;
            }
            opt = OP_ABS32;
            break;
        case OP_NUMBER:
        case OP_ABS:
        case OP_ABS16:
        case OP_ABS24:
        case OP_ABS32:
            if ( !PERMITTED(opt, optype, OP_INDIR) || op->value == NULL ) {
                goto nomatch;
            }
        case OP_DATA:
            opt = optype;
            break;
        case OP_PORT:
            if ( opt != (OP_INDIR|OP_REG8) || op->reg != REG_C || second_reg != -1 ) {
                goto nomatch;
            }
            break;
        case OP_FLAGS:
        case OP_FLAGS_RCM:
            if ( opt != optype ) {
                goto nomatch;
            }
            break;
        case OP_IX:
            if ( BASIC_TYPE(opt) != OP_HL ||  !WANT(opt, optype, OP_INDIR) || op->reg != (REG_HL|REG_IX) || second_reg != -1 ) {
                goto nomatch;
            }
            opt = optype;
            if ( (optype & OP_OFFSET) ) {
                op->reg = REG_HLREF|REG_IX;
                if ( op->value == NULL ) {
                    char *expr_s = "0";
                    op->value = parse_expr(&expr_s);
                    opt |= OP_OFFSET;
                }
            }
            break;
        case OP_IY:
            if ( BASIC_TYPE(opt) != OP_HL ||  !WANT(opt, optype, OP_INDIR) || op->reg != (REG_HL|REG_IY) || second_reg != -1 ) {
                goto nomatch;
            }
            opt = optype;
            if ( (optype & OP_OFFSET) ) {
                op->reg = REG_HLREF|REG_IY;
                if ( op->value == NULL ) {
                    char *expr_s = "0";
                    op->value = parse_expr(&expr_s);
                    opt |= OP_OFFSET;
                }
            }
            break;
        case OP_NONE:
            if ( opt != 0 ) {
                goto nomatch;
            }
            break;
        default:
            cpu_error(20, optype, opt);
            goto nomatch;
        }
    }

    op->type = opt;

    return PO_MATCH;
nomatch:
    if ( op->value ) {
        free_expr(op->value);
    }
    return PO_NOMATCH;

}



void init_instruction_ext(instruction_ext *ext)
{
    ext->altd = altd_enabled;
    ext->ioi = ioi_enabled;
    ext->ioe = ioe_enabled;
}

static int parse_rcm_identifier(char **sptr)
{
    char *s = *sptr;
    char *name = s;

    while (ISIDCHAR(*s))
        s++;
    if ( s-name == 4 && strnicmp(name, "altd", 4)  == 0 ) {
        altd_enabled = 1;
    } else if ( s-name == 3 && strnicmp(name,"ioi",3) == 0 ) {
        ioi_enabled = 1;
    } else if ( s-name == 3 && strnicmp(name,"ioe",3) == 0 ) {
        ioe_enabled = 1;
    } else {
        return -1;
    }
    *sptr = s;
    return 0;
}



/*#include "syntax_extra.c"*/

/* TODO: z80asm compatibility
   defgroup
   defvars
   call_oz : macros?
   pkg_call:
   define
   undefine

   perhaps defw etc need to come into here?
*/
char *parse_z80asm_pseudo(char *s)
{
    char *name = s;
    char *asmpc;

    if ( z80asm_compat == 0 ) {
        return s;
    }

    /* z80asm uses ASMPC instead of $ for the program counter, so fix it */
    asmpc = strstr(s,"ASMPC");
    if ( asmpc != NULL ) {
        *asmpc++ = '$';
        *asmpc++ = ' ';
        *asmpc++ = ' ';
        *asmpc++ = ' ';
        *asmpc = ' ';
    }

    while (ISIDCHAR(*s))
        s++;
    if ( s - name == 6 && strnicmp(name,"module", 6) == 0 ) {
        s = skip(s);
        parse_name(&s);  /* We throw away the result */
        eol(s);
    } else {
        /* defb, defw, defl, defm, xdef, xref, lib, xlib, defc, defp dealt with by old syntax module */
        s = name;
    }

    return s;
}

char *parse_cpu_special(char *start)
{
    char *s = start;
    int   i;


    altd_enabled = ioi_enabled = ioe_enabled = 0;

    if (ISIDSTART(*s)) {
        /* Check for standard identifiers */
        if ( parse_rcm_identifier(&s) == -1 ) {
            /* Not a rabbit one, lets check z80asm versions */
            start = parse_z80asm_pseudo(s);
        } else {
            /* Check for upto 2 rabbit identifiers */
            for ( i = 0; i < 2; i++ ) {
                s = skip(s);
                if ( parse_rcm_identifier(&s) == -1 ) {
                    return s;
                }
                start = s;
            }
        }
    }
    return start;
}

size_t instruction_size(instruction *ip, section *sec, taddr pc)
{
    mnemonic *opcode = &mnemonics[ip->code];
    size_t    size;

    /* Try and find the right opcode as necessary */
    if ( (opcode->ext.cpus & cpu_type)  ) {
        int action = -1;

        if ( cpu_type & CPU_RCM2000 ) {
            action = opcode->ext.rabbit2000_action;
        } else if ( cpu_type & CPU_RCM4000 ) {
            action = opcode->ext.rabbit4000_action;
        } else if ( cpu_type & CPU_RCM3000 ) {
            action = opcode->ext.rabbit3000_action;
        } else if ( cpu_type & CPU_GB80 ) {
            action = opcode->ext.gb80_action;
        }
        switch ( action ) {
        case RCM_EMU_INCREMENT:  /* Move to next instruction in table that matches our cpu */
            /* We only want to do the increment if we're not dealing with index registers */
            if ( ( ip->op[0] == NULL || (ip->op[0] && (ip->op[0]->type & OP_INDEX) == 0) &&
                  (ip->op[1] == NULL || (ip->op[1] && (ip->op[1]->type & OP_INDEX) == 0) )) &&
                 !(opcode->ext.mode == TYPE_LD8 && (ip->op[1]->reg == REG_A || ip->op[1]->reg == REG_HLREF || ip->op[0]->reg == REG_A || ip->op[0]->reg == REG_HLREF) ) ) {
                do {
                    ip->code++;
                    opcode = &mnemonics[ip->code];
                } while ( (opcode->ext.cpus &  cpu_type) == 0 );
            }
            break;
        case RCM_EMU_LIBRARY:
            return 3;  /* Call (function) */
            break;
        }
    }

    /* Call cc need to be emulated on the rabbit and have a different size */
    if ( ( cpu_type & CPU_RABBIT) && opcode->ext.mode == TYPE_FLAGS && opcode->ext.opcode == 0xc4 ) {
        if ( ip->op[0]->flags <= FLAGS_C ) {
            size = 5;
        } else {
            size = 6;
        }
        return size;
    }



    /* Get the basic size of the opcode */
    if ( opcode->ext.opcode & 0xff000000 ) {
        size = 4;
    } else if ( opcode->ext.opcode & 0x00ff0000 ) {
        size = 3;
    } else if ( opcode->ext.opcode & 0x0000ff00 ) {
        size = 2;
    } else {
        size = 1;
    }

    /* Increase the size for any modifiers */
    if ( (( ip->op[0] && ip->op[0]->type & (OP_INDEX ) )  ||
          ( ip->op[1] && ip->op[1]->type & (OP_INDEX ) )) && opcode->ext.mode != TYPE_EDPREF ) {
        size++;
    }

    if ( ( ip->op[0] && ip->op[0]->type == OP_ABS ) ||
         ( ip->op[1] && ip->op[1]->type == OP_ABS  ) ) {
        size++;
    }
    if ( ( ip->op[0] && (ip->op[0]->type & OP_OFFSET )) ||
         ( ip->op[1] && (ip->op[1]->type & OP_OFFSET)  ) ) {
        size++;
    }

    if ( ( ip->op[0] && (ip->op[0]->type == (OP_ABS16|OP_INDIR) || ip->op[0]->type == OP_ABS16 )) ||
         ( ip->op[1] && (ip->op[1]->type == (OP_ABS16|OP_INDIR) || ip->op[1]->type == OP_ABS16 )) ) {
        size += 2;
    }
    if ( ( ip->op[0] && (ip->op[0]->type == (OP_ABS24|OP_INDIR) || ip->op[0]->type == OP_ABS24 )) ||
         ( ip->op[1] && (ip->op[1]->type == (OP_ABS24|OP_INDIR) || ip->op[1]->type == OP_ABS24 )) ) {
        size += 3;
    }
    if ( ( ip->op[0] && (ip->op[0]->type == (OP_ABS32|OP_INDIR) || ip->op[0]->type == OP_ABS32 )) ||
         ( ip->op[1] && (ip->op[1]->type == (OP_ABS32|OP_INDIR) || ip->op[1]->type == OP_ABS32 )) ) {
        size += 4;
    }


    /* Add on an altd flag if implicitly required by alternate registers */
    if ( ((opcode->operand_type[0] & OP_RALT) && ( ip->op[0]->type & OP_ALT) ) ||
         ( (opcode->operand_type[1] & OP_RALT) && ( ip->op[1]->type & OP_ALT) ) )  {
        ip->ext.altd = 1;
    }


    if  (ip->ext.altd ) {
        size++;
    }
    if ( ip->ext.ioi ) {
        size++;
    }
    if ( ip->ext.ioe ) {
        size++;
    }

    if (((cpu_type & CPU_80OS) == CPU_80OS) && 
            (opcode->ext.modifier_flags & F_80OS_CHECK) == F_80OS_CHECK && opcode->ext.opcode == 0xfe) {
        size = 3;
        ip->op[0]->type = OP_ABS16;
    }
    return size;
}



static taddr apply_modifier(rlist *rl, taddr val)
{
    switch (modifier) {
    case LOBYTE:
        if (rl)
            ((nreloc *)rl->reloc)->mask = 0xff;
        val = val & 0xff;
        break;
    case HIBYTE:
        if (rl)
            ((nreloc *)rl->reloc)->mask = 0xff00;
        val = (val >> 8) & 0xff;
        break;
    }
    return val;
}



static void write_opcode(mnemonic *opcode, dblock *db, int size, section *sec, taddr pc, operand *op1, operand *op2, int add, instruction_ext *ext)
{
    int   reg1 = 0 , reg2 = 0;
    operand *indexit = NULL;
    int   indexcode = 0;
    expr *expr = NULL;
    int   exprsize;
    unsigned char *d = (unsigned char *)db->data;
    unsigned char *start = (unsigned char *)db->data;

    if  (ext->altd ) {
        *d++ = 0x76;
        size--;
    }
    if ( ext->ioi ) {
        *d++ = 0xd3;
        size--;
    }
    if ( ext->ioe ) {
        *d++ = 0xdb;
        size--;
    }

    if ( op1 ) {
        reg1 = op1->reg;
        if ( op1->type & OP_OFFSET ) {
            indexit = op1;
        }
        if ( reg1 & REG_IX ) {
            indexcode = swapixiy == 0 ? 0xdd : 0xfd;
        } else if ( reg1 & REG_IY ) {
            indexcode = swapixiy == 0 ? 0xfd : 0xdd;
        }
        if ( BASIC_TYPE(op1->type) == OP_ABS ) {
            expr = op1->value;
            exprsize = 1;
            --size;
        }
        if ( BASIC_TYPE(op1->type) == OP_ABS16 ) {
            expr = op1->value;
            exprsize = 2;
            size -= 2;
        }
        if ( BASIC_TYPE(op1->type) == OP_ABS24 ) {
            expr = op1->value;
            exprsize = 3;
            size -= 3;
        }
        if ( BASIC_TYPE(op1->type) == OP_ABS32 ) {
            expr = op1->value;
            exprsize = 4;
            size -= 4;
        }
    }
    if ( op2 ) {
        reg2 = op2->reg;

        if ( op2->type & OP_OFFSET ) {
            indexit = op2;
        }
        if ( reg2 & REG_IX ) {
            indexcode = 0xdd;
        } else if ( reg2 & REG_IY ) {
            indexcode = 0xfd;
        }
        if ( BASIC_TYPE(op2->type) == OP_ABS ) {
            expr = op2->value;
            exprsize = 1;
            --size;
        }
        if ( BASIC_TYPE(op2->type) == OP_ABS16 ) {
            expr = op2->value;
            exprsize = 2;
            size -= 2;
        }
        if ( BASIC_TYPE(op2->type) == OP_ABS24 ) {
            expr = op2->value;
            exprsize = 3;
            size -= 3;
        }
        if ( BASIC_TYPE(op2->type) == OP_ABS32 ) {
            expr = op2->value;
            exprsize = 4;
            size -= 4;
        }
    }

    if ( opcode->ext.mode == TYPE_EDPREF ) {
        if ( indexcode == 0 ) {
            indexcode = 0xed;
        }
    } else if ( opcode->ext.mode == TYPE_NOPREFIX || opcode->ext.mode == TYPE_IDX32 ) {
        indexcode = 0;
    }

    if ( indexcode ) {
        *d++ = indexcode;
        --size;
    }

    if ( indexit ) {
        size--;
    }

    switch ( size ) {
    case 4:
        *d++ = ( opcode->ext.opcode & 0xff000000 ) >> 24;
        /* Fall through */
    case 3:
        *d++ = ( opcode->ext.opcode & 0x00ff0000 ) >> 16;
        /* Fall through */
    case 2:
        *d++ = ( opcode->ext.opcode & 0x0000ff00 ) >> 8;
        /* Fall through */
    case 1:
        *d++ = ( ( opcode->ext.opcode & 0x000000ff ) >> 0 ) + add ;
    }

    /* Now we can write out the index */
    if ( indexit ) {
        taddr val = 0;
        int cbmode = 0;
        /* If it's a CB code then the index isn't last */
        if ( (( opcode->ext.opcode & 0xffffff00 ) >> 8) == 0x000000cb )
            cbmode = 1;
        if ( indexit->value && eval_expr(indexit->value, &val, sec, pc) == 0 ) {
            symbol *base;
            rlist *rl;
            modifier = 0;
            if ( find_base(indexit->value, &base, sec, pc) == BASE_OK ) {
                rl = add_extnreloc(&db->relocs, base, val, REL_ABS, 0, 8,
                                   cbmode ? ((d-1)-start) : (d-start));
                val = apply_modifier(rl, val);
            } else
                general_error(38);  /* illegal relocation */
        }
        if ( ( (indexit->reg == REG_SP || ( indexit->reg & REG_PLAIN) == REG_HL) && val >= 0 && val <= 255 )
             || ( val >= -128 && val <= 127 ) )  {
            if ( cbmode ) {
                *d = *(d - 1);
                *(d -1 ) = val;
            } else {
                *d++ = val;
            }
        } else {
            cpu_error(0, val);
        }
    }

    if ( expr != NULL ) {
        taddr val;
        if ( eval_expr(expr, &val, sec, pc) == 0 ) {
            symbol *base;
            rlist *rl;
            modifier = 0;
            if ( find_base(expr, &base, sec, pc) == BASE_OK ) {
                if ( opcode->ext.mode == TYPE_RELJUMP ) {
                    add_extnreloc(&db->relocs, base, val -1, REL_PC,
                                  0, exprsize * 8, (d - start));
                    val -= (pc + db->size);
                    if (modifier)
                        ierror(0);  /* @@@ Hi/Lo modifier makes no sense here? */
                } else {
                    rl = add_extnreloc(&db->relocs, base, val, REL_ABS,
                                       0, exprsize * 8, (d - start));
                    val = apply_modifier(rl, val);
                }
                
            } else {
                general_error(38);  /* illegal relocation */
            }
        } else {
            /* It was a constant, if it's relative calculate now */
            if ( opcode->ext.mode == TYPE_RELJUMP ) {
                val -= ( pc + db->size );
            }
        }
        if ( exprsize == 1 && ( val < -128 || val >= 256 ||
             (opcode->ext.mode == TYPE_RELJUMP && val >= 128 ) ) )
            cpu_error(3, val);
        else
            d = setval(0,d,exprsize,val);
    }
}



static void rabbit_emu_call(instruction *ip,dblock *db,section *sec,taddr pc)
{
    unsigned char *d;
    unsigned char *start;
    taddr          val;


    myfree(db->data);
    db->data = mymalloc(6);  /* Maximum size */
    start = d = (unsigned char *)db->data;

    if (  ip->op[0]->flags < FLAGS_PO ) {
        switch ( ip->op[0]->flags ) {
        case FLAGS_NZ:
            *d++ = 0x28; /* jr z */
            *d++ = 0x03;
            break;
        case FLAGS_Z:
            *d++ = 0x20; /* jr z */
            *d++ = 0x03;
            break;
        case FLAGS_NC:
            *d++ = 0x38;  /* jr c */
            *d++ = 0x03;
            break;
        case FLAGS_C:
            *d++ = 0x30;  /* jr nc */
            *d++ = 0x03;
            break;
        }
    } else {
        expr *expr;
        char *t_expr = "$ + 6";
        /* These ones need to be jp to get a temporary expression */
        switch ( ip->op[0]->flags ) {
        case FLAGS_PO:
            *d++ = 0xea; /* jp pe */
            break;
        case FLAGS_PE:
            *d++ = 0xe2; /* jp po */
            break;
        case FLAGS_M:
            *d++ = 0xf2; /* jp p */
            break;
        case FLAGS_P:
            *d++ = 0xfa; /* jp m */
            break;
        }
        /* Create a temporary expression */
        expr = parse_expr(&t_expr);
        if ( eval_expr(expr, &val, sec, pc) == 0 ) {
            symbol *base;
            if ( find_base(expr, &base, sec, pc) == BASE_OK )
                add_extnreloc(&db->relocs,base, val, REL_ABS,
                              0, 8, (d - start));
            else
                general_error(38);  /* illegal relocation */
        } 
        free_expr(expr);
        *d++ = val % 256;
        *d++ = val / 256;
    }
    *d++ = 0xcd;  /* call */
    /* Evaluate the real expression */
    if ( eval_expr(ip->op[1]->value, &val, sec, pc) == 0 ) {
        symbol *base;
        if ( find_base(ip->op[1]->value, &base, sec, pc) == BASE_OK )
            add_extnreloc(&db->relocs,base, val, REL_ABS, 0, 8, (d - start));
        else
            general_error(38);  /* illegal relocation */
    } 
    *d++ = val % 256;
    *d++ = val / 256;

    /* Set the size */
    db->size= d - start;
    return;
}

/* Check whether an operand is an offset operator and thus affects memory */
static int is_offset_operand(operand *op)
{
    if ( op ) {
        if ( BASIC_TYPE(op->type) == OP_REG8 && ( (op->reg & REG_PLAIN) == REG_HLREF) ) {
            return 1;
        }
    }
    return 0;
}

dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
{
    dblock *db = new_dblock();
    taddr val;
 
    if (bitsize!=8 && bitsize!=16 && bitsize != 24 && bitsize != 32)
        cpu_error(2,bitsize);  /* data size not supported */
 
    db->size = bitsize >> 3;
    db->data = mymalloc(db->size);
    if (!eval_expr(op->value,&val,sec,pc)) {
        symbol *base;
        int btype;
        rlist *rl;

        modifier = 0;
        btype = find_base(op->value, &base, sec, pc);
        if ( btype == BASE_OK || ( btype == BASE_PCREL && modifier == 0 ) ) {
            rl = add_extnreloc(&db->relocs, base, val,
                               btype==BASE_PCREL ? REL_PC : REL_ABS,
                               0, bitsize, 0);
            val = apply_modifier(rl, val);
        }
        else if (btype != BASE_NONE)
            general_error(38);  /* illegal relocation */
    }
    if (bitsize < 16 && (val<-0x80 || val>0xff))
        cpu_error(3, val);  /* operand doesn't fit into 8-bits */
    setval(0,db->data,db->size,val);
 
    return db;
}

dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
{
    dblock *db;
    mnemonic *opcode = &mnemonics[ip->code];
    symbol *base;
    unsigned char *d;
    taddr val = 0;
    int size = 0, offs = 0; 
    int error = 0;


    size = instruction_size(ip, sec, pc);

    if ( (opcode->ext.cpus & cpu_type) == 0 ) {
        cpu_error(1, cpuname, opcode->name);
        error = 1;
    } else {
        int action = 0;


        if ( cpu_type & (CPU_RABBIT|CPU_GB80) ) {
            if ( cpu_type & CPU_RCM2000 ) {
                action = opcode->ext.rabbit2000_action;
            } else if ( cpu_type & CPU_RCM4000 ) {
                action = opcode->ext.rabbit4000_action;
            } else if ( cpu_type & CPU_RCM3000 ) {
                action = opcode->ext.rabbit3000_action;
            } else if ( cpu_type & CPU_GB80 ) {
                action = opcode->ext.gb80_action;
            }
            switch ( action ) { /* INCREMENT dealt with by instruction_size */
            case RCM_EMU_LIBRARY:
            {
                char    buf[128];
                char   *bufptr = buf;
                symbol *sym;
                expr   *expr;
                taddr   val;

                /* Emulate by calling a library routine */
                if ( rcmemu ) {
                    sprintf(buf,"rcmx_%s",opcode->name);

                    if ( (sym = find_symbol(buf)) == NULL ) {
                        sym = new_import(buf);
                    }
                    db = new_dblock();

                    size = db->size = 3;
                    db->data = mymalloc(db->size);
                    d = (unsigned char *)db->data;
                    *d++ = 205; /* Call */

                    /* Create a temporary expression */
                    expr = parse_expr(&bufptr);
                    if ( eval_expr(expr, &val, sec, pc) == 0 ) {
                        if (find_base(expr, &base, sec, pc) == BASE_OK)
                            add_extnreloc(&db->relocs,base, val, REL_ABS,
                                          0, 8, 1);
                        else
                            general_error(38);  /* illegal relocation */
                    } 
                    *d++ = val % 256;
                    *d++ = val / 256;

                    free_expr(expr);
                    return db;
                } else {
                    cpu_error(13, cpuname, opcode->name);
                    error = 1;
                }
            }
            }
        }
    }


    /* Check altd/ioi on non-rabbit cpus */
    if ( (cpu_type & CPU_RABBIT) == 0 ) {
        if  ( ip->ext.altd ) {
            cpu_error(14,"altd");
            error = 1;
        }
        if  ( ip->ext.ioi ) {
            cpu_error(14,"ioi");
            error = 1;
        }
        if  ( ip->ext.ioe ) {
            cpu_error(14,"ioe");
            error = 1;
        }
    } else {
        if ( ip->ext.ioe && ip->ext.ioi ) {
            cpu_error(15);
            error = 1;
        }
        if ( ip->ext.altd ) {
            if ( (opcode->ext.modifier_flags & F_ALTDW) == F_ALTDW ) {
                /* Warning, altd not needed */
                cpu_error(17, "altd", opcode->name);
            } else if ( (opcode->ext.modifier_flags & F_ALTD) == 0 ) {
                /* Error, altd not supported */
                cpu_error(16, "altd", opcode->name);
                error = 1;
            }
            if ( (opcode->ext.modifier_flags & F_ALTDWHL ) ) {
                /* Check for (hl) usage and indicate it has no effect */
                if ( ((opcode->operand_type[0] & OP_RALT) && (ip->op[0]->reg & REG_PLAIN) == REG_HLREF) ||
                      ((opcode->operand_type[1] & OP_RALT) && (ip->op[1]->reg & REG_PLAIN) == REG_HLREF) ) {
                    /* Warning, altd not needed */
                    cpu_error(18, "altd", opcode->name);
                }
            }
            if ( (ip->op[0] && (ip->op[0]->reg == (REG_HL|REG_IX) || ip->op[0]->reg == (REG_HL|REG_IY))) ) {
                cpu_error(12);
            }

        }
        if ( ip->ext.ioe && (opcode->ext.modifier_flags & F_IO) == 0 ) {
            /* If (hl) or (ix+) (iy+) then we shouldn't warn */
            if ( is_offset_operand(ip->op[0]) == 0 && is_offset_operand(ip->op[1]) == 0 ) {
                cpu_error(18, "ioe", opcode->name);
            }
        }
        if ( ip->ext.ioi && (opcode->ext.modifier_flags & F_IO) == 0 ) {
            if ( is_offset_operand(ip->op[0]) == 0 && is_offset_operand(ip->op[1]) == 0 ) {
                cpu_error(18, "ioi", opcode->name);
            }
        }

    }

    if (((cpu_type & CPU_80OS) == CPU_80OS) && 
        (opcode->ext.modifier_flags & F_80OS_CHECK) == F_80OS_CHECK) {
        char *warn[2];
        switch (opcode->ext.opcode) {
        case 0xc3:
        case 0xf2:
            opcode->ext.opcode = 0xf2;
            warn[0] =  "jp";
            warn[1] =  "jump when positive";
            break;
        case 0xfe:
        case 0xf4:
            opcode->ext.opcode = 0xf4;
            warn[0] =  "cp";
            warn[1] =  "call when positive";
            break;
        default:
            warn[0] = NULL;
            break;
        }
        if (((cpu_type & CPU_8080) == CPU_8080) && warn[0])
            cpu_error(26, warn[0], warn[1]);
    }

    /* Check for bad register usage */
    if ( (cpu_type & (CPU_RABBIT|CPU_Z180)) ) {
        /* Rabbit can't use index registers by themselves */
        if ( (ip->op[0] && ip->op[0]->type == ( OP_REG8|OP_INDEX ) ) || 
             (ip->op[1] && ip->op[1]->type == ( OP_REG8|OP_INDEX ) ) ) {
            cpu_error(cpu_type & CPU_RABBIT ? 10 : 11);
            error = 1;
        }
    }
    if ( (cpu_type & (CPU_GB80|CPU_8080)) ) {
        /* 8080 can't use index registers at all */
        if ( (ip->op[0] && ip->op[0]->type & ( OP_INDEX ) ) || 
             (ip->op[1] && ip->op[1]->type & ( OP_INDEX ) ) ) {
            cpu_error(2, cpuname);
            error = 1;
        }
    }


    db = new_dblock();

    db->size = size;
    db->data = mymalloc(db->size); /* Ed prefix ones are larger in anycase */
    d = (unsigned char *)db->data;

    offs = 0;
    switch ( opcode->ext.mode ) {
    case TYPE_ARITH8:
        offs = ip->op[0]->reg & REG_PLAIN;
        if ( ip->op[1] && ip->op[0]->reg != OP_A ) {
            offs = ip->op[1]->reg & REG_PLAIN;
        }
        break;
    case TYPE_MISC8:
        if ( BASIC_TYPE(opcode->operand_type[0]) == OP_REG8 ) {
            offs = (ip->op[0]->reg & REG_PLAIN) * 8;
        } else if ( BASIC_TYPE(opcode->operand_type[1]) == OP_REG8 ) {
            offs = (ip->op[1]->reg & REG_PLAIN) * 8; 
        }
        break;
    case TYPE_LD8:
        /* ix/iy couples forbidden */
        if ( (ip->op[0]->reg & (REG_IX |REG_IY)) &&
             (ip->op[1]->reg & (REG_IX|REG_IY)) &&
             (((ip->op[0]->reg & REG_IX) && (ip->op[1]->reg & REG_IY)) ||
              ((ip->op[1]->reg & REG_IX) && (ip->op[0]->reg & REG_IY)) )){
            cpu_error(23,opcode->name);
        }
        /* forbid ld ixl, (ix+0) */
        if ( (ip->op[0]->reg & (REG_IX |REG_IY)) &&
             (ip->op[1]->reg & (REG_IX|REG_IY)) &&
             (ip->op[1]->type & OP_OFFSET)
           ){
            cpu_error(23,opcode->name);
        }
        /* forbid ld ixl,(hl) or similar expressions */
        if ( (ip->op[0]->reg & (REG_IX|REG_IY)) &&
             (ip->op[1]->reg & REG_PLAIN) == REG_HLREF) {
            cpu_error(24,opcode->name);
        }
        /* forbid ld ixh/l,h/l and ld iyh/l,h/l */
        if ( ( (ip->op[0]->reg & (REG_IX|REG_IY)) &&
               !(ip->op[0]->reg & REG_INDEX) ) &&
             ( !(ip->op[1]->reg & (REG_IX|REG_IY)) &&
               ( ((ip->op[1]->reg & REG_PLAIN) == REG_H) ||
                 ((ip->op[1]->reg & REG_PLAIN) == REG_L) ) )
           ) {
            cpu_error(24,opcode->name);
        }
        /* forbid ld h/l,ixh/l and ld h/l,iyh/l */
        if ( ( (ip->op[1]->reg & (REG_IX|REG_IY)) &&
               !(ip->op[1]->reg & REG_INDEX) ) &&
             ( !(ip->op[0]->reg & (REG_IX|REG_IY)) &&
               ( ((ip->op[0]->reg & REG_PLAIN) == REG_H) ||
                 ((ip->op[0]->reg & REG_PLAIN) == REG_L) ) )
           ) {
            cpu_error(24,opcode->name);
        }               
        offs =  ((ip->op[0]->reg & REG_PLAIN) * 8) + ( ip->op[1]->reg & REG_PLAIN);
        break;
    case TYPE_ARITH16:
        /* Forbid instructions of type ld (hl), (memory) */
        if ( opcode->operand_type[1] && (opcode->operand_type[1] & (OP_ADDR)) &&
             opcode->operand_type[0] && (OP_INDIR) &&
             (ip->op[0]->reg & REG_PLAIN) == REG_HL) {
            cpu_error(25);
        }
        /* Forbid instructions of type ld (memory), (hl) */
        if (BASIC_TYPE(ip->op[0]->type) == OP_ABS16 && 
             BASIC_TYPE(ip->op[1]->type) == OP_HL) {
            cpu_error(25);
        }
        if ( opcode->operand_type[1] && (opcode->operand_type[1] & ( OP_ARITH16)) ) {
            offs = (ip->op[1]->reg & REG_PLAIN) * 16;
            if ( (ip->op[0]->reg & REG_PLAIN) == REG_HL && (ip->op[1]->reg & REG_PLAIN) == REG_HL &&
                (ip->op[0]->reg & (REG_IX|REG_IY)) != (ip->op[1]->reg & (REG_IX|REG_IY)) ) {
                cpu_error(21,opcode->name);
            }
        } else {
            if ( (ip->op[0]->reg & REG_PLAIN) == REG_AF ) {
                offs = 3 * 16;
            } else {
                offs = (ip->op[0]->reg & REG_PLAIN) * 16;
            }
        }
        /* reg = ip->op[0]->reg & (REG_IX | REG_IY); not needed? */
        break;
    case TYPE_IDX32:
        offs = 0;
        if ( BASIC_TYPE(opcode->operand_type[1]) == OP_IDX32 ) {
            offs = 16 * ( ip->op[1]->reg - REG_PW );
        } else {
            offs = 16 * ( ip->op[0]->reg - REG_PW );
        }
        break;
    case TYPE_IDX32R:
        if ( BASIC_TYPE(opcode->operand_type[1]) == OP_IDX32 ) {
            offs = 16 * ( ip->op[1]->reg - REG_PW );
        }
        if ( BASIC_TYPE(opcode->operand_type[0]) == OP_IDX32 ) {
            offs += 64 * ( ip->op[0]->reg - REG_PW );
        }
        break;
    case TYPE_FLAGS:
    case TYPE_RELJUMP:
        if ( opcode->operand_type[0] == OP_FLAGS || opcode->operand_type[0] == OP_FLAGS_RCM ) {
           
            offs = ip->op[0]->flags * 8;

            if ( ( cpu_type & CPU_GB80 ) && ip->op[0]->flags >= FLAGS_PO ) {
                /* GB80 doesn't support flags po, p, m, pe */
                cpu_error(8, opcode->name);
                error = 1;
            } else if (  opcode->ext.opcode == 0xc4 && (cpu_type & CPU_RABBIT ) ) {
                /* If this is a call then we need to do special stuff for the rabbit */
                if ( rcmemu ) {
                    rabbit_emu_call(ip,db,sec,pc);
                    return db;
                } else {
                    cpu_error(13, cpuname, opcode->name);
                    error = 1;
                }
            } else if ( strcmp(opcode->name,"jr") == 0 || strcmp(opcode->name,"jre") == 0 ) {
                if ( ip->op[0]->flags >= FLAGS_PO ) {
                    cpu_error(8, opcode->name);
                    error = 1;
                }
            }
        }
        break;
    case TYPE_NONE:
    case TYPE_EDPREF:
    case TYPE_NOPREFIX:
        break;
    case TYPE_IPSET:
        if ( eval_expr(ip->op[0]->value, &val, sec, pc) == 0 ) {
            cpu_error(19, opcode->name);
            error = 1;
        } else  if ( val >= 0 && val <= 3) {  /* ipset has to be between 0 and 3 */
            if ( val <= 1 ) {
                offs = val * 16;
            } else {
                offs = (val - 2) * 16 + 8;
            }
        } else {
            cpu_error(6,opcode->name,val);  /* ipset out of range */
            error = 1 ;
        }
        break;
    case TYPE_BIT:
        if ( eval_expr(ip->op[0]->value, &val, sec, pc) == 0 ) {
            cpu_error(19, opcode->name);
            error = 1;
        } else  if ( val >= 0 && val <= 7) {  /* Bit has to be between 0 and 7 */
            offs = val * 8 +  (ip->op[1]->reg & REG_PLAIN);
        } else {
            cpu_error(4,val);  /* Bit count out of range */
            error = 1;
        }
        break;
    case TYPE_IM:
        if ( eval_expr(ip->op[0]->value, &val, sec, pc) == 0 ) {
            cpu_error(19, opcode->name);
            error = 1;
        } else  if ( val >=0 && val <= 2) {
            offs = val * 16;
            if ( val == 2 ) { /* More opcode placement oddness */
                offs = 24;
            }
        } else {
            cpu_error(6,opcode->name,val);  /* im out of range */
            error = 1;
        }
        break;
    case TYPE_OUT_C_0:
        if ( eval_expr(ip->op[1]->value, &val, sec, pc) == 0 ) {
            cpu_error(19, opcode->name);
            error = 1;
        } else if (val != 0){
            cpu_error(22,opcode->name);
            error = 1;
        }
        break;
    case TYPE_RST:
        if ( eval_expr(ip->op[0]->value, &val, sec, pc) == 0 ) {
            cpu_error(19, opcode->name);
            error = 1;
        } else if ( (val & ~0x38) == 0 && cpu_type != CPU_80OS) {
            if ( cpu_type == CPU_RCM2000 ) {
                /* Check for valid rst on Rabbit */
                if ( val == 0 ||  val == 8 || val == 0x30 ) {
                    cpu_error(9, val); /* Invalid restart */
                    error = 1;
                }
            }
            offs = val;
        } else if ( (val & 0xf8) == 0 && cpu_type == CPU_80OS) {
            offs = val << 3;
        } else {
            cpu_error(5, val, val);  /* rst out of range */
            error = 1;
        }
        break;
    case TYPE_RP:
        if (ip->op[0]->reg == REG_SP)
            ip->op[0]->reg = REG_A;
        offs = ((ip->op[0]->reg & REG_PLAIN) / 2) * 16;
        break;
    }

    if ( error == 0 ) {
        write_opcode(opcode, db, size, sec, pc, ip->op[0], ip->op[1], offs, &ip->ext);
    }


    return db;
}


operand *new_operand()
{
  operand *new = mymalloc(sizeof(*new));
  new->type = -1;
  new->reg = 0;
  return new;
}


int init_cpu()
{
  current_pc_char = '$';
  return 1;
}


int cpu_args(char *p)
{

    if ( strcmp(p, "-intel-syntax") == 0) {
        cpu_type = (cpu_type & CPU_8080) | CPU_80OS;
        cpuname="8080 Intel Syntax";
        return 1;
    } else if ( strcmp(p, "-8080") == 0 ) {
        cpu_type = (cpu_type & CPU_80OS) | CPU_8080;
        if ((cpu_type & CPU_80OS) == 0)
            cpuname = "8080";
        return 1;
    } else if ( strcmp(p, "-rcm2000") == 0 ) {
        cpu_type = CPU_RCM2000;
        cpuname = "Rabbit2000";
        return 1;
    } else if ( strcmp(p, "-rcm3000") == 0 ) {
        cpu_type = CPU_RCM3000;
        cpuname = "Rabbit3000";
        return 1;
    } else if ( strcmp(p, "-rcm4000") == 0 ) {
        cpu_type = CPU_RCM4000;
        cpuname = "Rabbit4000";
        return 1;
    } else if ( strcmp(p, "-hd64180") == 0 ) {
        cpu_type = CPU_Z180;
        cpuname = "hd64180";
        return 1;
    } else if ( strcmp(p, "-gbz80") == 0 ) {
        cpu_type = CPU_GB80;
        cpuname = "gbz80";
        return 1;
    } else if ( strcmp(p, "-swapixiy") == 0 ) {
        swapixiy = 1;
        return 1;
    } else if ( strcmp(p, "-rcmemu" ) == 0 ) {
        rcmemu = 1;
        return 1;
    } else if ( strcmp(p, "-z80asm" ) == 0 ) {
        z80asm_compat = 1;
        return 1;
    }
    return 0;
}
