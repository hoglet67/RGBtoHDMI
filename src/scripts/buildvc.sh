#!/bin/bash
../../tools/vasm/vasmvidcore_std  -Fbin -L ../videocore.lst -o ../videocore.asm ../videocore.s
rm -f ../videocore.c
xxd -i ../videocore.asm >> ../videocore.c
